#pragma once

#include <concepts>
#include <functional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <algorithm>

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

static constexpr std::string_view FIELD_SITENAME = "sitename";
static constexpr std::string_view FIELD_COUNTY = "county";
static constexpr std::string_view FIELD_AQI = "aqi";
static constexpr std::string_view FIELD_POLLUTANT = "pollutant";
static constexpr std::string_view FIELD_STATUS = "status";
static constexpr std::string_view FIELD_SO2 = "so2";
static constexpr std::string_view FIELD_CO = "co";
static constexpr std::string_view FIELD_O3 = "o3";
static constexpr std::string_view FIELD_O3_8HR = "o3_8hr";
static constexpr std::string_view FIELD_PM10 = "pm10";
static constexpr std::string_view FIELD_PM25 = "pm2.5";
static constexpr std::string_view FIELD_NO2 = "no2";
static constexpr std::string_view FIELD_NOX = "nox";
static constexpr std::string_view FIELD_NO = "no";
static constexpr std::string_view FIELD_WIND_SPEED = "wind_speed";
static constexpr std::string_view FIELD_WIND_DIREC = "wind_direc";
static constexpr std::string_view FIELD_PUBLISH_TIME = "publishtime";
static constexpr std::string_view FIELD_CO_8HR = "co_8hr";
static constexpr std::string_view FIELD_PM25_AVG = "pm2.5_avg";
static constexpr std::string_view FIELD_PM10_AVG = "pm10_avg";
static constexpr std::string_view FIELD_SO2_AVG = "so2_avg";
static constexpr std::string_view FIELD_LONGITUDE = "longitude";
static constexpr std::string_view FIELD_LATITUDE = "latitude";
static constexpr std::string_view FIELD_SITEID = "siteid";

static const int MAX_FUTURE_PUBLISH_TIME_MINUTES = 10;


// Buffered stream class that pre-reads data into internal buffer
class BufferedStream : public Stream {
 public:
  static constexpr size_t MIN_BUFFER_SIZE = 64;
  static constexpr size_t MAX_BUFFER_SIZE = 4096;
  static constexpr size_t DEFAULT_BUFFER_SIZE = 512;
  
  BufferedStream(Stream& stream, size_t bufferSize = DEFAULT_BUFFER_SIZE) 
    : stream_(stream), bytes_read_(0), buffer_pos_(0), buffer_len_(0) {
    
    // Validate and clamp buffer size
    if (bufferSize < MIN_BUFFER_SIZE) {
      ESP_LOGW(TAG, "Buffer size %zu too small, using minimum %zu", bufferSize, MIN_BUFFER_SIZE);
      bufferSize = MIN_BUFFER_SIZE;
    } else if (bufferSize > MAX_BUFFER_SIZE) {
      ESP_LOGW(TAG, "Buffer size %zu too large, using maximum %zu", bufferSize, MAX_BUFFER_SIZE);
      bufferSize = MAX_BUFFER_SIZE;
    }
    
    buffer_.reserve(bufferSize);
    buffer_.resize(bufferSize);
    if (buffer_.size() != bufferSize) {
      ESP_LOGE(TAG, "Failed to allocate buffer for BufferedStream (requested: %zu, got: %zu)", 
               bufferSize, buffer_.size());
      buffer_.clear();
    } else {
      ESP_LOGD(TAG, "BufferedStream created with buffer size: %zu", buffer_.size());
    }
  }
  
  // Explicitly delete copy operations to prevent issues
  BufferedStream(const BufferedStream&) = delete;
  BufferedStream& operator=(const BufferedStream&) = delete;
  
  // Allow move operations
  BufferedStream(BufferedStream&&) = default;
  BufferedStream& operator=(BufferedStream&&) = default;
  
  ~BufferedStream() = default;
  
  // Simple health check based on buffer availability
  bool isHealthy() const { return !buffer_.empty(); }
  
  int available() override {
    // Return buffered data + underlying stream data
    return (buffer_len_ - buffer_pos_) + stream_.available();
  }
  
  int read() override {
    // If no buffer allocated, delegate to underlying stream
    if (buffer_.empty()) {
      int byte = stream_.read();
      if (byte != -1) {
        bytes_read_++;
      }
      return byte;
    }
    
    // If buffer is empty, try to fill it
    if (buffer_pos_ >= buffer_len_) {
      if (!fillBuffer()) {
        // Fall back to direct stream reading
        int byte = stream_.read();
        if (byte != -1) {
          bytes_read_++;
        }
        return byte;
      }
    }
    
    // Return from buffer if available
    if (buffer_pos_ < buffer_len_) {
      int byte = buffer_[buffer_pos_++];
      bytes_read_++;
      
      // Only log on significant milestones or special debug mode
      #if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
      if (bytes_read_ % 500 == 0 || (byte == '{' && bytes_read_ < 50) || 
          (byte == '}' && bytes_read_ > 100)) {
        ESP_LOGVV(TAG, "Read byte %d: 0x%02X ('%c') at position %zu", 
                  byte, byte, (byte >= 32 && byte <= 126) ? (char)byte : '.', bytes_read_);
      }
      #endif
      
      return byte;
    }
    
    return -1; // No data available
  }
  
