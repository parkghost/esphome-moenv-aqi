#include "moenv_aqi.h"

#include <ArduinoJson.h>
#include <esp_random.h>

#include <algorithm>
#include <array>
#include <ctime>
#include <memory>

#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

namespace esphome {
namespace moenv_aqi {

static constexpr size_t MAX_RECORDS_CHECKED = 500;
static constexpr size_t URL_BASE_RESERVE_SIZE = 256;
static constexpr size_t URL_OFFSET_RESERVE_SIZE = 20;
uint32_t global_moenv_aqi_id = 1911044085ULL;

// Setup priority
float MoenvAQI::get_setup_priority() const { return setup_priority::LATE; }

// Component setup
void MoenvAQI::setup() {
  const auto object_id = str_sanitize(str_snake_case(App.get_friendly_name()) + std::to_string(global_moenv_aqi_id));
  auto object_id_hash_ = fnv1_hash(object_id);
  ESP_LOGV(TAG, "Object ID: %s, hash: %u", object_id.c_str(), object_id_hash_);
  this->pref_ = global_preferences->make_preference<size_t>(object_id_hash_);
  if (this->pref_.load(&last_successful_offset_)) {
    ESP_LOGD(TAG, "Loaded last_successful_offset_: %u", last_successful_offset_);
  }
  global_moenv_aqi_id++;
}

// Reset site data
void MoenvAQI::reset_site_data_() {
  ESP_LOGD(TAG, "Site name changed, resetting data and offsets");
  last_successful_offset_ = 0;
  data_ = Record();
  if (this->publish_time_) this->publish_time_->publish_state("");
  if (this->site_id_) this->site_id_->publish_state(this->data_.site_id);
  if (this->longitude_) this->longitude_->publish_state(this->data_.longitude);
  if (this->latitude_) this->latitude_->publish_state(this->data_.latitude);
  if (this->current_site_name_) this->current_site_name_->publish_state(this->data_.site_name);
  if (this->county_) this->county_->publish_state(this->data_.county);
}

// Periodic update
void MoenvAQI::update() {
  if (!validate_config_()) {
    ESP_LOGE(TAG, "Configuration validation failed");
    return;
  }

  if (limit_.value() != last_limit_ && last_limit_ != 0) {
    ESP_LOGD(TAG, "Limit changed, resetting last_successful_offset_");
    last_successful_offset_ = 0;
  }

  if (last_site_name_ != "" && site_name_.value() != last_site_name_) {
    reset_site_data_();
  }

  // Cancel any pending retry before starting fresh
  this->cancel_timeout("moenv_retry");
  this->retry_in_progress_ = false;

  this->try_send_request_(0);
}

// Debug configuration
void MoenvAQI::dump_config() {
  ESP_LOGCONFIG(TAG, "MOENV AQI:");
  ESP_LOGCONFIG(TAG, "  API Key: %s", api_key_.value().empty() ? "not set" : "set");
  ESP_LOGCONFIG(TAG, "  Site Name: %s", site_name_.value().c_str());
  ESP_LOGCONFIG(TAG, "  Language: %s", language_.value().c_str());
  ESP_LOGCONFIG(TAG, "  Limit: %u", limit_.value());
  ESP_LOGCONFIG(TAG, "  Sensor Expired: %u minutes", sensor_expiry_.value() / 1000 / 60);
  ESP_LOGCONFIG(TAG, "  Retry Count: %u", retry_count_.value());
  ESP_LOGCONFIG(TAG, "  Retry Delay: %u ms", retry_delay_.value());
  LOG_UPDATE_INTERVAL(this);
}

// Validate configuration
bool MoenvAQI::validate_config_() {
  bool valid = true;
  if (api_key_.value().empty()) {
    ESP_LOGE(TAG, "API Key not set");
    valid = false;
  }

  if (site_name_.value().empty()) {
    ESP_LOGE(TAG, "Site Name not set");
    valid = false;
  }

  if (language_.value().empty()) {
    ESP_LOGE(TAG, "Language not set");
    valid = false;
  }

  if (limit_.value() == 0) {
    ESP_LOGE(TAG, "Limit must be greater than 0");
    valid = false;
  }

  return valid;
}

// Send HTTP request
bool MoenvAQI::send_request_() {
  if (!this->rtc_->now().is_valid()) {
    ESP_LOGW(TAG, "RTC is not valid");
    return false;
  }

  if (!network::is_connected()) {
    ESP_LOGW(TAG, "Network not connected");
    return false;
  }

  const size_t start_offset = last_successful_offset_;
  size_t offset = start_offset;
  size_t limit = limit_.value();
  bool wrapped = false;

  std::string limit_parm;
  limit_parm.reserve(32);
  if (limit > 0) {
    limit_parm = "&limit=";
    limit_parm += std::to_string(limit);
  }

  bool found = false;
  Record record{};
  int records_count = 0;

  std::string url_base;
  url_base.reserve(URL_BASE_RESERVE_SIZE);
  url_base = "https://data.moenv.gov.tw/api/v2/aqx_p_432?language=";
  url_base += language_.value();
  url_base += "&api_key=";
  url_base += api_key_.value();
  url_base += limit_parm;

  size_t total_checked = 0;
  while (!found) {
    if (total_checked >= MAX_RECORDS_CHECKED) {
      ESP_LOGW(TAG, "Safeguard: checked over %u records, aborting search.", MAX_RECORDS_CHECKED);
      break;
    }
    total_checked += limit;
    std::string url;
    url.reserve(url_base.length() + URL_OFFSET_RESERVE_SIZE);
    url = url_base;
    url += "&offset=";
    url += std::to_string(offset);

    ESP_LOGD(TAG, "Sending query: %s", url.c_str());
    ESP_LOGD(TAG, "Before request: free heap:%u, max block:%u",
             esp_get_free_heap_size(),
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
    App.feed_wdt();

    auto container = this->http_request_->get(url);

    ESP_LOGD(TAG, "After request: free heap:%u, max block:%u",
             esp_get_free_heap_size(),
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));

    if (container == nullptr) {
      ESP_LOGE(TAG, "HTTP request failed: no response container");
      return false;
    }

    if (container->status_code != 200) {
      ESP_LOGE(TAG, "HTTP request failed with code: %d", container->status_code);
      container->end();
      return false;
    }

    App.feed_wdt();
    ESP_LOGD(TAG, "Looking for site: %s", site_name_.value().c_str());

    HttpStreamAdapter stream(container, 1024, this->http_request_->get_timeout());
    bool result = process_response_(stream, record, records_count);
    ESP_LOGD(TAG, "Processed %zu bytes, records_count: %d", stream.getBytesRead(), records_count);

    container->end();

    ESP_LOGD(TAG, "After json parse: free heap:%u, max block:%u",
             esp_get_free_heap_size(),
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));

    if (result) {
      found = true;
      this->last_successful_offset_ = offset;
      if (check_changes_(record)) {
        this->data_ = record;
        if (validate_record_()) {
          ESP_LOGD(TAG, "Triggering on_data_change automation.");
          this->on_data_change_trigger_.trigger(this->data_);
        } else {
          ESP_LOGW(TAG, "Record validation failed.");
          return false;
        }
      } else {
        ESP_LOGD(TAG, "Data has not changed since last update.");
      }
    } else {
      ESP_LOGD(TAG, "Site '%s' not found at offset %u (records_count: %d, limit: %u)",
               site_name_.value().c_str(), offset, records_count, limit);

      if (records_count == 0 || records_count < (int)limit) {
        if (wrapped) {
          ESP_LOGW(TAG, "Site '%s' not found after full scan", site_name_.value().c_str());
          break;
        }
        ESP_LOGD(TAG, "Reached end of data, wrapping around to offset 0");
        offset = 0;
        wrapped = true;
      } else {
        offset += limit;
      }

      if (wrapped && offset >= start_offset && start_offset > 0) {
        ESP_LOGW(TAG, "Completed wrap-around scan, site '%s' not found", site_name_.value().c_str());
        break;
      }
    }
  }
  return found;
}

// Non-blocking retry with exponential backoff
void MoenvAQI::try_send_request_(uint32_t attempt) {
  if (this->send_request_()) {
    this->retry_in_progress_ = false;
    this->status_clear_warning();

    this->last_site_name_ = site_name_.value();
    this->last_limit_ = limit_.value();
    if (this->last_success_) {
      auto now = this->rtc_->now();
      if (now.is_valid()) {
        this->last_success_->publish_state(now.strftime("%Y-%m-%d %H:%M:%S"));
      }
    }

    ESP_LOGD(TAG, "Saving last_successful_offset_: %u", this->last_successful_offset_);
    this->pref_.save(&this->last_successful_offset_);
    this->publish_states_();
    return;
  }

  uint32_t retry_count = retry_count_.value();
  if (attempt < retry_count) {
    uint32_t retry_delay = retry_delay_.value();
    uint32_t backoff_delay = retry_delay * (1 << attempt);
    uint32_t jitter = esp_random() % 1000;
    uint32_t total_delay = std::min(backoff_delay + jitter, static_cast<uint32_t>(30000));

    ESP_LOGW(TAG, "Request failed (attempt %u/%u), retrying in %u ms",
             attempt + 1, retry_count + 1, total_delay);

    this->retry_in_progress_ = true;
    this->set_timeout("moenv_retry", total_delay, [this, next = attempt + 1]() {
      this->try_send_request_(next);
    });
    return;
  }

  // Final failure
  this->retry_in_progress_ = false;
  this->last_successful_offset_ = 0;
  this->status_set_warning();
  this->on_error_trigger_.trigger();

  if (this->last_error_) {
    auto now = this->rtc_->now();
    if (now.is_valid()) {
      this->last_error_->publish_state(now.strftime("%Y-%m-%d %H:%M:%S"));
    }
  }

  ESP_LOGE(TAG, "Request failed after %u attempts", retry_count_.value() + 1);
  ESP_LOGD(TAG, "Saving last_successful_offset_: %u", this->last_successful_offset_);
  this->pref_.save(&this->last_successful_offset_);
  this->publish_states_();
}

// Process HTTP response
bool MoenvAQI::process_response_(HttpStreamAdapter &stream, Record &record, int &records_count) {
  records_count = 0;

  if (!stream.find("[")) {
    ESP_LOGE(TAG, "Could not find array start '['");
    return false;
  }

  const std::string target_site_name = site_name_.value();

  JsonDocument doc;

  // Iterate through each record in the array
  bool found = false;
  do {
    App.feed_wdt();
    DeserializationError error = deserializeJson(doc, stream);
    if (error) {
      ESP_LOGE(TAG, "deserializeJson() failed: %s", error.c_str());
      continue;  // Skip to the next record
    }

    records_count++;  // Count successfully parsed records

    // Extract the sitename
    JsonVariant sitename_json = doc["sitename"];
    if (!sitename_json) {
      ESP_LOGW(TAG, "Could not find 'sitename' field, skipping record");
      continue;
    }
    if (sitename_json.isNull()) {
      ESP_LOGW(TAG, "'sitename' field is null, skipping record");
      continue;
    }

    const std::string sitename = sitename_json.as<std::string>();
    ESP_LOGV(TAG, "sitename: %s", sitename.c_str());

    // Check if this is the target site
    if (sitename == target_site_name) {
      ESP_LOGD(TAG, "Found target site: %s", target_site_name.c_str());

      static const std::array mappings{
          FieldMapping{FIELD_SITENAME, true, [](Record &r, JsonVariant &v) { r.site_name = v.as<std::string>(); }},
          FieldMapping{FIELD_COUNTY, false, [](Record &r, JsonVariant &v) { r.county = v.as<std::string>(); }},
          FieldMapping{FIELD_AQI, true, [](Record &r, JsonVariant &v) { r.aqi = v.as<int>(); }},
          FieldMapping{FIELD_POLLUTANT, false, [](Record &r, JsonVariant &v) { r.pollutant = v.as<std::string>(); }},
          FieldMapping{FIELD_STATUS, false, [](Record &r, JsonVariant &v) { r.status = v.as<std::string>(); }},
          FieldMapping{FIELD_SO2, false, [](Record &r, JsonVariant &v) { r.so2 = v.as<float>(); }},
          FieldMapping{FIELD_CO, false, [](Record &r, JsonVariant &v) { r.co = v.as<float>(); }},
          FieldMapping{FIELD_O3, false, [](Record &r, JsonVariant &v) { r.o3 = v.as<int>(); }},
          FieldMapping{FIELD_O3_8HR, false, [](Record &r, JsonVariant &v) { r.o3_8hr = v.as<int>(); }},
          FieldMapping{FIELD_PM10, false, [](Record &r, JsonVariant &v) { r.pm10 = v.as<int>(); }},
          FieldMapping{FIELD_PM25, false, [](Record &r, JsonVariant &v) { r.pm2_5 = v.as<int>(); }},
          FieldMapping{FIELD_NO2, false, [](Record &r, JsonVariant &v) { r.no2 = v.as<int>(); }},
          FieldMapping{FIELD_NOX, false, [](Record &r, JsonVariant &v) { r.nox = v.as<int>(); }},
          FieldMapping{FIELD_NO, false, [](Record &r, JsonVariant &v) { r.no = v.as<float>(); }},
          FieldMapping{FIELD_WIND_SPEED, false, [](Record &r, JsonVariant &v) { r.wind_speed = v.as<float>(); }},
          FieldMapping{FIELD_WIND_DIREC, false, [](Record &r, JsonVariant &v) { r.wind_direc = v.as<int>(); }},
          FieldMapping{FIELD_PUBLISH_TIME, true, [](Record &r, JsonVariant &v) { r.publish_time = v.as<std::string>(); }},
          FieldMapping{FIELD_CO_8HR, false, [](Record &r, JsonVariant &v) { r.co_8hr = v.as<float>(); }},
          FieldMapping{FIELD_PM25_AVG, false, [](Record &r, JsonVariant &v) { r.pm2_5_avg = v.as<float>(); }},
          FieldMapping{FIELD_PM10_AVG, false, [](Record &r, JsonVariant &v) { r.pm10_avg = v.as<int>(); }},
          FieldMapping{FIELD_SO2_AVG, false, [](Record &r, JsonVariant &v) { r.so2_avg = v.as<float>(); }},
          FieldMapping{FIELD_LONGITUDE, false, [](Record &r, JsonVariant &v) { r.longitude = v.as<double>(); }},
          FieldMapping{FIELD_LATITUDE, false, [](Record &r, JsonVariant &v) { r.latitude = v.as<double>(); }},
          FieldMapping{FIELD_SITEID, false, [](Record &r, JsonVariant &v) { r.site_id = v.as<int>(); }},
      };

      for (const auto &m : mappings) {
        JsonVariant val = doc[m.key];
        if (val.isNull()) {
          if (m.required) {
            ESP_LOGE(TAG, "Required field '%s' missing or null, record invalid", m.key.data());
            return false;
          }
          continue;
        }
        m.setter(record, val);
      }

      if (record.aqi < 0 || record.aqi > 500) {
        ESP_LOGE(TAG, "Invalid AQI value: %d", record.aqi);
        return false;
      }
      if (record.latitude < -90.0 || record.latitude > 90.0 || record.longitude < -180.0 || record.longitude > 180.0) {
        ESP_LOGE(TAG, "Invalid coordinates: lat=%.6f lon=%.6f", record.latitude, record.longitude);
        return false;
      }
      return true;
    }
  } while (!found && stream.findUntil(",", "]"));
  return found;
}

// Compare new data with stored data; return true if they differ
bool MoenvAQI::check_changes_(const Record &new_data) { return !(this->data_ == new_data); }

// Validate the record based on the current time and valid duration
bool MoenvAQI::validate_record_() { return this->data_.validate(this->rtc_->now(), this->sensor_expiry_.value() / 1000 / 60); }

// Publish all sensor and text sensor states
void MoenvAQI::publish_states_() {
  if (this->last_updated_) {
    ESPTime now = this->rtc_->now();
    if (now.is_valid()) {
      this->last_updated_->publish_state(now.strftime("%Y-%m-%d %H:%M:%S"));
    }
  }

  const bool valid = validate_record_();

  auto publish = [valid](sensor::Sensor *s, float value) {
    if (s) s->publish_state(valid ? value : NAN);
  };

  publish(this->aqi_, this->data_.aqi);
  publish(this->so2_, this->data_.so2);
  publish(this->co_, this->data_.co);
  publish(this->no_, this->data_.no);
  publish(this->wind_speed_, this->data_.wind_speed);
  publish(this->co_8hr_, this->data_.co_8hr);
  publish(this->pm2_5_avg_, this->data_.pm2_5_avg);
  publish(this->so2_avg_, this->data_.so2_avg);
  publish(this->o3_, this->data_.o3);
  publish(this->o3_8hr_, this->data_.o3_8hr);
  publish(this->pm10_, this->data_.pm10);
  publish(this->pm2_5_, this->data_.pm2_5);
  publish(this->no2_, this->data_.no2);
  publish(this->nox_, this->data_.nox);
  publish(this->wind_direc_, this->data_.wind_direc);
  publish(this->pm10_avg_, this->data_.pm10_avg);

  if (this->pollutant_) this->pollutant_->publish_state(valid ? this->data_.pollutant : "");
  if (this->status_) this->status_->publish_state(valid ? this->data_.status : "");

  if (valid) {
    if (this->publish_time_) this->publish_time_->publish_state(this->data_.publish_time);
    if (this->site_id_) this->site_id_->publish_state(this->data_.site_id);
    if (this->longitude_) this->longitude_->publish_state(this->data_.longitude);
    if (this->latitude_) this->latitude_->publish_state(this->data_.latitude);
    if (this->current_site_name_) this->current_site_name_->publish_state(this->data_.site_name);
    if (this->county_) this->county_->publish_state(this->data_.county);
  }
}

}  // namespace moenv_aqi
}  // namespace esphome
