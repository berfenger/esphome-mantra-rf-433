"""Fan platform — 6-speed reversible ceiling fan on a Mantra RF 433Mhz.

Exposes the device's three operating modes as HA preset_modes on the fan card
(no separate select needed). The default labels are English — `Normal`,
`Night`, `Breeze` — but each can be overridden via the `preset_modes:` map
for localisation. The underlying protocol byte is unchanged; only the display
string differs.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan

from .. import (
    CONF_MANTRA_ID,
    MantraRf433Device,
    mantra_rf_433_ns,
)

DEPENDENCIES = ["mantra_rf_433"]

CONF_PRESET_MODES = "preset_modes"
CONF_NORMAL = "normal"
CONF_NIGHT = "night"
CONF_BREEZE = "breeze"

MantraFan = mantra_rf_433_ns.class_("MantraFan", fan.Fan, cg.Component)

PRESET_MODES_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_NORMAL, default="Normal"): cv.string_strict,
        cv.Optional(CONF_NIGHT, default="Night"): cv.string_strict,
        cv.Optional(CONF_BREEZE, default="Breeze"): cv.string_strict,
    }
)


def _validate_unique_labels(config):
    labels = [config[CONF_NORMAL], config[CONF_NIGHT], config[CONF_BREEZE]]
    if len(set(labels)) != len(labels):
        raise cv.Invalid(
            f"preset_modes labels must be unique (got {labels})"
        )
    return config


CONFIG_SCHEMA = fan.fan_schema(MantraFan).extend(
    {
        cv.Required(CONF_MANTRA_ID): cv.use_id(MantraRf433Device),
        cv.Optional(CONF_PRESET_MODES, default={}): cv.All(
            PRESET_MODES_SCHEMA, _validate_unique_labels
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_MANTRA_ID])
    cg.add(var.set_parent(parent))

    presets = config[CONF_PRESET_MODES]
    cg.add(var.set_preset_normal(presets[CONF_NORMAL]))
    cg.add(var.set_preset_night(presets[CONF_NIGHT]))
    cg.add(var.set_preset_breeze(presets[CONF_BREEZE]))
