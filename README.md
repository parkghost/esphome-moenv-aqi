# ESPHome MOENV AQI Component

This is an external component for ESPHome that fetches Air Quality Index (AQI) data for a specific monitoring site from Taiwan's Ministry of Environment (MOENV) Open Data platform.

## Prerequisites

- You need a MOENV Open Data API authorization key. You can obtain one from the [MOENV Open Data Platform](https://data.moenv.gov.tw/paradigm).
- You need to know the exact `site_name` for the monitoring station you want to use (e.g., "永和"). Refer to the [MOENV API Dataset](https://data.moenv.gov.tw/dataset/detail/AQX_P_432).

## Components

### `moenv_aqi`

#### Configuration Variables:

* **id** (Optional, ID): The id to use for this component.
* **api_key** (Required, string, templatable): Your MOENV Open Data API key.
* **site_name** (Required, string, templatable): The name of the monitoring site (e.g., "永和", "板橋").
* **language** (Optional, string, templatable): Language for the data. Defaults to `zh`. Other options might include `en`.
* **limit** (Optional, integer, templatable): Number of records to fetch per API request page. Defaults to `20`.
* **sensor_expiry** (Optional, Time, templatable): How long fetched data is considered valid relative to its publish time. Defaults to `90min`.
* **watchdog_timeout** (Optional, Time, templatable): Timeout for the watchdog timer during the HTTP request. Defaults to `30s`.
* **http_connect_timeout** (Optional, Time, templatable): Timeout for establishing the HTTP connection. Defaults to `10s`.
* **http_timeout** (Optional, Time, templatable): Timeout for the TCP connection. Defaults to `10s`.
* **retry_count** (Optional, integer, templatable): Number of retry attempts for failed HTTP requests. Defaults to `1`. Range: 0-5.
* **retry_delay** (Optional, Time, templatable): Base delay between retry attempts. Uses exponential backoff with jitter. Defaults to `1s`.
* **update_interval** (Optional, Time): How often to check for new data. Defaults to `never` (manual updates only).

#### Automations

##### Automation Triggers:

* **on_data_change** (Optional, Action): An automation action to be performed when new data is received. In Lambdas you can get the value from the trigger with `data`.
*   **on_error** (Optional, Action): An automation action to be performed when a fetch error occurs.

#### Example

```yaml
external_components:
  - source: github://parkghost/esphome-moenv-aqi
    components: [moenv_aqi]

time:
  - platform: sntp
    id: esp_time
    timezone: Asia/Taipei

moenv_aqi:
  - api_key: !secret moenv_api_key
    id: moenv_aqi_id
    site_name: "永和"
    update_interval: never
    retry_count: 3
    retry_delay: 1s

sensor:
  - platform: moenv_aqi
    aqi:
      name: "AQI"
    so2:
      name: "SO2"
    co:
      name: "CO"
    "no":
      name: "NO"
    wind_speed:
      name: "Wind Speed"
    co_8hr:
      name: "CO 8hr"
    pm2_5_avg:
      name: "PM2.5 Avg"
    so2_avg:
      name: "SO2 Avg"
    o3:
      name: "O3"
    o3_8hr:
      name: "O3 8hr"
    pm10:
      name: "PM10"
    pm2_5:
      name: "PM2.5"
    no2:
      name: "NO2"
    nox:
      name: "NOx"
    wind_direc:
      name: "Wind Direction"
    pm10_avg:
      name: "PM10 Avg"
    site_id:
      name: "Site ID"
    longitude:
      name: "Longitude"
    latitude:
      name: "Latitude"

text_sensor:
  - platform: moenv_aqi
    site_name:
      name: "Site Name"
    county:
      name: "County"
    pollutant:
      name: "Pollutant"
    status:
      name: "Status"
    publish_time:
      name: "Publish Time"
    last_updated:
      name: "Last Updated"
    last_success:
      name: "Last Success"
    last_error:
      name: "Last Error"
```

#### Use In Lambdas
```cpp
auto data = id(moenv_aqi_id).get_data();
auto time = id(esp_time).now();
// Check publish time is within 90 minutes
if (data.validate(time, 90)) {
  ESP_LOGI("moenv_aqi", "Site Name: %s", data.site_name.c_str());
  ESP_LOGI("moenv_aqi", "County: %s", data.county.c_str());
  ESP_LOGI("moenv_aqi", "AQI: %d", data.aqi);
  ESP_LOGI("moenv_aqi", "Pollutant: %s", data.pollutant.c_str());
  ESP_LOGI("moenv_aqi", "Status: %s", data.status.c_str());
  ESP_LOGI("moenv_aqi", "SO2: %.2f", data.so2);
  ESP_LOGI("moenv_aqi", "CO: %.2f", data.co);
  ESP_LOGI("moenv_aqi", "O3: %d", data.o3);
  ESP_LOGI("moenv_aqi", "O3 8hr: %d", data.o3_8hr);
  ESP_LOGI("moenv_aqi", "PM10: %d", data.pm10);
  ESP_LOGI("moenv_aqi", "PM2.5: %d", data.pm2_5);
  ESP_LOGI("moenv_aqi", "NO2: %d", data.no2);
  ESP_LOGI("moenv_aqi", "NOx: %d", data.nox);
  ESP_LOGI("moenv_aqi", "NO: %.2f", data.no);
  ESP_LOGI("moenv_aqi", "Wind Speed: %.2f", data.wind_speed);
  ESP_LOGI("moenv_aqi", "Wind Direction: %d", data.wind_direc);
  ESP_LOGI("moenv_aqi", "Publish Time: %s", data.publish_time.c_str());
  ESP_LOGI("moenv_aqi", "CO 8hr: %.2f", data.co_8hr);
  ESP_LOGI("moenv_aqi", "PM2.5 Avg: %.2f", data.pm2_5_avg);
  ESP_LOGI("moenv_aqi", "PM10 Avg: %d", data.pm10_avg);
  ESP_LOGI("moenv_aqi", "SO2 Avg: %.2f", data.so2_avg);
  ESP_LOGI("moenv_aqi", "Longitude: %.6f", data.longitude);
  ESP_LOGI("moenv_aqi", "Latitude: %.6f", data.latitude);
  ESP_LOGI("moenv_aqi", "Site ID: %d", data.site_id);
} else {
  ESP_LOGI("moenv_aqi", "Data is not valid");
}
```