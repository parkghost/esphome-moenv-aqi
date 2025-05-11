#include "moenv_aqi.h"

#include <HTTPClient.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <functional>
#include <set>

#include "esphome/components/watchdog/watchdog.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

namespace esphome {
namespace moenv_aqi {

static const size_t MAX_RECORDS_CHECKED = 500;
uint32_t global_moenv_aqi_id = 1911044085ULL;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

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

  if (send_request_()) {
    this->status_clear_warning();

    this->last_site_name_ = site_name_.value();
    this->last_limit_ = limit_.value();
    if (this->last_success_) {
      auto now = this->rtc_->now();
      if (now.is_valid()) {
        this->last_success_->publish_state(now.strftime("%Y-%m-%d %H:%M:%S"));
      }
    }
  } else {
    this->last_successful_offset_ = 0;
    this->status_set_warning();
    this->on_error_trigger_.trigger();

    if (this->last_error_) {
      auto now = this->rtc_->now();
      if (now.is_valid()) {
        this->last_error_->publish_state(now.strftime("%Y-%m-%d %H:%M:%S"));
      }
    }
  }

  ESP_LOGD(TAG, "Saving last_successful_offset_: %u", this->last_successful_offset_);
  this->pref_.save(&this->last_successful_offset_);

  this->publish_states_();
}

// Debug configuration
void MoenvAQI::dump_config() {
  ESP_LOGCONFIG(TAG, "MOENV AQI:");
  ESP_LOGCONFIG(TAG, "  API Key: %s", api_key_.value().empty() ? "not set" : "set");
  ESP_LOGCONFIG(TAG, "  Site Name: %s", site_name_.value().c_str());
  ESP_LOGCONFIG(TAG, "  Language: %s", language_.value().c_str());
  ESP_LOGCONFIG(TAG, "  Limit: %u", limit_.value());
  ESP_LOGCONFIG(TAG, "  Sensor Expired: %u minutes", sensor_expiry_.value() / 1000 / 60);
  ESP_LOGCONFIG(TAG, "  Watchdog Timeout: %u ms", watchdog_timeout_.value());
  ESP_LOGCONFIG(TAG, "  HTTP Connect Timeout: %u ms", http_connect_timeout_.value());
  ESP_LOGCONFIG(TAG, "  HTTP Timeout: %u ms", http_timeout_.value());
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

  if (sensor_expiry_.value() < 0) {
    ESP_LOGE(TAG, "Sensor Expiry must be greater or equal to 0");
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

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    ESP_LOGW(TAG, "WiFi not connected");
    return false;
  }

  watchdog::WatchdogManager wdm(this->watchdog_timeout_.value());

  size_t offset = last_successful_offset_;  // Start from the last known offset
  size_t limit = limit_.value();

  std::string limit_parm;
  if (limit > 0) {
    limit_parm = "&limit=" + std::to_string(limit);
  }

  bool found = false;
  Record record;
  int total = -1;

  std::string url_base = "https://data.moenv.gov.tw/api/v2/aqx_p_432?language=" + language_.value() + "&api_key=" + api_key_.value() + limit_parm;

  HTTPClient http;
  http.useHTTP10(true);
  http.setConnectTimeout(http_connect_timeout_.value());
  http.setTimeout(http_timeout_.value());
  http.addHeader("Content-Type", "application/json");

  size_t total_checked = 0;  // safeguard counter
  while (!found) {
    if (total_checked >= MAX_RECORDS_CHECKED) {
      ESP_LOGW(TAG, "Safeguard: checked over %u records, aborting search.", MAX_RECORDS_CHECKED);
      break;
    }
    total_checked += limit;
    std::string url = url_base + "&offset=" + std::to_string(offset);

    http.begin(url.c_str());
    ESP_LOGD(TAG, "Sending query: %s", url.c_str());
    ESP_LOGD(TAG, "Before request: free heap:%u, max block:%u", ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
    App.feed_wdt();
    int http_code = http.GET();
    ESP_LOGD(TAG, "After request: free heap:%u, max block:%u", ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));

    // Process successful HTTP response
    if (http_code == HTTP_CODE_OK) {
      App.feed_wdt();
      ESP_LOGD(TAG, "Looking for site: %s", site_name_.value().c_str());
      bool result = process_response_(http.getStream(), record, total);
      http.end();
      ESP_LOGD(TAG, "After json parse: free heap:%u, max block:%u", ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));

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
        if (total == -1) {
          return false;
        }

        ESP_LOGW(TAG, "No matching record found for site_name: %s, offset: %u, limit: %u, try next page", site_name_.value().c_str(), offset, limit);
        offset += limit;
      }
    } else {
      ESP_LOGE(TAG, "HTTP request failed, code: %d, error: %s", http_code, http.getString().c_str());
      return false;
    }

