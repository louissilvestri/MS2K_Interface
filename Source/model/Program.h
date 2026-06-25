// Program — typed view over the MS2000's 254-byte program buffer (TABLE 1/2/3).
//
// Pure C++/STL, no JUCE, so the data model is unit-testable on its own.
// Raw byte/bit access lives here; semantic (display-value) access is layered on
// top in ParameterModel, which knows each parameter's scaling.
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace ms2000 {

constexpr int kProgramSize   = 254;
constexpr int kTimbre1Base   = 38;   // TABLE 1: Timbre 1 at 38..145
constexpr int kTimbreStride  = 108;  // Timbre 2 at 146 (38 + 108)
constexpr int kVocoderBase   = 38;   // TABLE 3 shares the timbre-1 region
constexpr int kNameLen       = 12;

enum class VoiceMode : uint8_t { Single = 0, Layer = 2, Vocoder = 3 };

// A program is just its 254 raw bytes plus typed helpers.
class Program {
public:
    Program() { data_.fill(0); }
    explicit Program(const std::array<uint8_t, kProgramSize>& d) : data_(d) {}

    const std::array<uint8_t, kProgramSize>& bytes() const { return data_; }
    std::array<uint8_t, kProgramSize>&       bytes()       { return data_; }

    // ---- name (bytes 0..11, ASCII) ----------------------------------------
    std::string name() const {
        std::string s;
        for (int i = 0; i < kNameLen; ++i) {
            char c = static_cast<char>(data_[i]);
            if (c == 0) break;
            s.push_back(c);
        }
        while (!s.empty() && s.back() == ' ') s.pop_back(); // names are space-padded
        return s;
    }
    void setName(const std::string& s) {
        for (int i = 0; i < kNameLen; ++i)
            data_[i] = i < (int)s.size() ? static_cast<uint8_t>(s[i]) : ' ';
    }

    // ---- voice mode (byte 16, bits 4-5) -----------------------------------
    VoiceMode voiceMode() const {
        return static_cast<VoiceMode>((data_[16] >> 4) & 0x03);
    }
    void setVoiceMode(VoiceMode m) {
        data_[16] = static_cast<uint8_t>((data_[16] & ~0x30) | ((static_cast<int>(m) & 0x03) << 4));
    }

    // ---- timbre base offset for the two synth timbres ---------------------
    static int timbreBase(int timbre /*0 or 1*/) {
        return kTimbre1Base + timbre * kTimbreStride;
    }

    // ---- generic raw bitfield access --------------------------------------
    // Reads `width` bits at byte `offset`, right-shifted by `shift`.
    int getRaw(int offset, uint8_t shift, uint8_t mask) const {
        return (data_[offset] >> shift) & mask;
    }
    void setRaw(int offset, uint8_t shift, uint8_t mask, int value) {
        const uint8_t clear = static_cast<uint8_t>(~(mask << shift));
        data_[offset] = static_cast<uint8_t>((data_[offset] & clear) |
                        ((static_cast<uint8_t>(value) & mask) << shift));
    }

private:
    std::array<uint8_t, kProgramSize> data_{};
};

} // namespace ms2000
