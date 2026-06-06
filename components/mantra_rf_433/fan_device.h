#pragma once

#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include "bridge.h"
#include "protocol.h"

namespace esphome {
namespace mantra_rf_433 {

// Persisted state for a single Mantra fan. Layout is stable on the flash.
struct DeviceState {
  bool led_on{false};
  uint8_t led_brightness{6};  // 0x2..0xB = 20..100% (0x6 = 60%)
  uint8_t led_kelvin{4};      // 4..24 = 3000..5000K  (4 = 3000K)
  bool fan_on{false};
  uint8_t fan_speed{3};       // 1..6
  uint8_t fan_reverse{0};
  uint8_t fan_mode{0};        // 0=normal, 1=night, 2=eco
} __attribute__((packed));

class MantraRf433Device : public Component, public MantraRf433DeviceBase {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_bridge(MantraRf433Bridge *bridge) { bridge_ = bridge; }
  void set_address(uint16_t address) { address_ = address; }
  void set_repeats(uint8_t repeats) { repeats_ = repeats; }

  // MantraRf433DeviceBase
  uint16_t get_address() const override { return address_; }
  void handle_frame(const MantraRf433Frame &frame) override;

  const DeviceState &state() const { return state_; }

  // Setters from entity platforms. Each dedups against current state and only
  // emits an RF burst if the device-visible state actually changes.
  void set_led_power(bool on);
  void set_led_brightness(uint8_t step);   // 0x2..0xB
  void set_led_kelvin(uint8_t k_index);    // 4..24
  void set_fan_power(bool on);
  void set_fan_speed(uint8_t speed);       // 1..6, 0 = power off
  void set_fan_reverse(bool reverse);
  void set_fan_mode(uint8_t mode);         // 0..2

  // Re-transmit the current persisted state to the fan, regardless of what
  // the fan's own firmware thinks. Intended for recovery after an upstream
  // AC switch cuts power: the fan boots with the LED state
  // restored but the fan motor off. Calling this from an HA automation
  // shortly after power returns makes the fan match the saved state. The
  // whole state is carried in bytes 2-3 of one frame so a single
  // transmission is sufficient.
  void resend_state();

  void add_on_state_callback(std::function<void()> &&cb) { state_callback_.add(std::move(cb)); }

 protected:
  MantraRf433Frame build_frame_(uint8_t button) const;
  void tx_(uint8_t button);
  void save_();
  void notify_();

  MantraRf433Bridge *bridge_{nullptr};
  uint16_t address_{0};
  uint8_t repeats_{DEFAULT_REPEATS};

  DeviceState state_{};
  ESPPreferenceObject pref_;
  CallbackManager<void()> state_callback_;
};

}  // namespace mantra_rf_433
}  // namespace esphome
