#include "protocol.h"

#include <string>
#include <vector>

namespace esphome {
namespace mantra_rf_433 {

uint8_t compute_checksum(const uint8_t bytes[5]) {
  uint16_t sum = 0;
  for (int i = 0; i < 5; i++)
    sum += bytes[i];
  return static_cast<uint8_t>((0x55 - sum) & 0xFF);
}

void pack_frame(const MantraRf433Frame &frame, uint8_t out[6]) {
  out[0] = (frame.address >> 8) & 0xFF;
  out[1] = frame.address & 0xFF;
  out[2] = frame.flags_k;
  out[3] = frame.led_fan;
  out[4] = frame.button;
  out[5] = compute_checksum(out);
}

bool unpack_frame(const uint8_t bytes[6], MantraRf433Frame *out) {
  if (bytes[5] != compute_checksum(bytes))
    return false;
  out->address = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
  out->flags_k = bytes[2];
  out->led_fan = bytes[3];
  out->button = bytes[4];
  return true;
}

void encode(const MantraRf433Frame &frame, remote_base::RemoteTransmitData *dst, uint8_t repeats) {
  if (repeats == 0)
    repeats = 1;

  uint8_t b[6];
  pack_frame(frame, b);

  uint64_t value = 0;
  for (int i = 0; i < 6; i++)
    value = (value << 8) | b[i];

  dst->set_carrier_frequency(0);
  // 2 entries for preamble + 2 per bit, per repeat, plus a gap between repeats.
  dst->reserve(static_cast<uint32_t>(repeats) * (2 + 2 * FRAME_BITS + 1));

  for (uint8_t r = 0; r < repeats; r++) {
    dst->mark(PREAMBLE_US);
    dst->space(PREAMBLE_US);
    for (int k = FRAME_BITS - 1; k >= 0; k--) {
      bool one = (value >> k) & 1ULL;
      dst->item(one ? LONG_US : SHORT_US, one ? SHORT_US : LONG_US);
    }
    if (r + 1 < repeats)
      dst->space(GAP_US);
  }
}

optional<MantraRf433Frame> decode(remote_base::RemoteReceiveData &src) {
  const auto &x = src.get_raw_data();

  std::vector<std::string> frames;
  std::string cur;
  cur.reserve(FRAME_BITS);

  size_t i = (!x.empty() && x[0] < 0) ? 1 : 0;
  auto flush = [&]() {
    if (cur.length() == FRAME_BITS)
      frames.push_back(cur);
    cur.clear();
  };

  while (i + 1 < x.size()) {
    int32_t hi = x[i];
    int32_t lo = -x[i + 1];
    if (hi <= 0 || lo <= 0) {
      i++;
      continue;
    }
    if (static_cast<uint32_t>(hi) > SYNC_MIN_US || static_cast<uint32_t>(lo) > SYNC_MIN_US) {
      flush();
      i += 2;
      continue;
    }
    cur += (hi > lo) ? '1' : '0';
    if (cur.length() > FRAME_BITS)
      cur.clear();  // overshoot — burst is corrupt, drop
    i += 2;
  }
  flush();

  if (frames.empty())
    return {};

  // All repeats inside a single burst must agree, otherwise we got noise.
  for (size_t k = 1; k < frames.size(); k++) {
    if (frames[k] != frames[0])
      return {};
  }

  uint8_t b[6] = {0};
  for (int bi = 0; bi < 6; bi++) {
    uint8_t v = 0;
    for (int bj = 0; bj < 8; bj++)
      v = (v << 1) | (frames[0][bi * 8 + bj] == '1' ? 1 : 0);
    b[bi] = v;
  }

  MantraRf433Frame out;
  if (!unpack_frame(b, &out))
    return {};
  return out;
}

}  // namespace mantra_rf_433
}  // namespace esphome
