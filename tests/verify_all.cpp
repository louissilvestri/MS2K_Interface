// Comprehensive app->synth round-trip test: for every parameter, send its MIDI
// (CC / NRPN / full dump exactly as the app does), read the program back, and
// confirm the byte changed to the expected value.
//   g++ -std=c++17 -D__WINDOWS_MM__ tests/verify_all.cpp \
//       Source/midi/rtmidi/RtMidi.cpp Source/midi/SysexCodec.cpp \
//       Source/midi/MidiMessages.cpp Source/model/ParameterModel.cpp -lwinmm -o verify_all
#include "../Source/midi/rtmidi/RtMidi.h"
#include "../Source/midi/SysexCodec.h"
#include "../Source/midi/MidiMessages.h"
#include "../Source/model/ParameterModel.h"
#include "../Source/model/Program.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <optional>
#include <thread>
#include <vector>

using namespace ms2000;
using clk = std::chrono::steady_clock;

static RtMidiOut out; static RtMidiIn in;

static const std::map<std::string, std::pair<uint8_t, uint8_t>> kNrpn = {
    {"arp_on",{0,2}},{"arp_range",{0,3}},{"arp_latch",{0,4}},{"arp_type",{0,7}},{"arp_gate",{0,0x0A}},
    {"patch1_src",{4,0}},{"patch2_src",{4,1}},{"patch3_src",{4,2}},{"patch4_src",{4,3}},
    {"patch1_dst",{4,8}},{"patch2_dst",{4,9}},{"patch3_dst",{4,0x0A}},{"patch4_dst",{4,0x0B}},
};

static void sendMsg(std::vector<unsigned char> v) { out.sendMessage(&v); }
static void sendSplit(const Bytes& b) {            // mimic MidiEngine::sendRaw
    if (b.empty()) return;
    if (b[0] == 0xF0) { sendMsg({b.begin(), b.end()}); return; }
    size_t i = 0;
    while (i < b.size()) { size_t j = i + 1; while (j < b.size() && b[j] < 0x80) ++j;
                           sendMsg({b.begin() + i, b.begin() + j}); i = j; }
}

static std::optional<Program> extractProgram(const std::vector<unsigned char>& d) {
    for (size_t i = 0; i < d.size(); ++i) {
        if (d[i] != 0xF0) continue;
        size_t j = i + 1; while (j < d.size() && d[j] != 0xF7) ++j;
        if (j >= d.size()) break;
        Bytes msg(d.begin() + i, d.begin() + j + 1);
        if (auto p = parseProgramDump(msg)) {
            std::array<uint8_t, kProgramSize> a{}; std::copy_n(p->begin(), kProgramSize, a.begin());
            return Program(a);
        }
        i = j;
    }
    return std::nullopt;
}

static std::optional<Program> requestProgram() {
    sendMsg({0xF0, 0x42, 0x30, 0x58, 0x10, 0xF7});
    std::vector<unsigned char> flat; std::vector<unsigned char> m;
    auto deadline = clk::now() + std::chrono::milliseconds(1500);
    while (clk::now() < deadline) {
        in.getMessage(&m);
        if (!m.empty()) { flat.insert(flat.end(), m.begin(), m.end());
                          if (auto p = extractProgram(flat)) return p; }
        else std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return std::nullopt;
}

static int pickTarget(const ParamSpec& s, int curRaw) {
    if (s.type == ValueType::Bool) return curRaw ? 0 : 1;
    if (s.type == ValueType::Enum) { int n = (int)std::max<size_t>(1, s.enums.size());
                                     return (curRaw + 1) % n; }
    const int lo = s.rawMin(), hi = s.rawMax();
    const int c1 = lo + (hi - lo) / 3, c2 = lo + 2 * (hi - lo) / 3;
    return (curRaw == c1) ? c2 : c1;
}

static int findPort(RtMidi& m, const char* n) {
    for (unsigned i = 0; i < m.getPortCount(); ++i)
        if (m.getPortName(i).find(n) != std::string::npos) return (int)i;
    return -1;
}

int main() {
    int oi = findPort(out, "UM-ONE"), ii = findPort(in, "UM-ONE");
    if (oi < 0 || ii < 0) { printf("UM-ONE not found\n"); return 1; }
    out.openPort(oi); in.ignoreTypes(false, true, true); in.openPort(ii);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto cur = requestProgram();
    if (!cur) { printf("No program reply from synth (SysEx off?)\n"); return 1; }
    Program current = *cur;

    // Reset state that changes how some CCs are interpreted (tempo-sync notes,
    // DWGS routing of Control 2) so each parameter is tested in its normal mode.
    for (const char* id : {"lfo1_temposync", "lfo2_temposync", "delay_sync", "osc1_wave"})
        if (auto* sp = findParam(id)) setDisplay(current, 0, *sp, 0);
    { Bytes d(current.bytes().begin(), current.bytes().end());
      sendSplit(makeProgramDump(d, 0, Func::CurrentProgramDump));
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      if (auto rb = requestProgram()) current = *rb; }

    int pass = 0, fail = 0; std::vector<std::string> fails;
    for (const auto& s : parameterTable()) {
        const int curRaw = getRaw(current, 0, s);
        const int target = pickTarget(s, curRaw);
        const char* via;
        if (s.cc >= 0) { sendSplit(buildCC(0, (uint8_t)s.cc, (uint8_t)ccValueForRaw(s, target))); via = "CC"; }
        else if (kNrpn.count(s.id)) { auto a = kNrpn.at(s.id);
            sendSplit(buildNRPN(0, a.first, a.second, (uint8_t)ccValueForRaw(s, target))); via = "NRPN"; }
        else { Program w = current; setDisplay(w, 0, s, target - s.displayOffset);
            Bytes d(w.bytes().begin(), w.bytes().end());
            sendSplit(makeProgramDump(d, 0, Func::CurrentProgramDump)); via = "DUMP"; }

        std::this_thread::sleep_for(std::chrono::milliseconds(180));
        auto rb = requestProgram();
        if (!rb) { printf("FAIL %-16s no readback\n", s.id.c_str()); fails.push_back(s.id + " (no readback)"); ++fail; continue; }
        current = *rb;
        int got = getRaw(current, 0, s);
        if (s.bipolar && s.displayOffset == 0 && got > 127) got -= 256; // signed byte
        const bool ok = (got == target);
        printf("%s %-16s %-4s target=%-4d got=%-4d cc=%d\n", ok ? "ok  " : "FAIL", s.id.c_str(), via, target, got, s.cc);
        if (ok) ++pass; else { fails.push_back(s.id + " [" + via + "]"); ++fail; }
    }

    printf("\n==== SUMMARY ====  PASS %d / %d\n", pass, pass + fail);
    if (!fails.empty()) { printf("NOT WORKING:\n"); for (auto& f : fails) printf("  - %s\n", f.c_str()); }
    return fail ? 1 : 0;
}
