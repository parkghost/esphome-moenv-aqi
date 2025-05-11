#pragma once

#include <set>
#include <string>
#include <vector>

#include "esphome.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/time.h"

namespace esphome {
namespace moenv_aqi {

static const char *const TAG = "moenv_aqi";

static const char *const FIELD_SITENAME = "sitename";
static const char *const FIELD_COUNTY = "county";
static const char *const FIELD_AQI = "aqi";
static const char *const FIELD_POLLUTANT = "pollutant";
static const char *const FIELD_STATUS = "status";
static const char *const FIELD_SO2 = "so2";
static const char *const FIELD_CO = "co";
static const char *const FIELD_O3 = "o3";
static const char *const FIELD_O3_8HR = "o3_8hr";
static const char *const FIELD_PM10 = "pm10";
static const char *const FIELD_PM25 = "pm2.5";
static const char *const FIELD_NO2 = "no2";
static const char *const FIELD_NOX = "nox";
static const char *const FIELD_NO = "no";
static const char *const FIELD_WIND_SPEED = "wind_speed";
static const char *const FIELD_WIND_DIREC = "wind_direc";
static const char *const FIELD_PUBLISH_TIME = "publishtime";
static const char *const FIELD_CO_8HR = "co_8hr";
static const char *const FIELD_PM25_AVG = "pm2.5_avg";
static const char *const FIELD_PM10_AVG = "pm10_avg";
static const char *const FIELD_SO2_AVG = "so2_avg";
static const char *const FIELD_LONGITUDE = "longitude";
static const char *const FIELD_LATITUDE = "latitude";
static const char *const FIELD_SITEID = "siteid";

static const int MAX_FUTURE_PUBLISH_TIME_MINUTES = 10;

struct Record {
  std::string site_name;
  std::string county;
  int aqi;
  std::string pollutant;
  std::string status;
  float so2;
  float co;
  int o3;
  int o3_8hr;
  int pm10;
  int pm2_5;
  int no2;
  int nox;
  float no;
  float wind_speed;
  int wind_direc;
  std::string publish_time;
  float co_8hr;
  float pm2_5_avg;
  int pm10_avg;
  float so2_avg;
  double longitude;
  double latitude;
  int site_id;

  bool validate(esphome::ESPTime time, size_t minutes) const {
    if (!time.is_valid()) {
      ESP_LOGW(TAG, "Invalid time");
      return false;
    }

    if (publish_time.empty()) {
      ESP_LOGW(TAG, "Empty publish_time");
      return false;
    }

    struct tm tm{};
    if (strptime(publish_time.c_str(), "%Y/%m/%d %H:%M:%S", &tm) == nullptr) {
      ESP_LOGW(TAG, "Could not parse publish_time: %s", publish_time.c_str());
      return false;
    }

    time_t publish_time_ts = mktime(&tm);
    if (publish_time_ts == -1) {
      ESP_LOGW(TAG, "mktime failed for publish_time: %s", publish_time.c_str());
      return false;
    }

    double diff_seconds = difftime(time.timestamp, publish_time_ts);
    if (diff_seconds > (double)(minutes * 60)) {
      ESP_LOGW(TAG, "Publish time is too old: %s", publish_time.c_str());
      return false;
    }

    if (diff_seconds < -MAX_FUTURE_PUBLISH_TIME_MINUTES * 60) {
      ESP_LOGW(TAG, "Publish time is in the future: %s", publish_time.c_str());
      return false;
    }
    return true;
  }

  bool operator==(const Record &rhs) const {
    return site_name == rhs.site_name && county == rhs.county && aqi == rhs.aqi && pollutant == rhs.pollutant && status == rhs.status && so2 == rhs.so2 &&
           co == rhs.co && o3 == rhs.o3 && o3_8hr == rhs.o3_8hr && pm10 == rhs.pm10 && pm2_5 == rhs.pm2_5 && no2 == rhs.no2 && nox == rhs.nox && no == rhs.no &&
           wind_speed == rhs.wind_speed && wind_direc == rhs.wind_direc && publish_time == rhs.publish_time && co_8hr == rhs.co_8hr &&
           pm2_5_avg == rhs.pm2_5_avg && pm10_avg == rhs.pm10_avg && so2_avg == rhs.so2_avg && longitude == rhs.longitude && latitude == rhs.latitude &&
           site_id == rhs.site_id;
  }
};

class MoenvAQI : public PollingComponent {
 public:
  float get_setup_priority() const override;
  void setup() override;
  void update() override;
  void dump_config() override;

  template <typename V>
  void set_api_key(V key) {
    api_key_ = key;
  }
  template <typename V>
  void set_site_name(V site_name) {
    site_name_ = site_name;
  }
  template <typename V>
  void set_language(V language) {
    language_ = language;
  }
  template <typename V>
  void set_limit(V limit) {
    limit_ = limit;
  }
  template <typename V>
  void set_sensor_expiry(V sensor_expiry) {
    sensor_expiry_ = sensor_expiry;
  }
  template <typename V>
  void set_watchdog_timeout(V watchdog_timeout) {
    watchdog_timeout_ = watchdog_timeout;
  }
  template <typename V>
  void set_http_connect_timeout(V http_connect_timeout) {
    http_connect_timeout_ = http_connect_timeout;
  }
  template <typename V>
  void set_http_timeout(V http_timeout) {
    http_timeout_ = http_timeout;
  }

  void set_time(time::RealTimeClock *rtc) { rtc_ = rtc; }

