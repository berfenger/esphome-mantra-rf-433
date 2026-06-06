#pragma once

#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"

#include "protocol.h"

namespace esphome {
namespace mantra_rf_433 {

// Anything that wants to receive frames from the bridge implements this.
// Per-fan devices in device/fan_device.{h,cpp} are the primary users.
class MantraRf433DeviceBase {
 public:
  virtual ~MantraRf433DeviceBase() = default;
  virtual uint16_t get_address() const = 0;
  virtual void handle_frame(const MantraRf433Frame &frame) = 0;
};

class MantraRf433Trigger : public Trigger<MantraRf433Frame> {};

class MantraRf433Bridge : public Component, public remote_base::RemoteReceiverListener {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_receiver(remote_base::RemoteReceiverBase *recv) { receiver_ = recv; }
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *tx) { transmitter_ = tx; }

  void register_device(MantraRf433DeviceBase *dev) { devices_.push_back(dev); }
  void register_trigger(MantraRf433Trigger *t) { triggers_.push_back(t); }

  void transmit(const MantraRf433Frame &frame, uint8_t repeats);

 protected:
  bool on_receive(remote_base::RemoteReceiveData src) override;

  remote_base::RemoteReceiverBase *receiver_{nullptr};
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  std::vector<MantraRf433DeviceBase *> devices_;
  std::vector<MantraRf433Trigger *> triggers_;
};

template<typename... Ts> class TransmitMantraRf433Action : public Action<Ts...> {
 public:
  explicit TransmitMantraRf433Action(MantraRf433Bridge *bridge) : bridge_(bridge) {}

  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint8_t, flags_k)
  TEMPLATABLE_VALUE(uint8_t, led_fan)
  TEMPLATABLE_VALUE(uint8_t, button)
  TEMPLATABLE_VALUE(uint8_t, repeats)

  void play(Ts... x) override {
    MantraRf433Frame f;
    f.address = this->address_.value(x...);
    f.flags_k = this->flags_k_.value(x...);
    f.led_fan = this->led_fan_.value(x...);
    f.button = this->button_.value(x...);
    uint8_t r = this->repeats_.has_value() ? this->repeats_.value(x...) : DEFAULT_REPEATS;
    this->bridge_->transmit(f, r);
  }

 protected:
  MantraRf433Bridge *bridge_;
};

}  // namespace mantra_rf_433
}  // namespace esphome