    if (total != -1 && offset >= (size_t)total) {
      ESP_LOGW(TAG, "Exceeded total records, site not found.");
      break;
    }
  }
  http.end();
  return found;
}

// Process HTTP response
bool MoenvAQI::process_response_(Stream &stream, Record &record, int &total) {
  // Get total records
  if (stream.find("\"total\": \"")) {
    char buffer[16];
    int len = stream.readBytesUntil('"', buffer, sizeof(buffer) - 1);
    if (len > 0) {
      buffer[len] = '\0';
      total = atoi(buffer);
      ESP_LOGD(TAG, "Total records: %d", total);
    } else {
      ESP_LOGE(TAG, "Could not read total records");
      total = -1;
      return false;
    }
  } else {
    ESP_LOGE(TAG, "Could not find 'total' field");
    total = -1;
    return false;
  }

  String target_site_name = String(site_name_.value().c_str());

  // Find the "records" array
  if (!stream.find("\"records\": [")) {
    ESP_LOGE(TAG, "Could not find 'records' array");
    return false;
  }

  // check for empty array
  int next = stream.peek();
  if (next == ']') {
    ESP_LOGE(TAG, "Empty records array, skipping");
    return false;
  }

  DynamicJsonDocument doc(1024);

  // Iterate through each record in the array
  bool found = false;
  do {
    App.feed_wdt();
    DeserializationError error = deserializeJson(doc, stream);
    if (error) {
      ESP_LOGE(TAG, "deserializeJson() failed: %s", error.c_str());
      continue;  // Skip to the next record
    }

    // Extract the sitename
    if (!doc.containsKey("sitename")) {
      ESP_LOGW(TAG, "Could not find 'sitename' field, skipping record");
      continue;
    }

    JsonVariant sitename_json = doc["sitename"];
    if (sitename_json.isNull()) {
      ESP_LOGW(TAG, "'sitename' field is null, skipping record");
      continue;
    }

    String sitename = sitename_json.as<String>();
    ESP_LOGV(TAG, "sitename: %s", sitename.c_str());

    // Check if this is the target site
    if (sitename == target_site_name) {
      ESP_LOGD(TAG, "Found target site: %s", target_site_name.c_str());

      // Define schema-driven field mappings
      struct FieldMapping {
        const char *key;
        bool required;
        std::function<void(Record &, JsonVariant &)> setter;
      };
      static const FieldMapping mappings[] = {
          {FIELD_SITENAME, true, [](Record &r, JsonVariant &v) { r.site_name = v.as<String>().c_str(); }},
          {FIELD_COUNTY, false, [](Record &r, JsonVariant &v) { r.county = v.as<String>().c_str(); }},
          {FIELD_AQI, true, [](Record &r, JsonVariant &v) { r.aqi = v.as<int>(); }},
          {FIELD_POLLUTANT, false, [](Record &r, JsonVariant &v) { r.pollutant = v.as<String>().c_str(); }},
          {FIELD_STATUS, false, [](Record &r, JsonVariant &v) { r.status = v.as<String>().c_str(); }},
          {FIELD_SO2, false, [](Record &r, JsonVariant &v) { r.so2 = v.as<float>(); }},
          {FIELD_CO, false, [](Record &r, JsonVariant &v) { r.co = v.as<float>(); }},
          {FIELD_O3, false, [](Record &r, JsonVariant &v) { r.o3 = v.as<int>(); }},
          {FIELD_O3_8HR, false, [](Record &r, JsonVariant &v) { r.o3_8hr = v.as<int>(); }},
          {FIELD_PM10, false, [](Record &r, JsonVariant &v) { r.pm10 = v.as<int>(); }},
          {FIELD_PM25, false, [](Record &r, JsonVariant &v) { r.pm2_5 = v.as<int>(); }},
          {FIELD_NO2, false, [](Record &r, JsonVariant &v) { r.no2 = v.as<int>(); }},
          {FIELD_NOX, false, [](Record &r, JsonVariant &v) { r.nox = v.as<int>(); }},
          {FIELD_NO, false, [](Record &r, JsonVariant &v) { r.no = v.as<float>(); }},
          {FIELD_WIND_SPEED, false, [](Record &r, JsonVariant &v) { r.wind_speed = v.as<float>(); }},
          {FIELD_WIND_DIREC, false, [](Record &r, JsonVariant &v) { r.wind_direc = v.as<int>(); }},
          {FIELD_PUBLISH_TIME, true, [](Record &r, JsonVariant &v) { r.publish_time = v.as<String>().c_str(); }},
          {FIELD_CO_8HR, false, [](Record &r, JsonVariant &v) { r.co_8hr = v.as<float>(); }},
          {FIELD_PM25_AVG, false, [](Record &r, JsonVariant &v) { r.pm2_5_avg = v.as<float>(); }},
          {FIELD_PM10_AVG, false, [](Record &r, JsonVariant &v) { r.pm10_avg = v.as<int>(); }},
          {FIELD_SO2_AVG, false, [](Record &r, JsonVariant &v) { r.so2_avg = v.as<float>(); }},
          {FIELD_LONGITUDE, false, [](Record &r, JsonVariant &v) { r.longitude = v.as<double>(); }},
          {FIELD_LATITUDE, false, [](Record &r, JsonVariant &v) { r.latitude = v.as<double>(); }},
          {FIELD_SITEID, false, [](Record &r, JsonVariant &v) { r.site_id = v.as<int>(); }},
      };

      // Iterate mappings, enforce required, and apply setters
      for (auto &m : mappings) {
        JsonVariant val = doc[m.key];  // cache variant lookup
        if (val.isNull()) {
          if (m.required) {
            ESP_LOGE(TAG, "Required field '%s' missing or null, record invalid", m.key);
            return false;
          }
          continue;
        }
        m.setter(record, val);
      }

      // Additional validation checks
      if (record.aqi < 0) {
        ESP_LOGE(TAG, "Invalid AQI value: %d", record.aqi);
        return false;
      }
      if (record.latitude < -90.0 || record.latitude > 90.0 || record.longitude < -180.0 || record.longitude > 180.0) {
        ESP_LOGE(TAG, "Invalid coordinates: lat=%.6f lon=%.6f", record.latitude, record.longitude);
        return false;
      }
      doc.clear();
      return true;
    }
    doc.clear();
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

  if (validate_record_()) {
    if (this->aqi_) this->aqi_->publish_state(this->data_.aqi);
    if (this->so2_) this->so2_->publish_state(this->data_.so2);
    if (this->co_) this->co_->publish_state(this->data_.co);
    if (this->no_) this->no_->publish_state(this->data_.no);
    if (this->wind_speed_) this->wind_speed_->publish_state(this->data_.wind_speed);
    if (this->co_8hr_) this->co_8hr_->publish_state(this->data_.co_8hr);
    if (this->pm2_5_avg_) this->pm2_5_avg_->publish_state(this->data_.pm2_5_avg);
    if (this->so2_avg_) this->so2_avg_->publish_state(this->data_.so2_avg);
    if (this->o3_) this->o3_->publish_state(this->data_.o3);
    if (this->o3_8hr_) this->o3_8hr_->publish_state(this->data_.o3_8hr);
    if (this->pm10_) this->pm10_->publish_state(this->data_.pm10);
    if (this->pm2_5_) this->pm2_5_->publish_state(this->data_.pm2_5);
    if (this->no2_) this->no2_->publish_state(this->data_.no2);
    if (this->nox_) this->nox_->publish_state(this->data_.nox);
    if (this->wind_direc_) this->wind_direc_->publish_state(this->data_.wind_direc);
    if (this->pm10_avg_) this->pm10_avg_->publish_state(this->data_.pm10_avg);
    if (this->pollutant_) this->pollutant_->publish_state(this->data_.pollutant);
    if (this->status_) this->status_->publish_state(this->data_.status);
    if (this->publish_time_) this->publish_time_->publish_state(this->data_.publish_time);
    if (this->site_id_) this->site_id_->publish_state(this->data_.site_id);
    if (this->longitude_) this->longitude_->publish_state(this->data_.longitude);
    if (this->latitude_) this->latitude_->publish_state(this->data_.latitude);
    if (this->current_site_name_) this->current_site_name_->publish_state(this->data_.site_name);
    if (this->county_) this->county_->publish_state(this->data_.county);
  } else {
    if (this->aqi_) this->aqi_->publish_state(NAN);
    if (this->so2_) this->so2_->publish_state(NAN);
    if (this->co_) this->co_->publish_state(NAN);
    if (this->no_) this->no_->publish_state(NAN);
    if (this->wind_speed_) this->wind_speed_->publish_state(NAN);
    if (this->co_8hr_) this->co_8hr_->publish_state(NAN);
    if (this->pm2_5_avg_) this->pm2_5_avg_->publish_state(NAN);
    if (this->so2_avg_) this->so2_avg_->publish_state(NAN);
    if (this->o3_) this->o3_->publish_state(NAN);
    if (this->o3_8hr_) this->o3_8hr_->publish_state(NAN);
    if (this->pm10_) this->pm10_->publish_state(NAN);
    if (this->pm2_5_) this->pm2_5_->publish_state(NAN);
    if (this->no2_) this->no2_->publish_state(NAN);
    if (this->nox_) this->nox_->publish_state(NAN);
    if (this->wind_direc_) this->wind_direc_->publish_state(NAN);
    if (this->pm10_avg_) this->pm10_avg_->publish_state(NAN);
    if (this->pollutant_) this->pollutant_->publish_state("");
    if (this->status_) this->status_->publish_state("");
  }
}

}  // namespace moenv_aqi
}  // namespace esphome
