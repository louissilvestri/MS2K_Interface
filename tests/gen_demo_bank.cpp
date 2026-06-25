// Generates a synthetic 128-program MS2000 bank .syx (func 0x4C) for testing the
// librarian without hardware.
//   g++ -std=c++17 -I../Source tests/gen_demo_bank.cpp Source/midi/SysexCodec.cpp -o gen && ./gen docs/demo_bank.syx
#include "../Source/midi/SysexCodec.h"
#include <cstdio>
#include <cstring>

using namespace ms2000;

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "demo_bank.syx";
    std::vector<Bytes> bank;
    for (int i = 0; i < 128; ++i) {
        Bytes p(kProgramBytes, 0);
        char name[13] = {0};
        std::snprintf(name, sizeof name, "Demo %03d", i + 1);
        for (int c = 0; c < 12; ++c) p[(size_t)c] = name[c] ? (uint8_t)name[c] : ' ';
        // vary a few audible params so loading shows different knob positions
        p[16] = 0x00;                       // voice mode Single
        p[38 + 20] = (uint8_t)(i * 2 % 128);// timbre1 cutoff
        p[38 + 21] = (uint8_t)(i * 3 % 128);// resonance
        p[38 + 7]  = (uint8_t)(i % 8);      // osc1 wave
        bank.push_back(p);
    }
    Bytes syx = makeBankDump(bank, 0, Func::ProgramDump);
    FILE* f = std::fopen(out, "wb");
    if (!f) { std::perror("open"); return 1; }
    std::fwrite(syx.data(), 1, syx.size(), f);
    std::fclose(f);
    std::printf("wrote %s (%zu bytes, 128 programs)\n", out, syx.size());
    return 0;
}