  Record &get_data() { return this->data_; }
  Trigger<Record &> *get_on_data_change_trigger() { return &this->on_data_change_trigger_; }
  Trigger<> *get_on_error_trigger() { return &this->on_error_trigger_; }

  void set_aqi_sensor(sensor::Sensor *sensor) { aqi_ = sensor; }
  void set_so2_sensor(sensor::Sensor *sensor) { so2_ = sensor; }
  void set_co_sensor(sensor::Sensor *sensor) { co_ = sensor; }
  void set_no_sensor(sensor::Sensor *sensor) { no_ = sensor; }
  void set_wind_speed_sensor(sensor::Sensor *sensor) { wind_speed_ = sensor; }
  void set_co_8hr_sensor(sensor::Sensor *sensor) { co_8hr_ = sensor; }
  void set_pm2_5_avg_sensor(sensor::Sensor *sensor) { pm2_5_avg_ = sensor; }
  void set_so2_avg_sensor(sensor::Sensor *sensor) { so2_avg_ = sensor; }
  void set_o3_sensor(sensor::Sensor *sensor) { o3_ = sensor; }
  void set_o3_8hr_sensor(sensor::Sensor *sensor) { o3_8hr_ = sensor; }
  void set_pm10_sensor(sensor::Sensor *sensor) { pm10_ = sensor; }
  void set_pm2_5_sensor(sensor::Sensor *sensor) { pm2_5_ = sensor; }
  void set_no2_sensor(sensor::Sensor *sensor) { no2_ = sensor; }
  void set_nox_sensor(sensor::Sensor *sensor) { nox_ = sensor; }
  void set_wind_direc_sensor(sensor::Sensor *sensor) { wind_direc_ = sensor; }
  void set_pm10_avg_sensor(sensor::Sensor *sensor) { pm10_avg_ = sensor; }
  void set_site_id_sensor(sensor::Sensor *sensor) { site_id_ = sensor; }
  void set_longitude_sensor(sensor::Sensor *sensor) { longitude_ = sensor; }
  void set_latitude_sensor(sensor::Sensor *sensor) { latitude_ = sensor; }
  void set_site_name_text_sensor(text_sensor::TextSensor *sensor) { current_site_name_ = sensor; }
  void set_county_text_sensor(text_sensor::TextSensor *sensor) { county_ = sensor; }
  void set_pollutant_text_sensor(text_sensor::TextSensor *sensor) { pollutant_ = sensor; }
  void set_status_text_sensor(text_sensor::TextSensor *sensor) { status_ = sensor; }
  void set_publish_time_text_sensor(text_sensor::TextSensor *sensor) { publish_time_ = sensor; }
  void set_last_updated_text_sensor(text_sensor::TextSensor *sensor) { last_updated_ = sensor; }
  void set_last_success_text_sensor(text_sensor::TextSensor *sensor) { last_success_ = sensor; }
  void set_last_error_text_sensor(text_sensor::TextSensor *sensor) { last_error_ = sensor; }

 protected:
  TemplatableValue<std::string> api_key_;
  TemplatableValue<std::string> site_name_;
  TemplatableValue<std::string> language_;
  TemplatableValue<std::uint32_t> limit_;
  TemplatableValue<uint32_t> watchdog_timeout_;
  TemplatableValue<uint32_t> http_connect_timeout_;
  TemplatableValue<uint32_t> http_timeout_;
  TemplatableValue<uint32_t> sensor_expiry_;
  time::RealTimeClock *rtc_{nullptr};

  sensor::Sensor *aqi_{nullptr};
  sensor::Sensor *so2_{nullptr};
  sensor::Sensor *co_{nullptr};
  sensor::Sensor *no_{nullptr};
  sensor::Sensor *wind_speed_{nullptr};
  sensor::Sensor *co_8hr_{nullptr};
  sensor::Sensor *pm2_5_avg_{nullptr};
  sensor::Sensor *so2_avg_{nullptr};
  sensor::Sensor *o3_{nullptr};
  sensor::Sensor *o3_8hr_{nullptr};
  sensor::Sensor *pm10_{nullptr};
  sensor::Sensor *pm2_5_{nullptr};
  sensor::Sensor *no2_{nullptr};
  sensor::Sensor *nox_{nullptr};
  sensor::Sensor *wind_direc_{nullptr};
  sensor::Sensor *pm10_avg_{nullptr};
  sensor::Sensor *site_id_{nullptr};
  sensor::Sensor *longitude_{nullptr};
  sensor::Sensor *latitude_{nullptr};
  text_sensor::TextSensor *current_site_name_{nullptr};
  text_sensor::TextSensor *county_{nullptr};
  text_sensor::TextSensor *pollutant_{nullptr};
  text_sensor::TextSensor *status_{nullptr};
  text_sensor::TextSensor *publish_time_{nullptr};
  text_sensor::TextSensor *last_updated_{nullptr};
  text_sensor::TextSensor *last_success_{nullptr};
  text_sensor::TextSensor *last_error_{nullptr};

  Trigger<Record &> on_data_change_trigger_{};
  Trigger<> on_error_trigger_{};

  ESPPreferenceObject pref_;
  size_t last_successful_offset_ = 0;
  std::string last_site_name_;
  uint32_t last_limit_;
  Record data_;

  bool validate_config_();
  bool send_request_();
  void reset_site_data_();
  bool process_response_(Stream &stream, Record &record, int &total);
  bool check_changes_(const Record &new_data);
  bool validate_record_();
  void publish_states_();
};

}  // namespace moenv_aqi
}  // namespace esphome
