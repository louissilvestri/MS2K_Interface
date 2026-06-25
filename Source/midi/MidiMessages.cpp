#include "MidiMessages.h"

namespace ms2000 {

static uint8_t statusCC(uint8_t ch) { return static_cast<uint8_t>(0xB0 | (ch & 0x0F)); }

Bytes buildCC(uint8_t channel, uint8_t cc, uint8_t value) {
    return { statusCC(channel), static_cast<uint8_t>(cc & 0x7F),
             static_cast<uint8_t>(value & 0x7F) };
}

Bytes buildNRPN(uint8_t channel, uint8_t msb, uint8_t lsb, uint8_t data) {
    const uint8_t s = statusCC(channel);
    return { s, 0x63, static_cast<uint8_t>(msb & 0x7F),
             s, 0x62, static_cast<uint8_t>(lsb & 0x7F),
             s, 0x06, static_cast<uint8_t>(data & 0x7F) };
}

Bytes realtimeForParam(const ParamSpec& spec, int rawValue, uint8_t channel) {
    if (spec.cc < 0) return {}; // no CC -> caller uses NRPN or a full dump
    return buildCC(channel, static_cast<uint8_t>(spec.cc),
                   static_cast<uint8_t>(ccValueForRaw(spec, rawValue)));
}

} // namespace ms2000
