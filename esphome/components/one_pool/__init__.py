import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

CODEOWNERS = ["@martial"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["switch", "binary_sensor"]

CONF_SHARED_KEY = "shared_key"

one_pool_ns = cg.esphome_ns.namespace("one_pool")
OnePoolClient = one_pool_ns.class_(
    "OnePoolClient", cg.Component, ble_client.BLEClientNode
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OnePoolClient),
            cv.Required(CONF_SHARED_KEY): cv.string,
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    cg.add(var.set_shared_key(config[CONF_SHARED_KEY]))
