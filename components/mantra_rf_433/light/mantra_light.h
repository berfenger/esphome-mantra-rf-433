#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/core/component.h"

#include "../fan_device.h"

namespace esphome {
namespace mantra_rf_433 {

class MantraLight : public Component, public light::LightOutput {
 public:
  void set_parent(MantraRf433Device *parent) { parent_ = parent; }

  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;

 protected:
  void push_remote_state_();
  void start_boot_sync_();
  bool ha_connected_() const;

  MantraRf433Device *parent_{nullptr};
  light::LightState *light_state_{nullptr};
  // Becomes true once HA (API or MQTT) is connected and we've seeded the
  // entity from the device's persisted state. Until then write_state skips
  // RF dispatch so the LightState's restore_mode doesn't trigger a phantom
  // "off" frame on every boot.
  bool boot_synced_{false};
};

}  // namespace mantra_rf_433
}  // namespace esphome
