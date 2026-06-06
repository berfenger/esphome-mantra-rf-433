#include "bridge.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mantra_rf_433 {

static const char *const TAG = "mantra_rf_433";

void MantraRf433Bridge::setup() {
  if (this->receiver_ != nullptr)
    this->receiver_->register_listener(this);
}

void MantraRf433Bridge::dump_config() {
  ESP_LOGCONFIG(TAG, "Mantra RF R00143 bridge:");
  ESP_LOGCONFIG(TAG, "  Receiver:    %s", this->receiver_ ? "configured" : "MISSING");
  ESP_LOGCONFIG(TAG, "  Transmitter: %s", this->transmitter_ ? "configured" : "MISSING");
  ESP_LOGCONFIG(TAG, "  Devices:     %u", static_cast<unsigned>(this->devices_.size()));
  ESP_LOGCONFIG(TAG, "  Triggers:    %u", static_cast<unsigned>(this->triggers_.size()));
}

bool MantraRf433Bridge::on_receive(remote_base::RemoteReceiveData src) {
  auto frame_opt = decode(src);
  if (!frame_opt.has_value())
    return false;
  const auto &frame = *frame_opt;

  ESP_LOGD(TAG, "RX addr=%04X b2=%02X b3=%02X btn=%02X", frame.address, frame.flags_k, frame.led_fan, frame.button);

  for (auto *t : this->triggers_)
    t->trigger(frame);

  bool claimed = false;
  for (auto *dev : this->devices_) {
    if (dev->get_address() == frame.address) {
      dev->handle_frame(frame);
      claimed = true;
    }
  }
  return claimed;
}

void MantraRf433Bridge::transmit(const MantraRf433Frame &frame, uint8_t repeats) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "TX called with no transmitter configured");
    return;
  }
  if (repeats == 0)
    repeats = DEFAULT_REPEATS;

  auto call = this->transmitter_->transmit();
  encode(frame, call.get_data(), repeats);
  call.set_send_times(1);
  call.set_send_wait(GAP_US);
  call.perform();

  ESP_LOGD(TAG, "TX addr=%04X b2=%02X b3=%02X btn=%02X x%u", frame.address, frame.flags_k, frame.led_fan, frame.button,
           repeats);
}

}  // namespace mantra_rf_433
}  // namespace esphome
