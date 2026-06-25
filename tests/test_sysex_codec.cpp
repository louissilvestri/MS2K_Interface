// Standalone test for SysexCodec — compile with g++, no JUCE needed:
//   g++ -std=c++17 -I../Source tests/test_sysex_codec.cpp Source/midi/SysexCodec.cpp -o test
#include "../Source/midi/SysexCodec.h"
#include <cstdio>
#include <random>

using namespace ms2000;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++failures; } \
                              else { printf("ok:   %s\n", msg); } } while (0)

int main() {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, 255);

    // 1. pack/unpack round-trips for a full 254-byte program of random data.
    Bytes prog(kProgramBytes);
    for (auto& b : prog) b = static_cast<uint8_t>(dist(rng));
    Bytes packed = pack7to8(prog);
    CHECK(packed.size() == 291, "254-byte program packs to 291 bytes");
    // every transmitted byte must have bit7 clear (legal MIDI data)
    bool allClear = true; for (auto b : packed) allClear &= (b & 0x80) == 0;
    CHECK(allClear, "all packed bytes have bit7 == 0");
    Bytes back = unpack8to7(packed);
    back.resize(kProgramBytes);
    CHECK(back == prog, "unpack(pack(prog)) == prog (254 bytes)");

    // 2. pack/unpack for sizes that don't divide by 7 (partial final group).
    for (size_t n : {1u, 2u, 7u, 8u, 13u, 200u}) {
        Bytes d(n); for (auto& b : d) b = static_cast<uint8_t>(dist(rng));
        Bytes r = unpack8to7(pack7to8(d)); r.resize(n);
        char m[64]; snprintf(m, sizeof m, "round-trip size %zu", n);
        CHECK(r == d, m);
    }

    // 3. full SysEx dump build + parse round-trip.
    Bytes dump = makeProgramDump(prog, /*channel=*/3, Func::CurrentProgramDump);
    CHECK(isMs2000Sysex(dump), "built dump is recognised as MS2000 SysEx");
    CHECK(dump.front() == 0xF0 && dump.back() == 0xF7, "framed with F0..F7");
    CHECK(dump[2] == 0x33, "channel 3 encoded as 0x33");
    auto parsed = parseProgramDump(dump);
    CHECK(parsed.has_value(), "dump parses back to a program");
    CHECK(parsed && *parsed == prog, "parsed program equals original 254 bytes");

    // 4. request framing.
    Bytes req = makeRequest(Func::CurrentProgramDumpRequest, 0);
    CHECK(req.size() == 6 && req[4] == 0x10, "current-program request is F0 42 30 58 10 F7");
    Bytes breq = makeRequest(Func::ProgramDumpRequest, 0);
    CHECK(breq[4] == 0x1C, "program(bank) dump request is func 0x1C");

    // 5. bank dump round-trip: build N programs, frame as 0x4C, parse back.
    std::vector<Bytes> bank;
    for (int n = 0; n < 4; ++n) {
        Bytes pr(kProgramBytes);
        for (auto& b : pr) b = static_cast<uint8_t>(dist(rng));
        bank.push_back(pr);
    }
    Bytes bankDump = makeBankDump(bank, 5, Func::ProgramDump);
    CHECK(bankDump[2] == 0x35 && bankDump[4] == 0x4C, "bank dump framed as ch5 / func 0x4C");
    auto pbank = parseBankDump(bankDump);
    CHECK(pbank.has_value() && pbank->size() == 4, "bank parses back to 4 programs");
    CHECK(pbank && *pbank == bank, "all 4 programs survive the bank round-trip");
    // a single-program (0x40) dump must NOT be mistaken for a bank, and vice versa.
    CHECK(!parseBankDump(dump).has_value(), "0x40 current-program is not a bank");
    CHECK(!parseProgramDump(bankDump).has_value(), "0x4C bank is not a single program");

    printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
