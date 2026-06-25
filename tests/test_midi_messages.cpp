// Verifies the real-time MIDI byte builders.
//   g++ -std=c++17 tests/test_midi_messages.cpp Source/midi/MidiMessages.cpp \
//       Source/model/ParameterModel.cpp -o t
#include "../Source/midi/MidiMessages.h"
#include <cstdio>

using namespace ms2000;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } \
                              else { printf("ok:   %s\n", msg); } } while (0)

int main() {
    // CC on channel 4 (status 0xB3).
    Bytes cc = buildCC(3, 74, 100);
    CHECK((cc == Bytes{0xB3, 74, 100}), "buildCC ch3 cc74 v100");

    // NRPN sequence order: 63 msb, 62 lsb, 06 data.
    Bytes n = buildNRPN(0, 0x04, 0x00, 0x05);
    CHECK((n == Bytes{0xB0,0x63,0x04, 0xB0,0x62,0x00, 0xB0,0x06,0x05}),
          "buildNRPN emits 63/62/06 in order");

    // A param with a CC produces the band-centred CC; cutoff passes through.
    const ParamSpec* cutoff = findParam("filt_cutoff");
    CHECK((realtimeForParam(*cutoff, 100, 3) == Bytes{0xB3, 74, 100}),
          "cutoff realtime -> CC74 value 100");

    const ParamSpec* o1wave = findParam("osc1_wave");
    Bytes w = realtimeForParam(*o1wave, 5, 0); // DWGS index 5 -> band centre
    CHECK((w == Bytes{0xB0, 77, (uint8_t)((5*128+64)/8)}), "osc1 wave realtime band-centred on CC77");

    // A no-CC param yields no real-time CC bytes (caller falls back to dump/NRPN).
    const ParamSpec* tune = findParam("pitch_tune");
    CHECK(realtimeForParam(*tune, 70, 0).empty(), "no-CC param returns empty");

    printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
