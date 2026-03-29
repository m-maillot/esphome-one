import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID
from . import OnePoolClient, one_pool_ns

CONF_ONE_POOL_ID = "one_pool_id"
CONF_TYPE = "type"

CONFIG_SCHEMA = sensor.sensor_schema().extend(
    {
        cv.GenerateID(CONF_ONE_POOL_ID): cv.use_id(OnePoolClient),
        cv.Required(CONF_TYPE): cv.one_of("ble_disconnects", lower=True),
    }
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_ONE_POOL_ID])
    if config[CONF_TYPE] == "ble_disconnects":
        cg.add(parent.set_ble_disconnects_sensor(var))
