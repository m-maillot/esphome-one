import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from . import OnePoolClient, one_pool_ns

CONF_ONE_POOL_ID = "one_pool_id"
CONF_TYPE = "type"

OnePoolSwitch = one_pool_ns.class_("OnePoolSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.switch_schema(OnePoolSwitch).extend(
    {
        cv.GenerateID(CONF_ONE_POOL_ID): cv.use_id(OnePoolClient),
        cv.Required(CONF_TYPE): cv.one_of("pump", "light", lower=True),
    }
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_ONE_POOL_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE] == "pump"))
    cg.add(parent.register_switch(var, config[CONF_TYPE] == "pump"))
