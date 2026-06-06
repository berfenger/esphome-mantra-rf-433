#pragma once

#include <cstdint>

#include "esphome/core/optional.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace mantra_rf_433 {

// 48-bit frame carried over 433 MHz OOK by Mantra ceiling-fan remotes
// (R00143, R00149, and the wider Chinese OEM family that shares this framing).
//
// This is *not* compatible with Mantra's 2.4 GHz BLE-controlled fans — those
// use a completely different radio and protocol.
//
// Wire layout (6 bytes, MSB first):
//   b0,b1 : address (paired between remote and fan)
//   b2    : flags_k     = (fan_mode<<6) | (reverse<<5) | (K_index & 0x1F)
//                        K_index 4..24 maps to 3000..5000K in 100K steps
//                        fan_mode 0=normal, 1=night (bit6), 2=eco (bit7)
//   b3    : led_fan     = ((led_on<<3) | fan_state) << 4 | (brightness & 0x0F)
//                        fan_state: 0=off, 1..6=speed, 7=eco
//                        brightness 0x2..0xB = 20..100% in 10% steps
//   b4    : button code (which key was pressed, see Button namespace)
//   b5    : checksum = (0x55 - (b0+b1+b2+b3+b4)) & 0xFF
struct MantraRf433Frame {
  uint16_t address;
  uint8_t flags_k;
  uint8_t led_fan;
  uint8_t button;
};

namespace Button {
constexpr uint8_t LED_POWER = 0x01;
constexpr uint8_t FAN_POWER = 0x02;
constexpr uint8_t LED_BRIGHT_UP = 0x03;
constexpr uint8_t LED_BRIGHT_DN = 0x04;
constexpr uint8_t LED_KELVIN_UP = 0x05;
constexpr uint8_t LED_KELVIN_DN = 0x06;
constexpr uint8_t FAN_SPEED_DN = 0x09;
constexpr uint8_t FAN_SPEED_UP = 0x0A;
constexpr uint8_t FAN_REVERSE = 0x0C;
constexpr uint8_t FAN_MODE = 0x0D;
}  // namespace Button

constexpr uint32_t SHORT_US = 340;
constexpr uint32_t LONG_US = 850;
constexpr uint32_t PREAMBLE_US = 4010;
constexpr uint32_t GAP_US = 8020;
// Any pulse longer than this is treated as a frame delimiter (preamble or gap).
constexpr uint32_t SYNC_MIN_US = 2500;
constexpr uint8_t FRAME_BITS = 48;
constexpr uint8_t DEFAULT_REPEATS = 4;

uint8_t compute_checksum(const uint8_t bytes[5]);
void pack_frame(const MantraRf433Frame &frame, uint8_t out[6]);
bool unpack_frame(const uint8_t bytes[6], MantraRf433Frame *out);

void encode(const MantraRf433Frame &frame, remote_base::RemoteTransmitData *dst, uint8_t repeats);
optional<MantraRf433Frame> decode(remote_base::RemoteReceiveData &src);

}  // namespace mantra_rf_433
}  // namespace esphome
