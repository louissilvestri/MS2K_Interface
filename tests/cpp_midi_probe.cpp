// Proves the app's exact C++ MIDI stack (RtMidi + SysEx reassembly + codec) end
// to end on the real synth, without the GUI.
//   g++ -std=c++17 -D__WINDOWS_MM__ tests/cpp_midi_probe.cpp \
//       Source/midi/rtmidi/RtMidi.cpp Source/midi/SysexCodec.cpp -lwinmm -o cpp_probe
#include "../Source/midi/rtmidi/RtMidi.h"
#include "../Source/midi/SysexCodec.h"
#include "../Source/model/Program.h"
#include <algorithm>
#include <cstdio>
#include <thread>
#include <chrono>
#include <vector>

using namespace ms2000;

static std::vector<uint8_t> accum;
static std::string lastResult;

static std::string nameOf(const Bytes& prog) {
    std::array<uint8_t, kProgramSize> a{};
    std::copy_n(prog.begin(), kProgramSize, a.begin());
    return Program(a).name();
}

static void onSysex(const Bytes& full) {
    if (auto p = parseProgramDump(full)) {
        lastResult = "current program '" + nameOf(*p) + "'";
    } else if (auto b = parseBankDump(full)) {
        lastResult = "bank of " + std::to_string(b->size()) + " programs (first '" +
                     nameOf(b->front()) + "', last '" + nameOf(b->back()) + "')";
    } else {
        lastResult = "unrecognized sysex, " + std::to_string(full.size()) + " bytes";
    }
    printf("    >> %s\n", lastResult.c_str());
}

static void feed(const std::vector<unsigned char>& msg) {
    if (msg.empty()) return;
    if (!accum.empty()) accum.insert(accum.end(), msg.begin(), msg.end());
    else if (msg[0] == 0xF0) accum.assign(msg.begin(), msg.end());
    else return;
    if (accum.back() == 0xF7) { onSysex(accum); accum.clear(); }
}

static int findPort(unsigned count, RtMidi& m, const char* name) {
    for (unsigned i = 0; i < count; ++i)
        if (m.getPortName(i).find(name) != std::string::npos) return (int)i;
    return -1;
}

int main() {
    RtMidiOut out; RtMidiIn in;
    int oi = findPort(out.getPortCount(), out, "UM-ONE");
    int ii = findPort(in.getPortCount(), in, "UM-ONE");
    printf("UM-ONE out=%d in=%d\n", oi, ii);
    if (oi < 0 || ii < 0) { printf("FAIL: UM-ONE not found\n"); return 1; }
    out.openPort((unsigned)oi);
    in.ignoreTypes(false, true, true);
    in.openPort((unsigned)ii);   // POLLING mode (no setCallback), like the python probe

    auto send = [&](std::vector<unsigned char> m){ out.sendMessage(&m); };
    auto poll = [&](int ms){
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < deadline) {
            std::vector<unsigned char> m;
            in.getMessage(&m);
            if (!m.empty()) { feed(m); deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500); }
            else std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    };
    const uint8_t g = 0x30;

    printf("[1] current program request...\n");
    lastResult.clear();
    send({0xF0,0x42,g,0x58,0x10,0xF7});
    poll(1500);

    printf("[2] all-programs request...\n");
    lastResult.clear();
    send({0xF0,0x42,g,0x58,0x1C,0xF7});
    poll(8000);

    bool ok = lastResult.find("bank of 128") != std::string::npos;
    printf("\nRESULT: %s\n", ok ? "PASS - C++ RtMidi stack pulled the full bank" : "did not get full 128 bank");
    return ok ? 0 : 1;
}
