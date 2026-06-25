// MidiMessages — builds the raw MIDI byte sequences for real-time control.
//
// Pure C++/STL (no JUCE) so the message logic is unit-testable. The JUCE
// MidiEngine only handles port I/O and hands these bytes to the OS.
#pragma once

#include "../model/ParameterModel.h"
#include <cstdint>
#include <vector>

namespace ms2000 {

using Bytes = std::vector<uint8_t>;

// Single CC: Bn cc vv.
Bytes buildCC(uint8_t channel, uint8_t cc, uint8_t value);

// NRPN: Bn 63 msb, Bn 62 lsb, Bn 06 data (Data Entry MSB).
Bytes buildNRPN(uint8_t channel, uint8_t msb, uint8_t lsb, uint8_t data);

// What to transmit for a real-time edit of `spec` to `rawValue`.
// Returns the CC sequence when the param has an assigned CC, otherwise an
// empty vector (caller falls back to a full Current-Program SysEx dump, or an
// explicit NRPN via buildNRPN for the arp/patch-routing/vocoder-band params).
Bytes realtimeForParam(const ParamSpec& spec, int rawValue, uint8_t channel);

} // namespace ms2000
