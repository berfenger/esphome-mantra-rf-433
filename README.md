# Mantra RF 433 MHz Fan — ESPHome Component

An [ESPHome](https://esphome.io) external component that brings Mantra 433 MHz
ceiling fans into Home Assistant. It lets an ESP device act as a replacement
for the original RF remote, giving you full control over the fan and its
built-in light from Home Assistant.

Each fan appears in Home Assistant with:

- **A fan entity** — 6 speeds, reversible direction, and three operating
  modes (Normal / Night / Breeze) selectable directly on the fan card.
- **A light entity** — dimmable LED with adjustable color temperature
  (warm 3000 K to cool 5000 K).

A single ESP device can control multiple fans at once.

## Compatibility

This component works with Mantra ceiling fans controlled by the **R00143** and
**R00149** remotes. The following models use this protocol:

- Infinity
- Flower
- Nemo
- Nemo II
- Nepal II
- Rose
- Aconcagua
- Ceramica
- Kyoto
- Patagonia

> **Note:** This does **not** cover Mantra's 2.4 GHz Bluetooth (BLE) fans —
> those use a completely different system and are not supported.

## What you need

- An ESP32 (or compatible) board running ESPHome.
- A 433 MHz RF receiver and a 433 MHz RF transmitter connected to the board.
  The receiver is used to learn your remote's address and update the fan and light states in Home Assistant using the original remote; the transmitter sends
  commands to the fan.
  A **CC1101 transceiver** is strongly recommended — it handles both receiving
  and transmitting on a single module and gives far better range and
  reliability than the cheap fixed-frequency 433 MHz modules. See
  [`atom_lite_cc1101_example.yaml`](atom_lite_cc1101_example.yaml) for a
  complete CC1101 setup (requires ESPHome 2025.12.0 or newer).
- One or more compatible Mantra fans (see the list above).

## Setup

### 1. Add the component

Point ESPHome at this component in your device's YAML:

```yaml
external_components:
  - source: github://berfenger/esphome-mantra-rf-433
    components: mantra_rf_433
```

### 2. Configure the radios and the bridge

Set up your 433 MHz receiver and transmitter, then add the `mantra_rf_433`
bridge that ties them together:

```yaml
remote_receiver:
  id: rf_rx
  pin: GPIOXX
  filter: 150us
  idle: 12ms           # lower values can duplicate received commands
  tolerance: 25%

remote_transmitter:
  id: rf_tx
  pin: GPIOXX
  carrier_duty_percent: 100%

mantra_rf_433:
  id: rf_bridge
  receiver_id: rf_rx
  transmitter_id: rf_tx
  fans:
    - id: livingroom_fan
      address: 0x1A1B
```

Each entry under `fans:` represents one physical fan. The `address` is the
unique code paired between your original remote and the fan — see
[Finding your fan's address](#finding-your-fans-address) below.

### 3. Add the light and fan entities

For each fan, add a light and a fan platform that point back to it via
`mantra_id`:

```yaml
light:
  - platform: mantra_rf_433
    mantra_id: livingroom_fan
    name: "Living Room LED"

fan:
  - platform: mantra_rf_433
    mantra_id: livingroom_fan
    name: "Living Room Fan"
```

That's it — flash the device and the fan and light will appear in Home
Assistant.

## Finding your fan's address

Every Mantra fan is paired to its remote with a unique 2-byte address. To find
yours, temporarily add an `on_frame` logger to the bridge, flash the device,
then press any button on your original remote near the ESP's receiver:

```yaml
mantra_rf_433:
  id: rf_bridge
  receiver_id: rf_rx
  transmitter_id: rf_tx
  on_frame:
    then:
      - logger.log:
          level: INFO
          tag: "mantra_rx"
          format: "addr=%04X"
          args: ["x.address"]
```

> **Warning:** the message only appears if your `logger:` is configured with a
> log level at least as high as the one used in `on_frame`. The example above
> logs at `INFO`, so make sure your logger allows `INFO` (or more verbose):
>
> ```yaml
> logger:
>   level: INFO
> ```

Watch the ESPHome logs — the `addr=...` value (e.g. `1A1B`) is what you put in
the `address:` field. Once you have it, you can remove the `on_frame` block.

## Multiple fans

To control several fans from one ESP, just add more entries under `fans:` and a
matching light/fan platform for each:

```yaml
mantra_rf_433:
  id: rf_bridge
  receiver_id: rf_rx
  transmitter_id: rf_tx
  fans:
    - id: livingroom_fan
      address: 0x1A1B
    - id: bedroom_fan
      address: 0x2A2B

light:
  - platform: mantra_rf_433
    mantra_id: livingroom_fan
    name: "Living Room LED"
  - platform: mantra_rf_433
    mantra_id: bedroom_fan
    name: "Bedroom LED"

fan:
  - platform: mantra_rf_433
    mantra_id: livingroom_fan
    name: "Living Room Fan"
  - platform: mantra_rf_433
    mantra_id: bedroom_fan
    name: "Bedroom Fan"
```

## Renaming the fan modes

The three operating modes appear on the fan card as **Normal**, **Night**, and
**Breeze** by default. You can rename any of them (for example to translate
them into your language) with `preset_modes`:

```yaml
fan:
  - platform: mantra_rf_433
    mantra_id: livingroom_fan
    name: "Ventilador Salón"
    preset_modes:
      normal: "Normal"
      night: "Nocturno"
      breeze: "Brisa"
```

## Grouping in Home Assistant

If you'd like each fan to show up as its own device in Home Assistant (with the
light and fan grouped together), use ESPHome's standard `devices` feature and
the `device_id` option on each platform. See [`example.yaml`](example.yaml) for
a complete configuration that does this.

## Tip: restoring state after a power cut

If your fan's power is switched by an upstream smart plug or relay, the fan
firmware turns the LED back on after power returns but leaves the motor off.
You can have a Home Assistant automation re-send the last known state a few
seconds after power comes back so the fan returns to how you left it. See the
`api.actions` / `resend_state()` example in [`example.yaml`](example.yaml).

## Full examples

Two complete, heavily commented configurations are included, both covering two
fans, Home Assistant device grouping, localisation, and recovery automations:

- [`example.yaml`](example.yaml) — generic setup with plain 433 MHz
  receiver/transmitter modules.
- [`atom_lite_cc1101_example.yaml`](atom_lite_cc1101_example.yaml) — M5Stack
  ATOM Lite with a CC1101 transceiver (dual data pin), including Wi-Fi, OTA,
  logger, and API sections. Requires ESPHome 2025.12.0 or newer for native
  CC1101 support.
