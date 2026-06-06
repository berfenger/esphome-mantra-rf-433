"""mantra_rf_433 — bridge + per-fan devices for Mantra 433 MHz ceiling fans.

Covers the R00143, R00149, and the wider PT2262-class 48-bit OEM family
they belong to. Mantra's 2.4 GHz BLE-controlled fans use a different radio
and protocol and are not supported here — the "433" in the name is the
discriminator.

The bridge attaches itself to a shared remote_receiver / remote_transmitter
pair, owns the protocol encode/decode, and routes decoded frames to the
per-fan devices declared inline under `fans:`. Each fan holds its own
persisted state and is the parent referenced by the light / fan entity
platforms in this same component directory.

The bridge coexists with any other rc_switch / on_raw consumers on the same
remote_receiver — it simply registers itself as one more listener.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import remote_receiver, remote_transmitter
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@berfenger"]
DEPENDENCIES = ["remote_receiver", "remote_transmitter"]
MULTI_CONF = True

CONF_RECEIVER_ID = "receiver_id"
CONF_TRANSMITTER_ID = "transmitter_id"
CONF_BRIDGE_ID = "bridge_id"
CONF_FANS = "fans"
CONF_ADDRESS = "address"
CONF_FLAGS_K = "flags_k"
CONF_LED_FAN = "led_fan"
CONF_BUTTON = "button"
CONF_REPEATS = "repeats"
CONF_ON_FRAME = "on_frame"

# Attribute used by the light / fan platforms to point each entity at the
# per-fan device. Deliberately not `device_id` — that name is reserved by
# ESPHome for HA subdevice grouping at the platform level.
CONF_MANTRA_ID = "mantra_id"

mantra_rf_433_ns = cg.esphome_ns.namespace("mantra_rf_433")
MantraRf433Bridge = mantra_rf_433_ns.class_("MantraRf433Bridge", cg.Component)
MantraRf433Device = mantra_rf_433_ns.class_("MantraRf433Device", cg.Component)
MantraRf433Trigger = mantra_rf_433_ns.class_(
    "MantraRf433Trigger", automation.Trigger.template()
)
MantraRf433Frame = mantra_rf_433_ns.class_("MantraRf433Frame")
TransmitMantraRf433Action = mantra_rf_433_ns.class_(
    "TransmitMantraRf433Action", automation.Action
)

FAN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(MantraRf433Device),
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Optional(CONF_REPEATS, default=4): cv.int_range(min=1, max=32),
    }
).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MantraRf433Bridge),
        cv.Required(CONF_RECEIVER_ID): cv.use_id(remote_receiver.RemoteReceiverComponent),
        cv.Required(CONF_TRANSMITTER_ID): cv.use_id(
            remote_transmitter.RemoteTransmitterComponent
        ),
        cv.Optional(CONF_ON_FRAME): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MantraRf433Trigger),
            }
        ),
        cv.Optional(CONF_FANS, default=[]): cv.ensure_list(FAN_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    recv = await cg.get_variable(config[CONF_RECEIVER_ID])
    cg.add(var.set_receiver(recv))

    tx = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(tx))

    for conf in config.get(CONF_ON_FRAME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_trigger(trigger))
        await automation.build_automation(
            trigger, [(MantraRf433Frame, "x")], conf
        )

    for fan_conf in config[CONF_FANS]:
        fan_var = cg.new_Pvariable(fan_conf[CONF_ID])
        await cg.register_component(fan_var, fan_conf)
        cg.add(fan_var.set_bridge(var))
        cg.add(fan_var.set_address(fan_conf[CONF_ADDRESS]))
        cg.add(fan_var.set_repeats(fan_conf[CONF_REPEATS]))


TRANSMIT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BRIDGE_ID): cv.use_id(MantraRf433Bridge),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.hex_uint16_t),
        cv.Optional(CONF_FLAGS_K, default=0): cv.templatable(cv.hex_uint8_t),
        cv.Optional(CONF_LED_FAN, default=0): cv.templatable(cv.hex_uint8_t),
        cv.Required(CONF_BUTTON): cv.templatable(cv.hex_uint8_t),
        cv.Optional(CONF_REPEATS, default=4): cv.templatable(cv.uint8_t),
    }
)


@automation.register_action(
    "mantra_rf_433.transmit",
    TransmitMantraRf433Action,
    TRANSMIT_SCHEMA,
    # The action only overrides play() (not play_complex()), so play_next_()
    # always runs before play_complex() returns — nothing is deferred to a
    # callback/timer/loop(). Hence synchronous=True (enables StringRef opt).
    synchronous=True,
)
async def transmit_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_BRIDGE_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    addr = await cg.templatable(config[CONF_ADDRESS], args, cg.uint16)
    cg.add(var.set_address(addr))

    fk = await cg.templatable(config[CONF_FLAGS_K], args, cg.uint8)
    cg.add(var.set_flags_k(fk))

    lf = await cg.templatable(config[CONF_LED_FAN], args, cg.uint8)
    cg.add(var.set_led_fan(lf))

    btn = await cg.templatable(config[CONF_BUTTON], args, cg.uint8)
    cg.add(var.set_button(btn))

    rep = await cg.templatable(config[CONF_REPEATS], args, cg.uint8)
    cg.add(var.set_repeats(rep))

    return var
