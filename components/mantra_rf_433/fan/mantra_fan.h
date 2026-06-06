#pragma once

#include <string>

#include "esphome/components/fan/fan.h"
#include "esphome/core/component.h"

#include "../fan_device.h"

namespace esphome {
namespace mantra_rf_433 {

class MantraFan : public Component, public fan::Fan {
 public:
  void set_parent(MantraRf433Device *parent) { parent_ = parent; }

  // Per-mode display labels. Defaults are English; override from YAML for
  // localisation. Pointers handed to fan::Fan::set_supported_preset_modes()
  // must outlive the Fan, so the strings are stored as members.
  void set_preset_normal(const std::string &s) { preset_normal_ = s; }
  void set_preset_night(const std::string &s) { preset_night_ = s; }
  void set_preset_breeze(const std::string &s) { preset_breeze_ = s; }

  void setup() override;
  fan::FanTraits get_traits() override;

 protected:
  void control(const fan::FanCall &call) override;
  void push_remote_state_();
  void start_boot_sync_();
  bool ha_connected_() const;

  MantraRf433Device *parent_{nullptr};
  std::string preset_normal_{"Normal"};
  std::string preset_night_{"Night"};
  std::string preset_breeze_{"Breeze"};
  bool boot_synced_{false};
};

}  // namespace mantra_rf_433
}  // namespace esphome
