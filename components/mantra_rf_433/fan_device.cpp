#include "fan_device.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mantra_rf_433 {

static const char *const TAG = "mantra_rf_433.device";

void MantraRf433Device::setup() {
  // Per-fan persistence keyed by RF address so two devices don't clash.
  uint32_t hash = 0x6D724644u ^ static_cast<uint32_t>(address_);  // "mrFD" ^ addr
  pref_ = global_preferences->make_preference<DeviceState>(hash);
  pref_.load(&state_);

  if (bridge_ != nullptr)
    bridge_->register_device(this);
}

void MantraRf433Device::dump_config() {
  ESP_LOGCONFIG(TAG, "Mantra RF R00143 device:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%04X", address_);
  ESP_LOGCONFIG(TAG, "  Repeats: %u", static_cast<unsigned>(repeats_));
  ESP_LOGCONFIG(TAG, "  Bridge:  %s", bridge_ ? "linked" : "MISSING");
  ESP_LOGCONFIG(TAG, "  Restored: led=%d br=%X K=%u  fan=%d sp=%u mode=%u rev=%d", state_.led_on,
                state_.led_brightness, static_cast<unsigned>(state_.led_kelvin), state_.fan_on,
                static_cast<unsigned>(state_.fan_speed), static_cast<unsigned>(state_.fan_mode),
                static_cast<int>(state_.fan_reverse));
}

void MantraRf433Device::handle_frame(const MantraRf433Frame &frame) {
  // Mirror the decoding of `${fan_id}_rx_decode` in the old YAML pkg.
  state_.fan_reverse = (frame.flags_k & 0x20) != 0 ? 1 : 0;
  if (frame.flags_k & 0x80)
    state_.fan_mode = 2;  // breeze
  else if (frame.flags_k & 0x40)
    state_.fan_mode = 1;  // night
  else
    state_.fan_mode = 0;
  uint8_t ki = frame.flags_k & 0x1F;
  if (ki >= 4 && ki <= 24)
    state_.led_kelvin = ki;

  uint8_t hn = (frame.led_fan >> 4) & 0x0F;
  uint8_t br = frame.led_fan & 0x0F;
  uint8_t fan_st = hn & 0x7;

  if (br >= 0x2 && br <= 0xB)
    state_.led_brightness = br;
  state_.led_on = (hn & 0x8) != 0;

  if (fan_st == 0) {
    state_.fan_on = false;
  } else if (fan_st == 7) {
    state_.fan_on = true;
    state_.fan_mode = 2;
  } else {
    state_.fan_on = true;
    state_.fan_speed = fan_st;
  }

  ESP_LOGD(TAG, "[%04X] RX b2=%02X b3=%02X btn=%02X -> led=%d br=%X K=%u fan=%d sp=%u mode=%u rev=%d", address_,
           frame.flags_k, frame.led_fan, frame.button, state_.led_on, state_.led_brightness,
           static_cast<unsigned>(state_.led_kelvin), state_.fan_on, static_cast<unsigned>(state_.fan_speed),
           static_cast<unsigned>(state_.fan_mode), static_cast<int>(state_.fan_reverse));

  save_();
  notify_();
}

MantraRf433Frame MantraRf433Device::build_frame_(uint8_t button) const {
  MantraRf433Frame f{};
  f.address = address_;

  uint8_t b2 = state_.led_kelvin & 0x1F;
  if (state_.fan_reverse)
    b2 |= 0x20;
  if (state_.fan_mode == 1)
    b2 |= 0x40;
  else if (state_.fan_mode == 2)
    b2 |= 0x80;
  f.flags_k = b2;

  uint8_t lbr = state_.led_brightness;
  if (lbr < 0x2)
    lbr = 0x2;
  if (lbr > 0xB)
    lbr = 0xB;

  uint8_t fan_st;
  if (!state_.fan_on)
    fan_st = 0;
  else if (state_.fan_mode == 2)
    fan_st = 7;
  else
    fan_st = state_.fan_speed & 0x07;
  uint8_t led_bit = state_.led_on ? 0x8 : 0;
  f.led_fan = static_cast<uint8_t>(((led_bit | fan_st) << 4) | lbr);

  f.button = button;
  return f;
}

