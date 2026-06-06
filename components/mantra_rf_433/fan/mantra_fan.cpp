#include "mantra_fan.h"

#include <cstring>

#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif

namespace esphome {
namespace mantra_rf_433 {

static const char *const TAG = "mantra_rf_433.fan";

static constexpr uint32_t BOOT_SYNC_FALLBACK_MS = 60000;
static constexpr uint32_t BOOT_POLL_INTERVAL_MS = 200;

void MantraFan::setup() {
  // Declare preset modes on the Fan itself (post-2026.11 API). Labels come
  // from std::string members so the c_str() pointers handed in here stay
  // valid for the Fan's lifetime.
  this->set_supported_preset_modes(
      std::vector<const char *>{preset_normal_.c_str(), preset_night_.c_str(), preset_breeze_.c_str()});

  if (this->parent_ == nullptr)
    return;
  this->parent_->add_on_state_callback([this]() { this->push_remote_state_(); });
  this->start_boot_sync_();
}

bool MantraFan::ha_connected_() const {
#ifdef USE_API
  if (api::global_api_server != nullptr && api::global_api_server->is_connected())
    return true;
#endif
#ifdef USE_MQTT
  if (mqtt::global_mqtt_client != nullptr && mqtt::global_mqtt_client->is_connected())
    return true;
#endif
  return false;
}

void MantraFan::start_boot_sync_() {
  // Wait for HA to be reachable (API or MQTT) before seeding the entity
  // and releasing the control() guard. If neither connects in time we sync
  // anyway so the user isn't locked out forever.
  this->set_interval("boot_sync", BOOT_POLL_INTERVAL_MS, [this]() {
    if (!this->ha_connected_() && millis() < BOOT_SYNC_FALLBACK_MS)
      return;
    ESP_LOGD(TAG, "boot sync at %u ms (ha_connected=%d)", static_cast<unsigned>(millis()),
             static_cast<int>(this->ha_connected_()));
    this->push_remote_state_();
    this->boot_synced_ = true;
    this->cancel_interval("boot_sync");
  });
}

fan::FanTraits MantraFan::get_traits() {
  // (oscillation=false, speed=true, direction=true, speed_count=6)
  fan::FanTraits traits(false, true, true, 6);
  // FanCall validates preset_mode via traits.find_preset_mode(), so wire the
  // Fan-owned preset modes list into the returned traits — otherwise the
  // validation sees an empty list and logs "Preset mode 'X' not supported".
  this->wire_preset_modes_(traits);
  return traits;
}

void MantraFan::control(const fan::FanCall &call) {
  if (this->parent_ == nullptr)
    return;
  // Skip control until HA is reachable and we've seeded the entity from
  // the device. Otherwise the Fan's own restore_mode would fire phantom
  // off-state frames here.
  if (!this->boot_synced_)
    return;

  // Resolve the user-requested target from the call. Fields not set in this
  // call fall back to current entity state.
  bool want_on = call.get_state().has_value() ? *call.get_state() : this->state;
  int want_speed = call.get_speed().has_value() ? *call.get_speed() : this->speed;
  fan::FanDirection want_dir =
      call.get_direction().has_value() ? *call.get_direction() : this->direction;

  uint8_t want_mode = 0;
  bool mode_changed = call.has_preset_mode();
  const char *want_mode_label = preset_normal_.c_str();
  if (mode_changed) {
    const char *pm = call.get_preset_mode();
    if (pm != nullptr) {
      if (std::strcmp(pm, preset_night_.c_str()) == 0) {
        want_mode = 1;
        want_mode_label = preset_night_.c_str();
      } else if (std::strcmp(pm, preset_breeze_.c_str()) == 0) {
        want_mode = 2;
        want_mode_label = preset_breeze_.c_str();
      } else {
        want_mode = 0;
        want_mode_label = preset_normal_.c_str();
      }
    }
  }

  // FanCall::perform() only validates and calls control() — the subclass is
  // responsible for writing back to the public Fan fields and publishing.
  this->state = want_on;
  this->speed = want_speed;
  this->direction = want_dir;
  if (mode_changed) {
    this->set_preset_mode_(want_mode_label, std::strlen(want_mode_label));
  }
  this->publish_state();

  // Now reconcile with the device. Each setter dedups against device state
  // and is responsible for the actual RF transmission.
  const auto &dev = this->parent_->state();

  bool want_rev = (want_dir == fan::FanDirection::REVERSE);
  if (want_rev != static_cast<bool>(dev.fan_reverse)) {
    this->parent_->set_fan_reverse(want_rev);
  }

  if (!want_on) {
    if (dev.fan_on)
      this->parent_->set_fan_power(false);
    return;
  }

  // Preset mode change (set_fan_mode dedups; forces fan_on for non-Normal,
  // forces speed=1 for Night).
  if (mode_changed) {
    this->parent_->set_fan_mode(want_mode);
  }

  // Speed change forces Normal mode (set_fan_speed dedups).
  if (call.get_speed().has_value()) {
    int spd = want_speed > 0 ? want_speed : dev.fan_speed;
    this->parent_->set_fan_speed(static_cast<uint8_t>(spd));
  } else if (!dev.fan_on) {
    this->parent_->set_fan_power(true);
  }
}

void MantraFan::push_remote_state_() {
  if (this->parent_ == nullptr)
    return;
  const auto &dev = this->parent_->state();

  this->state = dev.fan_on;
  this->speed = dev.fan_speed;
  this->direction = dev.fan_reverse ? fan::FanDirection::REVERSE : fan::FanDirection::FORWARD;

  // preset_mode_ is private on Fan, but the protected setter is accessible
  // here and stores the validated pointer in the Fan's own list.
  const char *m = preset_normal_.c_str();
  if (dev.fan_mode == 1)
    m = preset_night_.c_str();
  else if (dev.fan_mode == 2)
    m = preset_breeze_.c_str();
  this->set_preset_mode_(m, std::strlen(m));

  this->publish_state();
}

}  // namespace mantra_rf_433
}  // namespace esphome
