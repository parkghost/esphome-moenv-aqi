import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIME_ID
from esphome.components import time
from esphome import automation

DEPENDENCIES = ["network", "time"]
AUTO_LOAD = ["json", "watchdog", "sensor", "text_sensor"]

moenv_aqi_ns = cg.esphome_ns.namespace("moenv_aqi")
MoenvAQI = moenv_aqi_ns.class_("MoenvAQI", cg.PollingComponent)
MoenvAQIRecord = moenv_aqi_ns.struct("Record")
MoenvAQIRecordPtr = MoenvAQIRecord.operator("ref")

CONF_API_KEY = "api_key"
CONF_SITE_NAME = "site_name"
CONF_LANGUAGE = "language"
CONF_LIMIT = "limit"
CONF_SENSOR_EXPIRY = "sensor_expiry"
CONF_ON_DATA_CHANGE = "on_data_change"
CONF_ON_ERROR = "on_error"
CONF_WATCHDOG_TIMEOUT = "watchdog_timeout"
CONF_HTTP_CONNECT_TIMEOUT = "http_connect_timeout"
CONF_HTTP_TIMEOUT = "http_timeout"
CONF_MOENV_AQI_ID = "moenv_aqi_id"

CHILD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MOENV_AQI_ID): cv.use_id(MoenvAQI),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(MoenvAQI),
                cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
                cv.Optional(CONF_API_KEY, default=""): cv.templatable(cv.string),
                cv.Optional(CONF_SITE_NAME, default=""): cv.templatable(cv.string),
                cv.Optional(CONF_LANGUAGE, default="zh"): cv.templatable(cv.string),
                cv.Optional(CONF_LIMIT, default=20): cv.templatable(cv.uint32_t),
                cv.Optional(CONF_SENSOR_EXPIRY, default="90min"): cv.templatable(
                    cv.All(
                        cv.positive_not_null_time_period,
                        cv.positive_time_period_milliseconds,
                    )
                ),
                cv.Optional(CONF_ON_DATA_CHANGE): automation.validate_automation(),
                cv.Optional(CONF_ON_ERROR): automation.validate_automation(),
                cv.Optional(CONF_WATCHDOG_TIMEOUT, default="30s"): cv.templatable(
                    cv.All(
                        cv.positive_not_null_time_period,
                        cv.positive_time_period_milliseconds,
                    )
                ),
                cv.Optional(CONF_HTTP_CONNECT_TIMEOUT, default="10s"): cv.templatable(
                    cv.All(
                        cv.positive_not_null_time_period,
                        cv.positive_time_period_milliseconds,
                    )
                ),
                cv.Optional(CONF_HTTP_TIMEOUT, default="10s"): cv.templatable(
                    cv.All(
                        cv.positive_not_null_time_period,
                        cv.positive_time_period_milliseconds,
                    )
                ),
            }
        ).extend(cv.polling_component_schema("never"))
    ),
    cv.only_on_esp32,
    cv.only_with_arduino,
)


async def to_code(configs):
    for config in configs:
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)

        if CONF_TIME_ID in config:
            rtc = await cg.get_variable(config[CONF_TIME_ID])
            cg.add(var.set_time(rtc))
        if CONF_API_KEY in config:
            api_key = await cg.templatable(config[CONF_API_KEY], [], cg.std_string)
            cg.add(var.set_api_key(api_key))
        if CONF_SITE_NAME in config:
            site_name = await cg.templatable(config[CONF_SITE_NAME], [], cg.std_string)
            cg.add(var.set_site_name(site_name))
        if CONF_LANGUAGE in config:
            language = await cg.templatable(config[CONF_LANGUAGE], [], cg.std_string)
            cg.add(var.set_language(language))
        if CONF_LIMIT in config:
            limit = await cg.templatable(config[CONF_LIMIT], [], cg.uint32)
            cg.add(var.set_limit(limit))
        if CONF_SENSOR_EXPIRY in config:
            duration = await cg.templatable(config[CONF_SENSOR_EXPIRY], [], cg.uint32)
            cg.add(var.set_sensor_expiry(duration))
        for trigger in config.get(CONF_ON_DATA_CHANGE, []):
            await automation.build_automation(
                var.get_on_data_change_trigger(), [(MoenvAQIRecordPtr, "data")], trigger
            )
        for trigger in config.get(CONF_ON_ERROR, []):
            await automation.build_automation(
                var.get_on_error_trigger(),
                [],
                trigger,
            )
        if CONF_WATCHDOG_TIMEOUT in config:
            timeout = await cg.templatable(config[CONF_WATCHDOG_TIMEOUT], [], cg.uint32)
            cg.add(var.set_watchdog_timeout(timeout))
        if CONF_HTTP_CONNECT_TIMEOUT in config:
            timeout = await cg.templatable(
                config[CONF_HTTP_CONNECT_TIMEOUT], [], cg.uint32
            )
            cg.add(var.set_http_connect_timeout(timeout))
        if CONF_HTTP_TIMEOUT in config:
            timeout = await cg.templatable(config[CONF_HTTP_TIMEOUT], [], cg.uint32)
            cg.add(var.set_http_timeout(timeout))

    cg.add_library("WiFiClientSecure", None)
    cg.add_library("HTTPClient", None)
