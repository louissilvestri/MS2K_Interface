// Verifies ParameterModel: byte placement, bipolar scaling, enum/CC mapping.
//   g++ -std=c++17 -I../Source tests/test_param_model.cpp Source/model/ParameterModel.cpp -o t
#include "../Source/model/ParameterModel.h"
#include <cstdio>

using namespace ms2000;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } \
                              else { printf("ok:   %s\n", msg); } } while (0)

int main() {
    Program p;

    // 1. A timbre-1 continuous param writes to the right absolute byte.
    //    Filter Cutoff is timbre offset +20 -> byte 38+20 = 58.
    const ParamSpec* cutoff = findParam("filt_cutoff");
    CHECK(cutoff != nullptr, "filt_cutoff exists in table");
    setDisplay(p, 0, *cutoff, 100);
    CHECK(p.bytes()[58] == 100, "cutoff (timbre1) lands at byte 58");
    CHECK(getDisplay(p, 0, *cutoff) == 100, "cutoff reads back 100");

    // 2. Timbre 2 of the same param lands 108 bytes later (byte 166).
    setDisplay(p, 1, *cutoff, 42);
    CHECK(p.bytes()[166] == 42, "cutoff (timbre2) lands at byte 166");
    CHECK(p.bytes()[58] == 100, "timbre1 cutoff untouched by timbre2 write");

    // 3. Bipolar scaling: EG1 Intensity is 64-centred. display 0 -> raw 64.
    const ParamSpec* eg1int = findParam("filt_eg1int");
    setDisplay(p, 0, *eg1int, 0);
    CHECK(getRaw(p, 0, *eg1int) == 64, "bipolar display 0 -> raw 64");
    setDisplay(p, 0, *eg1int, -63);
    CHECK(getRaw(p, 0, *eg1int) == 1, "bipolar display -63 -> raw 1");
    CHECK(getDisplay(p, 0, *eg1int) == -63, "bipolar reads back -63");

    // 4. Bitfield packing: OSC2 wave (b0-1) and mod (b4-5) share byte +12 (=50).
    const ParamSpec* o2wave = findParam("osc2_wave");
    const ParamSpec* o2mod  = findParam("osc2_mod");
    setDisplay(p, 0, *o2wave, 2);  // Tri
    setDisplay(p, 0, *o2mod, 3);   // RingSync
    CHECK((p.bytes()[50] & 0x03) == 2, "osc2 wave in low 2 bits");
    CHECK(((p.bytes()[50] >> 4) & 0x03) == 3, "osc2 mod in bits 4-5");
    CHECK(getDisplay(p, 0, *o2wave) == 2 && getDisplay(p, 0, *o2mod) == 3,
          "shared-byte bitfields read back independently");

    // 5. Enum CC band-centring: 8-wave selector -> centres 8,24,..,120.
    const ParamSpec* o1wave = findParam("osc1_wave");
    CHECK(ccValueForRaw(*o1wave, 0) == 8,  "osc1 wave idx0 -> CC 8");
    CHECK(ccValueForRaw(*o1wave, 7) == 120,"osc1 wave idx7 -> CC 120");
    const ParamSpec* ftype = findParam("filt_type");
    CHECK(ccValueForRaw(*ftype, 0) == 16, "filter type idx0 -> CC 16");
    CHECK(ccValueForRaw(*ftype, 3) == 112,"filter type idx3 -> CC 112");

    // 6. Bool CC is 0/127; continuous CC passes through.
    const ParamSpec* dist = findParam("amp_distortion");
    CHECK(ccValueForRaw(*dist, 1) == 127 && ccValueForRaw(*dist, 0) == 0, "bool CC 0/127");
    CHECK(ccValueForRaw(*cutoff, 99) == 99, "continuous CC passes through");

    // 7. Voice mode (raw 0/2/3) handled by Program directly.
    p.setVoiceMode(VoiceMode::Vocoder);
    CHECK(((p.bytes()[16] >> 4) & 3) == 3, "voice mode Vocoder -> byte16 bits4-5 = 3");
    CHECK(p.voiceMode() == VoiceMode::Vocoder, "voice mode reads back Vocoder");

    // 8. Program name round-trips.
    p.setName("BassPatch");
    CHECK(p.name() == "BassPatch", "program name round-trips");

    printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
