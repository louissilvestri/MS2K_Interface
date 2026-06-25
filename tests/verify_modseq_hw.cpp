// Hardware round-trip for the Mod Sequencer: take the synth's current program,
// write a known mod-seq into it (common + 3 lanes + a step curve) via the SAME
// full-dump path the app uses, read it back, and assert every byte matches.
// Non-destructive: writes only to the edit buffer (no program WRITE).
//   g++ -std=c++17 -D__WINDOWS_MM__ tests/verify_modseq_hw.cpp \
//       Source/midi/rtmidi/RtMidi.cpp Source/midi/SysexCodec.cpp -lwinmm -o verify_modseq_hw
#include "../Source/midi/rtmidi/RtMidi.h"
#include "../Source/midi/SysexCodec.h"
#include "../Source/model/ModSeq.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <optional>
#include <thread>
#include <vector>

using namespace ms2000;
using clk = std::chrono::steady_clock;
static RtMidiOut out; static RtMidiIn in;

static void sendSplit(const Bytes& b) {
    if (b.empty()) return;
    if (b[0] == 0xF0) { std::vector<unsigned char> v(b.begin(), b.end()); out.sendMessage(&v); return; }
    size_t i = 0; while (i < b.size()) { size_t j = i + 1; while (j < b.size() && b[j] < 0x80) ++j;
        std::vector<unsigned char> v(b.begin() + i, b.begin() + j); out.sendMessage(&v); i = j; }
}
static std::optional<Program> extract(const std::vector<unsigned char>& d) {
    for (size_t i = 0; i < d.size(); ++i) { if (d[i] != 0xF0) continue;
        size_t j = i + 1; while (j < d.size() && d[j] != 0xF7) ++j; if (j >= d.size()) break;
        Bytes msg(d.begin() + i, d.begin() + j + 1);
        if (auto p = parseProgramDump(msg)) { std::array<uint8_t, kProgramSize> a{}; std::copy_n(p->begin(), kProgramSize, a.begin()); return Program(a); }
        i = j; }
    return std::nullopt;
}
static std::optional<Program> request() {
    out.sendMessage(new std::vector<unsigned char>{0xF0,0x42,0x30,0x58,0x10,0xF7});
    std::vector<unsigned char> flat, m; auto dl = clk::now() + std::chrono::milliseconds(1500);
    while (clk::now() < dl) { in.getMessage(&m);
        if (!m.empty()) { flat.insert(flat.end(), m.begin(), m.end()); if (auto p = extract(flat)) return p; }
        else std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    return std::nullopt;
}
static int findPort(RtMidi& m, const char* n) {
    for (unsigned i = 0; i < m.getPortCount(); ++i) if (m.getPortName(i).find(n) != std::string::npos) return (int)i;
    return -1;
}

int main() {
    int oi = findPort(out, "UM-ONE"), ii = findPort(in, "UM-ONE");
    if (oi < 0 || ii < 0) { printf("UM-ONE not found (synth off / unplugged?)\n"); return 2; }
    out.openPort(oi); in.ignoreTypes(false, true, true); in.openPort(ii);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto cur = request();
    if (!cur) { printf("No program reply (enable Global>MIDI>SysEx and press WRITE)\n"); return 2; }
    Program p = *cur;

    // Author a known sequence.
    modseq::setOn(p, true); modseq::setRunMode(p, 1); modseq::setResolution(p, 9); // 1/4
    modseq::setLastStep(p, 11); modseq::setSeqType(p, 2); modseq::setKeySync(p, 1); // Timbre
    const int dests[3] = {11 /*Cutoff*/, 16 /*Panpot*/, 27 /*Patch1Int*/};
    for (int l = 0; l < 3; ++l) {
        modseq::setDest(p, l, dests[l]); modseq::setMotion(p, l, l == 2 ? 1 : 0);
        for (int s = 0; s < 16; ++s) modseq::setStep(p, l, s, ((s * 8 + l * 17) % 127) - 63);
    }
    Program want = p;

    Bytes d(p.bytes().begin(), p.bytes().end());
    sendSplit(makeProgramDump(d, 0, Func::CurrentProgramDump));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    auto rb = request();
    if (!rb) { printf("No readback after send\n"); return 1; }
    Program got = *rb;

    int fails = 0;
    auto chk = [&](const char* what, int a, int b) { if (a != b) { printf("FAIL %-14s want=%d got=%d\n", what, a, b); ++fails; } };
    chk("on",      modseq::on(want),       modseq::on(got));
    chk("runMode", modseq::runMode(want),  modseq::runMode(got));
    chk("resolution", modseq::resolution(want), modseq::resolution(got));
    chk("lastStep", modseq::lastStep(want), modseq::lastStep(got));
    chk("seqType",  modseq::seqType(want),  modseq::seqType(got));
    chk("keySync",  modseq::keySync(want),  modseq::keySync(got));
    for (int l = 0; l < 3; ++l) {
        chk("dest",   modseq::dest(want, l),   modseq::dest(got, l));
        chk("motion", modseq::motion(want, l), modseq::motion(got, l));
        for (int s = 0; s < 16; ++s) chk("step", modseq::step(want, l, s), modseq::step(got, l, s));
    }
    printf(fails ? "\n==== %d byte(s) MISMATCH ====\n" : "\n==== MOD-SEQ HARDWARE ROUND-TRIP: ALL %d VALUES MATCH ====\n",
           fails ? fails : (6 + 3 * (2 + 16)));
    return fails ? 1 : 0;
}