  int peek() override {
    // If buffer is empty, try to fill it
    if (buffer_pos_ >= buffer_len_) {
      fillBuffer();
    }
    
    // Return from buffer if available
    if (buffer_pos_ < buffer_len_) {
      return buffer_[buffer_pos_];
    }
    
    return -1; // No data available
  }
  
  void flush() override { stream_.flush(); }
  size_t write(uint8_t byte) override { return stream_.write(byte); }
  
  // Delegate other important methods (bypass buffer for these operations)
  void setTimeout(unsigned long timeout) { stream_.setTimeout(timeout); }
  bool find(char* target) { 
    ESP_LOGV(TAG, "find() bypassing buffer, delegating to underlying stream");
    return stream_.find(target); 
  }
  bool findUntil(char* target, char* terminator) { 
    ESP_LOGV(TAG, "findUntil() bypassing buffer, delegating to underlying stream");
    return stream_.findUntil(target, terminator); 
  }
  
  String readStringUntil(char terminator) {
    String result;
    result.reserve(128); // Pre-allocate reasonable capacity
    
    int c;
    while ((c = read()) != -1) {
      if (c == terminator) break;
      result += (char)c;
      
      // Prevent runaway strings
      if (result.length() > 1024) {
        ESP_LOGW(TAG, "readStringUntil('%c') exceeded 1024 chars, truncating", terminator);
        break;
      }
    }
    
    ESP_LOGV(TAG, "readStringUntil('%c') returned: %s (length: %d)", 
             terminator, result.c_str(), result.length());
    return result;
  }
  
  size_t getBytesRead() const { return bytes_read_; }
  size_t getBufferSize() const { return buffer_.size(); }
  size_t getBufferedBytes() const { return buffer_len_ - buffer_pos_; }
  
  // Additional monitoring methods
  float getBufferUtilization() const { 
    return buffer_.empty() ? 0.0f : (float)buffer_len_ / (float)buffer_.size(); 
  }
  
  bool hasBufferedData() const { return buffer_pos_ < buffer_len_; }
  
  // Debug information
  void logBufferStats() const {
    ESP_LOGD(TAG, "Buffer stats: size=%zu, pos=%zu, len=%zu, utilization=%.1f%%, healthy=%s", 
             buffer_.size(), buffer_pos_, buffer_len_, 
             getBufferUtilization() * 100.0f, isHealthy() ? "true" : "false");
  }
  
  // Drain remaining buffered data to avoid SSL connection state issues
  void drainBuffer() {
    size_t remaining = buffer_len_ - buffer_pos_;
    if (remaining > 0) {
      ESP_LOGD(TAG, "Draining %d remaining bytes from buffer", remaining);
      buffer_pos_ = buffer_len_; // Mark buffer as empty
    }
    
    // Also drain any remaining data from underlying stream
    static constexpr int DRAIN_LIMIT = 200;
    int drained = 0;
    while (stream_.available() && drained < DRAIN_LIMIT) {
      int byte = stream_.read();
      if (byte == -1) break;
      drained++;
    }
    if (drained > 0) {
      ESP_LOGD(TAG, "Drained %d bytes from underlying stream", drained);
    }
  }
  
 private:
  bool fillBuffer() {
    if (buffer_.empty()) {
      // No buffer available, can't fill
      return false;
    }
    
    buffer_pos_ = 0;
    buffer_len_ = 0;
    
    // Read data into buffer, but don't over-read to avoid SSL state issues
    size_t available = stream_.available();
    if (available == 0) {
      return false; // No data to read
    }
    
    // More efficient reading - read in chunks when possible
    size_t read_limit = std::min(buffer_.size(), available);
    
    // Try to read data efficiently
    size_t bytes_to_read = std::min(read_limit, buffer_.size());
    
    for (size_t i = 0; i < bytes_to_read; i++) {
      int byte = stream_.read();
      if (byte == -1) break;
      buffer_[buffer_len_++] = static_cast<uint8_t>(byte);
    }
    
    if (buffer_len_ > 0) {
      #if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
      ESP_LOGVV(TAG, "Filled buffer with %zu bytes (available: %zu)", buffer_len_, available);
      #endif
      return true;
    }
    
    return false;
  }
  
  Stream& stream_;
  size_t bytes_read_;
  std::vector<uint8_t> buffer_;
  size_t buffer_pos_;  // Current position in buffer
  size_t buffer_len_;  // Amount of data in buffer
};


// Forward declaration for FieldMapping
struct Record;

struct FieldMapping {
  std::string_view key;
  bool required;
  std::function<void(Record &, JsonVariant &)> setter;
};

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

  bool operator==(const Record &rhs) const = default;
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