void MantraRf433Device::tx_(uint8_t button) {
  if (bridge_ == nullptr) {
    ESP_LOGW(TAG, "[%04X] TX skipped: no bridge", address_);
    return;
  }
  auto frame = build_frame_(button);
  bridge_->transmit(frame, repeats_);
  save_();
  notify_();
}

void MantraRf433Device::set_led_power(bool on) {
  if (state_.led_on == on)
    return;
  state_.led_on = on;
  tx_(Button::LED_POWER);
}

void MantraRf433Device::set_led_brightness(uint8_t step) {
  if (step < 0x2)
    step = 0x2;
  if (step > 0xB)
    step = 0xB;
  if (step == state_.led_brightness && state_.led_on)
    return;
  uint8_t prev = state_.led_brightness;
  state_.led_brightness = step;
  state_.led_on = true;
  tx_(step >= prev ? Button::LED_BRIGHT_UP : Button::LED_BRIGHT_DN);
}

void MantraRf433Device::set_led_kelvin(uint8_t k_index) {
  if (k_index < 4)
    k_index = 4;
  if (k_index > 24)
    k_index = 24;
  if (k_index == state_.led_kelvin && state_.led_on)
    return;
  uint8_t prev = state_.led_kelvin;
  state_.led_kelvin = k_index;
  state_.led_on = true;
  tx_(k_index >= prev ? Button::LED_KELVIN_UP : Button::LED_KELVIN_DN);
}

void MantraRf433Device::set_fan_power(bool on) {
  if (state_.fan_on == on)
    return;
  state_.fan_on = on;
  tx_(Button::FAN_POWER);
}

void MantraRf433Device::set_fan_speed(uint8_t speed) {
  if (speed > 6)
    speed = 6;
  if (speed == 0) {
    if (!state_.fan_on)
      return;
    state_.fan_on = false;
    tx_(Button::FAN_POWER);
    return;
  }
  if (state_.fan_on && speed == state_.fan_speed && state_.fan_mode == 0)
    return;
  uint8_t prev = state_.fan_speed;
  state_.fan_speed = speed;
  state_.fan_on = true;
  state_.fan_mode = 0;
  tx_(speed >= prev ? Button::FAN_SPEED_UP : Button::FAN_SPEED_DN);
}

void MantraRf433Device::set_fan_reverse(bool reverse) {
  uint8_t want = reverse ? 1 : 0;
  if (state_.fan_reverse == want)
    return;
  state_.fan_reverse = want;
  tx_(Button::FAN_REVERSE);
}

void MantraRf433Device::set_fan_mode(uint8_t mode) {
  if (mode > 2)
    mode = 2;
  if (state_.fan_mode == mode)
    return;
  state_.fan_mode = mode;
  if (mode != 0)
    state_.fan_on = true;
  if (mode == 1)
    state_.fan_speed = 1;  // remote's "night" preset
  tx_(Button::FAN_MODE);
}

void MantraRf433Device::resend_state() {
  if (bridge_ == nullptr) {
    ESP_LOGW(TAG, "[%04X] resend_state skipped: no bridge", address_);
    return;
  }
  ESP_LOGD(TAG, "[%04X] resend_state  led=%d br=%X K=%u  fan=%d sp=%u mode=%u rev=%d", address_, state_.led_on,
           state_.led_brightness, static_cast<unsigned>(state_.led_kelvin), state_.fan_on,
           static_cast<unsigned>(state_.fan_speed), static_cast<unsigned>(state_.fan_mode),
           static_cast<int>(state_.fan_reverse));
  // tx_() builds the frame from current state_ and transmits it without
  // mutating any field, so calling it here is a pure "send what you have".
  // We pick FAN_POWER as the button code because the fan firmware honours the
  // absolute b3.fan_state nibble carried alongside it.
  tx_(Button::FAN_POWER);
}

void MantraRf433Device::save_() { pref_.save(&state_); }

void MantraRf433Device::notify_() { state_callback_.call(); }

}  // namespace mantra_rf_433
}  // namespace esphome
