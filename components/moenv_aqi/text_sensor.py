import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ICON_CHEMICAL_WEAPON, ICON_GAUGE, ICON_TIMER
from . import CHILD_SCHEMA, CONF_MOENV_AQI_ID

DEPENDENCIES = ["moenv_aqi"]

CONF_SITE_NAME = "site_name"
CONF_COUNTY = "county"
CONF_POLLUTANT = "pollutant"
CONF_STATUS = "status"
CONF_PUBLISH_TIME = "publish_time"
CONF_LAST_UPDATED = "last_updated"
CONF_LAST_SUCCESS = "last_success"
CONF_LAST_ERROR = "last_error"

TEXT_SENSORS = [
    CONF_SITE_NAME,
    CONF_COUNTY,
    CONF_POLLUTANT,
    CONF_STATUS,
    CONF_PUBLISH_TIME,
    CONF_LAST_UPDATED,
    CONF_LAST_SUCCESS,
    CONF_LAST_ERROR,
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SITE_NAME): text_sensor.text_sensor_schema(
            icon="mdi:home",
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_COUNTY): text_sensor.text_sensor_schema(
            icon="mdi:map-marker",
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_POLLUTANT): text_sensor.text_sensor_schema(
            icon=ICON_CHEMICAL_WEAPON
        ),
        cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(icon=ICON_GAUGE),
        cv.Optional(CONF_PUBLISH_TIME): text_sensor.text_sensor_schema(
            icon=ICON_TIMER, entity_category="diagnostic"
        ),
        cv.Optional(CONF_LAST_UPDATED): text_sensor.text_sensor_schema(
            icon="mdi:calendar-clock",
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_LAST_SUCCESS): text_sensor.text_sensor_schema(
            icon="mdi:calendar-clock",
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_LAST_ERROR): text_sensor.text_sensor_schema(
            icon="mdi:calendar-clock",
            entity_category="diagnostic",
        ),
    }
).extend(CHILD_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MOENV_AQI_ID])

    for key in TEXT_SENSORS:
        if sens_config := config.get(key):
            sens = await text_sensor.new_text_sensor(sens_config)
            cg.add(getattr(parent, f"set_{key}_text_sensor")(sens))
