import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    UNIT_EMPTY,
    ICON_EMPTY,
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_DEGREES,
    ICON_GAS_CYLINDER,
    ICON_MOLECULE_CO,
    ICON_MOLECULE_CO2,
    ICON_GRAIN,
    ICON_WEATHER_WINDY,
    ICON_SIGN_DIRECTION,
    ICON_GAUGE,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_SULPHUR_DIOXIDE,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_NITROGEN_MONOXIDE,
    DEVICE_CLASS_NITROGEN_DIOXIDE,
    DEVICE_CLASS_OZONE,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_WIND_SPEED,
)
from . import CHILD_SCHEMA, CONF_MOENV_AQI_ID

DEPENDENCIES = ["moenv_aqi"]

CONF_SO2 = "so2"
CONF_CO = "co"
CONF_NO = "no"
CONF_WIND_SPEED = "wind_speed"
CONF_CO_8HR = "co_8hr"
CONF_PM2_5_AVG = "pm2_5_avg"
CONF_SO2_AVG = "so2_avg"
CONF_AQI = "aqi"
CONF_O3 = "o3"
CONF_O3_8HR = "o3_8hr"
CONF_PM10 = "pm10"
CONF_PM2_5 = "pm2_5"
CONF_NO2 = "no2"
CONF_NOX = "nox"
CONF_WIND_DIREC = "wind_direc"
CONF_PM10_AVG = "pm10_avg"
CONF_SITE_ID = "site_id"
CONF_LONGITUDE = "longitude"
CONF_LATITUDE = "latitude"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_SO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_GAS_CYLINDER,
                device_class=DEVICE_CLASS_SULPHUR_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_CO): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO,
                device_class=DEVICE_CLASS_CARBON_MONOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=2,
            ),
            cv.Optional(CONF_NO): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_GAS_CYLINDER,
                device_class=DEVICE_CLASS_NITROGEN_MONOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_WIND_SPEED): sensor.sensor_schema(
                unit_of_measurement="m/s",
                icon=ICON_WEATHER_WINDY,
                device_class=DEVICE_CLASS_WIND_SPEED,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_CO_8HR): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO,
                device_class=DEVICE_CLASS_CARBON_MONOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_PM2_5_AVG): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_GRAIN,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_SO2_AVG): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_GAS_CYLINDER,
                device_class=DEVICE_CLASS_SULPHUR_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_AQI): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_GAUGE,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_O3): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_GAS_CYLINDER,
                device_class=DEVICE_CLASS_OZONE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_O3_8HR): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_GAS_CYLINDER,
                device_class=DEVICE_CLASS_OZONE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_PM10): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_GRAIN,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_PM2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_GRAIN,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_NO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_GAS_CYLINDER,
                device_class=DEVICE_CLASS_NITROGEN_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_NOX): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_GAS_CYLINDER,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_WIND_DIREC): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                icon=ICON_SIGN_DIRECTION,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_PM10_AVG): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_GRAIN,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_SITE_ID): sensor.sensor_schema(
                accuracy_decimals=0, entity_category="diagnostic"
            ),
            cv.Optional(CONF_LONGITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                accuracy_decimals=6,
                entity_category="diagnostic",
            ),
            cv.Optional(CONF_LATITUDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                accuracy_decimals=6,
                entity_category="diagnostic",
            ),
        }
    )
    .extend(CHILD_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

SENSORS = [
    CONF_SO2,
    CONF_CO,
    CONF_NO,
    CONF_WIND_SPEED,
    CONF_CO_8HR,
    CONF_PM2_5_AVG,
    CONF_SO2_AVG,
    CONF_AQI,
    CONF_O3,
    CONF_O3_8HR,
    CONF_PM10,
    CONF_PM2_5,
    CONF_NO2,
    CONF_NOX,
    CONF_WIND_DIREC,
    CONF_PM10_AVG,
    CONF_SITE_ID,
    CONF_LONGITUDE,
    CONF_LATITUDE,
]


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MOENV_AQI_ID])

    for key in SENSORS:
        if sens_config := config.get(key):
            sens = await sensor.new_sensor(sens_config)
            cg.add(getattr(parent, f"set_{key}_sensor")(sens))
