#include "mantra_light.h"

#include <cmath>

#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif

namespace esphome {
namespace mantra_rf_433 {

static const char *const TAG = "mantra_rf_433.light";

// Safety fallback: if neither API nor MQTT connects within this window we
// sync anyway, so the entity becomes controllable even without HA reachable.
static constexpr uint32_t BOOT_SYNC_FALLBACK_MS = 60000;
// Poll interval while waiting for HA to connect.
static constexpr uint32_t BOOT_POLL_INTERVAL_MS = 200;

light::LightTraits MantraLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
  // Device supports 3000K..5000K. ESPHome wants mireds; mireds = 1e6 / kelvin.
  traits.set_min_mireds(1000000.0f / 5000.0f);  // ~200
  traits.set_max_mireds(1000000.0f / 3000.0f);  // ~333.3
  return traits;
}

void MantraLight::setup_state(light::LightState *state) {
  this->light_state_ = state;
  if (this->parent_ != nullptr) {
    this->parent_->add_on_state_callback([this]() { this->push_remote_state_(); });
    this->start_boot_sync_();
  }
}

bool MantraLight::ha_connected_() const {
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

void MantraLight::start_boot_sync_() {
  // Poll once every BOOT_POLL_INTERVAL_MS until HA is reachable, or the
  // safety fallback elapses. Then seed the entity from the device's
  // persisted state and release the write_state guard.
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

void MantraLight::write_state(light::LightState *state) {
  if (this->parent_ == nullptr)
    return;
  // Skip RF dispatch until HA is up and we've seeded the entity. See
  // start_boot_sync_() for why.
  if (!this->boot_synced_)
    return;

  // Read remote_values (the final target HA requested), NOT current_values
  // (the live-animated value during a software transition). The device has a
  // discrete brightness/CT grid and accepts absolute byte-3 values directly,
  // so we want to dispatch one frame at the target the moment the request
  // comes in. Reading current_values made us snap to whatever the transition
  // was midway through (e.g. 40% on a 30%->80% ramp), dispatch that, and
  // then push_remote_state_() below re-anchored remote_values at 40%, so the
  // transition never reached the user's actual target.
  auto values = state->remote_values;
  bool desired_on = values.is_on();
  const auto &dev = this->parent_->state();

  if (desired_on != dev.led_on) {
    this->parent_->set_led_power(desired_on);
  }
  if (!desired_on)
    return;

  // Brightness 0..1 -> snap to nearest 10% step in [10..100]
  float bright = values.get_brightness();
  int pct = static_cast<int>(roundf(bright * 100.0f));
  pct = ((pct + 5) / 10) * 10;
  if (pct < 10)
    pct = 10;
  if (pct > 100)
    pct = 100;
  uint8_t desired_step = static_cast<uint8_t>((pct / 10) + 1);  // 10% -> 0x2, 100% -> 0xB
  if (desired_step != dev.led_brightness) {
    this->parent_->set_led_brightness(desired_step);
  }

  // Color temperature in mireds -> kelvin, snap to nearest 100 K in [3000..5000]
  float ct_mired = values.get_color_temperature();
  if (ct_mired > 0) {
    int kelvin = static_cast<int>(roundf(1000000.0f / ct_mired));
    kelvin = ((kelvin + 50) / 100) * 100;
    if (kelvin < 3000)
      kelvin = 3000;
    if (kelvin > 5000)
      kelvin = 5000;
    uint8_t desired_ki = static_cast<uint8_t>((kelvin - 3000) / 100 + 4);
    if (desired_ki != dev.led_kelvin) {
      this->parent_->set_led_kelvin(desired_ki);
    }
  }
}

void MantraLight::push_remote_state_() {
  if (this->light_state_ == nullptr || this->parent_ == nullptr)
    return;

  const auto &dev = this->parent_->state();
  auto call = this->light_state_->make_call();
  call.set_state(dev.led_on);
  if (dev.led_on) {
    float bright = (dev.led_brightness - 1) * 0.1f;
    if (bright > 1.0f)
      bright = 1.0f;
    if (bright < 0.0f)
      bright = 0.0f;
    call.set_brightness(bright);
    int kelvin = (dev.led_kelvin - 4) * 100 + 3000;
    call.set_color_temperature(1000000.0f / static_cast<float>(kelvin));
  }
  call.set_transition_length(0);
  call.perform();
}

}  // namespace mantra_rf_433
}  // namespace esphome
