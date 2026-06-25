#include "SysexCodec.h"

namespace ms2000 {

Bytes pack7to8(const Bytes& data) {
    Bytes out;
    out.reserve(data.size() + data.size() / 7 + 2);
    for (size_t i = 0; i < data.size(); i += 7) {
        const size_t n = std::min<size_t>(7, data.size() - i);
        uint8_t lead = 0;
        for (size_t j = 0; j < n; ++j)
            lead |= static_cast<uint8_t>((data[i + j] >> 7) & 1) << j;
        out.push_back(lead);
        for (size_t j = 0; j < n; ++j)
            out.push_back(static_cast<uint8_t>(data[i + j] & 0x7F));
    }
    return out;
}

Bytes unpack8to7(const Bytes& packed) {
    Bytes out;
    out.reserve(packed.size());
    for (size_t i = 0; i < packed.size();) {
        const uint8_t lead = packed[i++];
        for (size_t j = 0; j < 7 && i < packed.size(); ++j, ++i) {
            const uint8_t hi = static_cast<uint8_t>((lead >> j) & 1) << 7;
            out.push_back(static_cast<uint8_t>(hi | (packed[i] & 0x7F)));
        }
    }
    return out;
}

static Bytes header(uint8_t channel) {
    return { kSox, kKorgId, static_cast<uint8_t>(0x30 | (channel & 0x0F)), kModelId };
}

Bytes makeRequest(Func f, uint8_t channel) {
    Bytes m = header(channel);
    m.push_back(static_cast<uint8_t>(f));
    m.push_back(kEox);
    return m;
}

Bytes makeProgramWriteRequest(uint8_t channel, uint8_t programNo) {
    Bytes m = header(channel);
    m.push_back(static_cast<uint8_t>(Func::ProgramWriteRequest));
    m.push_back(0x00);
    m.push_back(static_cast<uint8_t>(programNo & 0x7F));
    m.push_back(kEox);
    return m;
}

Bytes makeProgramDump(const Bytes& program254, uint8_t channel, Func f) {
    Bytes m = header(channel);
    m.push_back(static_cast<uint8_t>(f));
    const Bytes packed = pack7to8(program254);
    m.insert(m.end(), packed.begin(), packed.end());
    m.push_back(kEox);
    return m;
}

Bytes makeBankDump(const std::vector<Bytes>& programs, uint8_t channel, Func f) {
    Bytes all;
    all.reserve(programs.size() * kProgramBytes);
    for (const auto& p : programs) all.insert(all.end(), p.begin(), p.end());
    Bytes m = header(channel);
    m.push_back(static_cast<uint8_t>(f));
    const Bytes packed = pack7to8(all);
    m.insert(m.end(), packed.begin(), packed.end());
    m.push_back(kEox);
    return m;
}

bool isMs2000Sysex(const Bytes& s) {
    return s.size() >= 6 && s.front() == kSox && s.back() == kEox &&
           s[1] == kKorgId && (s[2] & 0xF0) == 0x30 && s[3] == kModelId;
}

std::optional<Bytes> parseProgramDump(const Bytes& s) {
    if (!isMs2000Sysex(s)) return std::nullopt;
    if (s[4] != static_cast<uint8_t>(Func::CurrentProgramDump)) return std::nullopt;
    Bytes prog = unpack8to7(Bytes(s.begin() + 5, s.end() - 1));
    if (prog.size() < kProgramBytes) return std::nullopt;
    prog.resize(kProgramBytes);
    return prog;
}

std::optional<std::vector<Bytes>> parseBankDump(const Bytes& s) {
    if (!isMs2000Sysex(s)) return std::nullopt;
    const uint8_t func = s[4];
    if (func != static_cast<uint8_t>(Func::ProgramDump) &&
        func != static_cast<uint8_t>(Func::AllDump))
        return std::nullopt;
    Bytes data = unpack8to7(Bytes(s.begin() + 5, s.end() - 1));
    const size_t count = data.size() / kProgramBytes; // 128 for a full bank
    if (count == 0) return std::nullopt;
    std::vector<Bytes> programs;
    programs.reserve(count);
    for (size_t i = 0; i < count; ++i)
        programs.emplace_back(data.begin() + i * kProgramBytes,
                              data.begin() + (i + 1) * kProgramBytes);
    return programs;
}

} // namespace ms2000
