"""Light platform — CCT LED on a Mantra R00143 ceiling fan.

Supports brightness + color_temperature (3000K..5000K, 100K steps); the
device's discrete 10% brightness grid is enforced in write_state by snapping
the incoming continuous value to the nearest step before transmitting.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID

from .. import (
    CONF_MANTRA_ID,
    MantraRf433Device,
    mantra_rf_433_ns,
)

DEPENDENCIES = ["mantra_rf_433"]

MantraLight = mantra_rf_433_ns.class_(
    "MantraLight", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(MantraLight),
        cv.Required(CONF_MANTRA_ID): cv.use_id(MantraRf433Device),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_MANTRA_ID])
    cg.add(var.set_parent(parent))

    await light.register_light(var, config)
