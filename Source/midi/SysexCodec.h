// SysexCodec — Korg MS2000 SysEx pack/unpack and message framing.
//
// Pure C++/STL, no JUCE dependency, so it can be unit-tested standalone.
// See docs/MS2000_param_map.md for the byte-level spec this implements.
#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace ms2000 {

using Bytes = std::vector<uint8_t>;

// ---- Korg 7->8 bit packing (NOTE 5 of the MIDI implementation) -------------
// Every group of up to 7 data bytes is transmitted as: one lead byte holding
// the seven stripped MSBs (bit i = MSB of data[i]), followed by the low-7-bit
// data bytes. 254 data bytes -> 291 transmitted bytes.
Bytes pack7to8(const Bytes& data);
Bytes unpack8to7(const Bytes& packed);

// ---- Message framing -------------------------------------------------------
constexpr uint8_t kKorgId   = 0x42;
constexpr uint8_t kModelId  = 0x58; // MS2000 series
constexpr uint8_t kSox      = 0xF0;
constexpr uint8_t kEox      = 0xF7;

enum class Func : uint8_t {
    CurrentProgramDumpRequest = 0x10,
    ProgramDumpRequest        = 0x1C,
    GlobalDumpRequest         = 0x0E,
    AllDumpRequest            = 0x0F,
    ProgramWriteRequest       = 0x11,
    CurrentProgramDump        = 0x40,
    ProgramDump               = 0x4C,
    GlobalDump                = 0x51,
    AllDump                   = 0x50,
    LoadCompleted             = 0x23,
    LoadError                 = 0x24,
    DataFormatError           = 0x26,
    WriteCompleted            = 0x21,
    WriteError                = 0x22,
};

constexpr size_t kProgramBytes = 254; // unpacked single program (TABLE 1)

// Build a header-only request (e.g. CurrentProgramDumpRequest).
Bytes makeRequest(Func f, uint8_t channel);

// Build a "write current program to slot" request (Func 0x11).
Bytes makeProgramWriteRequest(uint8_t channel, uint8_t programNo);

// Wrap a 254-byte program in a Current-Program (0x40) dump.
Bytes makeProgramDump(const Bytes& program254, uint8_t channel,
                      Func f = Func::CurrentProgramDump);

// Wrap N×254-byte programs in a Program Data Dump (0x4C) or All Data Dump (0x50).
// All programs are concatenated then 7->8 packed as a single block.
Bytes makeBankDump(const std::vector<Bytes>& programs, uint8_t channel,
                   Func f = Func::ProgramDump);

// Parse a Current-Program dump (0x40). Returns the 254-byte program, else nullopt.
std::optional<Bytes> parseProgramDump(const Bytes& sysex);

// Parse a bank dump (0x4C Program Data, or 0x50 All Data). Returns each 254-byte
// program (128 for a full bank; trailing global bytes in 0x50 are ignored).
std::optional<std::vector<Bytes>> parseBankDump(const Bytes& sysex);

// True if msg is a well-formed MS2000 SysEx (F0 42 3g 58 ... F7).
bool isMs2000Sysex(const Bytes& sysex);

} // namespace ms2000
