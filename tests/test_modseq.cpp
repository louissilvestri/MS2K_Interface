// Validate ModSeq accessors by decoding the real bank and checking that a known
// patch ("Evolution", prog 2) reads back the exact step curve we saw in the raw
// bytes — and that a round-trip set/get is stable.
//   g++ -std=c++17 tests/test_modseq.cpp Source/midi/SysexCodec.cpp -o test_modseq
#include "../Source/midi/SysexCodec.h"
#include "../Source/model/ModSeq.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <vector>

using namespace ms2000;

static Bytes readFile(const char* p) {
    std::ifstream f(p, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s\n", msg); ++failures; } } while(0)

int main() {
    Bytes syx = readFile("docs/synth_bank.syx");
    if (syx.empty()) syx = readFile("C:/Claude/MS2K_Interface/docs/synth_bank.syx");
    auto bank = parseBankDump(syx);
    if (!bank || bank->size() < 3) { printf("could not load bank\n"); return 1; }

    auto mk = [](const Bytes& b){ std::array<uint8_t,kProgramSize> a{}; std::copy_n(b.begin(),kProgramSize,a.begin()); return Program(a); };
    Program evo = mk((*bank)[2]);   // "Evolution"

    // Common, from the raw dump: byte90=C0 (On=1,Loop), byte91=F2 (last=16,type=0,sync=Voice=2)
    CHECK(modseq::on(evo) == true,            "Evolution seq On");
    CHECK(modseq::runMode(evo) == 1,          "Evolution Loop");
    CHECK(modseq::lastStep(evo) == 15,        "Evolution last step 16");
    CHECK(modseq::seqType(evo) == 0,          "Evolution type Forward");
    CHECK(modseq::keySync(evo) == 2,          "Evolution keysync Voice");

    // Lane 0 dest = 0x19 = 25 = LFO1Freq; motion Smooth; first steps 32,37,39,3F (signed)
    CHECK(modseq::dest(evo, 0) == 25,         "Evolution lane0 dest LFO1Freq");
    CHECK(modseq::motion(evo, 0) == 0,        "Evolution lane0 motion smooth");
    CHECK(modseq::step(evo, 0, 0) == 0x32-64, "Evolution lane0 step0");
    CHECK(modseq::step(evo, 0, 3) == 0x3F-64, "Evolution lane0 step3");
    CHECK(modseq::dest(evo, 1) == 30,         "Evolution lane1 dest Patch4Int");
    CHECK(modseq::dest(evo, 2) == 24,         "Evolution lane2 dest EG2Release");

    // Round-trip: set every common + lane + step, read back identical.
    Program p;
    modseq::setOn(p, true); modseq::setRunMode(p, 1); modseq::setResolution(p, 9);
    modseq::setLastStep(p, 11); modseq::setSeqType(p, 2); modseq::setKeySync(p, 1);
    CHECK(modseq::on(p) && modseq::runMode(p)==1 && modseq::resolution(p)==9 &&
          modseq::lastStep(p)==11 && modseq::seqType(p)==2 && modseq::keySync(p)==1, "common roundtrip");
    for (int l = 0; l < 3; ++l) {
        modseq::setDest(p, l, 5 + l); modseq::setMotion(p, l, l & 1);
        for (int s = 0; s < 16; ++s) modseq::setStep(p, l, s, (s - 8) * 7 + l);
    }
    for (int l = 0; l < 3; ++l) {
        CHECK(modseq::dest(p, l) == 5 + l,   "dest roundtrip");
        CHECK(modseq::motion(p, l) == (l & 1), "motion roundtrip");
        for (int s = 0; s < 16; ++s) {
            int want = (s - 8) * 7 + l; if (want < -63) want = -63; if (want > 63) want = 63;
            CHECK(modseq::step(p, l, s) == want, "step roundtrip");
        }
    }
    // common bits must not bleed into each other
    CHECK(modseq::on(p) && modseq::resolution(p)==9, "no bitfield bleed after lane writes");

    printf(failures ? "\n%d CHECK(S) FAILED\n" : "\nALL MODSEQ CHECKS PASSED\n", failures);
    return failures ? 1 : 0;
}
