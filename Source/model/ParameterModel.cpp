#include "ParameterModel.h"
#include <algorithm>
#include <unordered_map>

namespace ms2000 {

// Non-linear MIDI value -> display value maps from the MIDI implementation.
// OSC2 Semitone (CC18, *2-5): MIDI 0..127 -> semitone -24..+24.
const int8_t kSemitoneTable[128] = {
 -24,-24,-24,-23,-23,-23,-22,-22, -21,-21,-21,-20,-20,-20,-19,-19,
 -18,-18,-18,-17,-17,-16,-16,-16, -15,-15,-15,-14,-14,-13,-13,-13,
 -12,-12,-11,-11,-11,-10,-10,-10,  -9, -9, -8, -8, -8, -7, -7, -7,
  -6, -6, -5, -5, -5, -4, -4, -3,  -3, -3, -2, -2, -2, -1, -1,  0,
   0,  0,  1,  1,  2,  2,  2,  3,   3,  3,  4,  4,  5,  5,  5,  6,
   6,  7,  7,  7,  8,  8,  8,  9,   9, 10, 10, 10, 11, 11, 11, 12,
  12, 13, 13, 13, 14, 14, 15, 15,  15, 16, 16, 16, 17, 17, 18, 18,
  18, 19, 19, 20, 20, 20, 21, 21,  21, 22, 22, 23, 23, 23, 24, 24,
};
// Arp Gate (NRPN 00 0A, *2-2): MIDI 0..127 -> gate 0..100 %.
const int8_t kArpGateTable[128] = {
   0,  0,  1,  2,  3,  3,  4,  5,   6,  7,  7,  8,  9, 10, 11, 11,
  12, 13, 14, 14, 15, 16, 17, 18,  18, 19, 20, 21, 22, 22, 23, 24,
  25, 26, 26, 27, 28, 29, 29, 30,  31, 32, 33, 33, 34, 35, 36, 37,
  37, 38, 39, 40, 41, 41, 42, 43,  44, 44, 45, 46, 47, 48, 48, 49,
  50, 51, 52, 52, 53, 54, 55, 56,  56, 57, 58, 59, 59, 60, 61, 62,
  63, 63, 64, 65, 66, 67, 67, 68,  69, 70, 71, 71, 72, 73, 74, 74,
  75, 76, 77, 78, 78, 79, 80, 81,  82, 82, 83, 84, 85, 86, 86, 87,
  88, 89, 89, 90, 91, 92, 93, 93,  94, 95, 96, 97, 97, 98, 99,100,
};

// Shorthand builders to keep the table readable.
namespace {
using S = Section; using V = ValueType; using Sc = Scope;

ParamSpec cont(std::string id, std::string label, S sec, Sc sc, int off,
               int dmin, int dmax, int dOff, bool bip, int cc) {
    return { std::move(id), std::move(label), sec, sc, V::Continuous, off,
             0, 0xFF, dmin, dmax, dOff, bip, cc, {} };
}
ParamSpec bits(std::string id, std::string label, S sec, Sc sc, int off,
               uint8_t shift, uint8_t mask, int dmin, int dmax, int dOff,
               bool bip, int cc) {
    return { std::move(id), std::move(label), sec, sc, V::Continuous, off,
             shift, mask, dmin, dmax, dOff, bip, cc, {} };
}
ParamSpec enm(std::string id, std::string label, S sec, Sc sc, int off,
              uint8_t shift, uint8_t mask, int cc, std::vector<std::string> e) {
    const int n = (int)e.size() - 1;
    return { std::move(id), std::move(label), sec, sc, V::Enum, off,
             shift, mask, 0, n, 0, false, cc, std::move(e) };
}
ParamSpec boolean(std::string id, std::string label, S sec, Sc sc, int off,
                  uint8_t shift, int cc) {
    return { std::move(id), std::move(label), sec, sc, V::Bool, off,
             shift, 0x01, 0, 1, 0, false, cc, {"Off","On"} };
}
} // namespace

const std::vector<ParamSpec>& parameterTable() {
    static const std::vector<ParamSpec> t = [] {
        std::vector<ParamSpec> v;

        // ---------- PROGRAM COMMON (Scope::Program) ------------------------
        // Effects (Mod FX + Delay FX in one block, like the hardware EFFECTS section)
        v.push_back(enm("modfx_type","Mod Type", S::Effects, Sc::Program, 25, 0,0xFF, -1,
                        {"Cho/Flg","Ensemble","Phaser"}));
        v.push_back(cont("modfx_speed","Mod Speed", S::Effects, Sc::Program, 23, 0,127,0,false, 12));
        v.push_back(cont("modfx_depth","Mod Depth", S::Effects, Sc::Program, 24, 0,127,0,false, 93));
        v.push_back(enm("delay_type","Dly Type", S::Effects, Sc::Program, 22, 0,0xFF, -1,
                        {"Stereo","Cross","L/R"}));
        v.push_back(cont("delay_time","Dly Time", S::Effects, Sc::Program, 20, 0,127,0,false, 13));
        v.push_back(cont("delay_depth","Dly Depth", S::Effects, Sc::Program, 21, 0,127,0,false, 94));
        v.push_back(boolean("delay_sync","Dly Sync", S::Effects, Sc::Program, 19, 7, -1));
        // EQ
        v.push_back(cont("eq_hi_freq","Hi Freq", S::Eq, Sc::Program, 26, 0,29,0,false, -1));
        v.push_back(cont("eq_hi_gain","Hi Gain", S::Eq, Sc::Program, 27, -12,12,64,true, -1));
        v.push_back(cont("eq_lo_freq","Low Freq", S::Eq, Sc::Program, 28, 0,29,0,false, -1));
        v.push_back(cont("eq_lo_gain","Low Gain", S::Eq, Sc::Program, 29, -12,12,64,true, -1));
        // Arpeggiator
        v.push_back(boolean("arp_on","Arp On", S::Arp, Sc::Program, 32, 7, -1));
        v.push_back(boolean("arp_latch","Latch", S::Arp, Sc::Program, 32, 6, -1));
        v.push_back(enm("arp_target","Target", S::Arp, Sc::Program, 32, 4,0x03, -1,
                        {"Both","Timbre1","Timbre2"}));
        v.push_back(boolean("arp_keysync","Key Sync", S::Arp, Sc::Program, 32, 0, -1));
        v.push_back(enm("arp_type","Type", S::Arp, Sc::Program, 33, 0,0x0F, -1,
                        {"Up","Down","Alt1","Alt2","Random","Trigger"}));
        v.push_back(bits("arp_range","Range", S::Arp, Sc::Program, 33, 4,0x0F, 0,3,0,false,-1));
        { auto s = cont("arp_gate","Gate Time", S::Arp, Sc::Program, 34, 0,100,0,false, -1);
          s.table = kArpGateTable; v.push_back(s); }     // NRPN via *2-2 table
        v.push_back(enm("arp_resolution","Resolution", S::Arp, Sc::Program, 35, 0,0xFF, -1,
                        {"1/24","1/16","1/12","1/8","1/6","1/4"}));
        v.push_back(cont("arp_swing","Swing", S::Arp, Sc::Program, 36, -100,100,0,true, -1));
        v.push_back(cont("kbd_octave","KBD Octave", S::Arp, Sc::Program, 37, -3,3,0,true, -1));

        // ---------- PER-TIMBRE (Scope::Timbre) -----------------------------
        // Voice
        v.push_back(enm("assign_mode","Assign", S::Voice, Sc::Timbre, 1, 6,0x03, -1,
                        {"Mono","Poly","Unison"}));
        v.push_back(cont("unison_detune","Uni Detune", S::Voice, Sc::Timbre, 2, 0,99,0,false, -1));
        // OSC1 (incl. the timbre pitch params, which have no dedicated front-panel
        // knobs on the hardware — they live within the oscillator group).
        v.push_back(enm("osc1_wave","Wave", S::Osc1, Sc::Timbre, 7, 0,0xFF, 77,
                        {"Saw","Pulse","Tri","Sin","Vox","DWGS","Noise","AudioIn"}));
        v.push_back(cont("osc1_ctrl1","Control 1", S::Osc1, Sc::Timbre, 8, 0,127,0,false, 14));
        v.push_back(cont("osc1_ctrl2","Control 2", S::Osc1, Sc::Timbre, 9, 0,127,0,false, 15));
        v.push_back(cont("osc1_dwgs","DWGS Wave", S::Osc1, Sc::Timbre, 10, 0,63,0,false, -1));
        v.push_back(cont("transpose","Transpose", S::Osc1, Sc::Timbre, 5, -24,24,64,true, -1));
        v.push_back(cont("pitch_tune","Tune", S::Osc1, Sc::Timbre, 3, -50,50,64,true, -1));
        v.push_back(cont("bend_range","Bend Range", S::Osc1, Sc::Timbre, 4, -12,12,64,true, -1));
        v.push_back(cont("vibrato_int","Vibrato Int", S::Osc1, Sc::Timbre, 6, -63,63,64,true, -1));
        // Portamento (own section, like the hardware)
        v.push_back(bits("portamento","Time", S::Portamento, Sc::Timbre, 15, 0,0x7F, 0,127,0,false, 5));
        // OSC2
        v.push_back(enm("osc2_wave","Wave", S::Osc2, Sc::Timbre, 12, 0,0x03, 78,
                        {"Saw","Squ","Tri"}));
        v.push_back(enm("osc2_mod","Mod", S::Osc2, Sc::Timbre, 12, 4,0x03, 82,
                        {"Off","Ring","Sync","RingSync"}));
        { auto s = cont("osc2_semitone","Semitone", S::Osc2, Sc::Timbre, 13, -24,24,64,true, 18);
          s.table = kSemitoneTable; v.push_back(s); }   // CC18 via *2-5 table
        v.push_back(cont("osc2_tune","Tune", S::Osc2, Sc::Timbre, 14, -63,63,64,true, 19));
        // Mixer
        v.push_back(cont("mix_osc1","OSC1 Level", S::Mixer, Sc::Timbre, 16, 0,127,0,false, 20));
        v.push_back(cont("mix_osc2","OSC2 Level", S::Mixer, Sc::Timbre, 17, 0,127,0,false, 21));
        v.push_back(cont("mix_noise","Noise Level", S::Mixer, Sc::Timbre, 18, 0,127,0,false, 22));
        // Filter
        v.push_back(enm("filt_type","Type", S::Filter, Sc::Timbre, 19, 0,0xFF, 83,
                        {"24LPF","12LPF","12BPF","12HPF"}));
        v.push_back(cont("filt_cutoff","Cutoff", S::Filter, Sc::Timbre, 20, 0,127,0,false, 74));
        v.push_back(cont("filt_reso","Resonance", S::Filter, Sc::Timbre, 21, 0,127,0,false, 71));
        v.push_back(cont("filt_eg1int","EG1 Intensity", S::Filter, Sc::Timbre, 22, -63,63,64,true, 79));
        v.push_back(cont("filt_velsens","Velocity Sense", S::Filter, Sc::Timbre, 23, -63,63,64,true, -1));
        v.push_back(cont("filt_kbdtrack","Keyboard Track", S::Filter, Sc::Timbre, 24, -63,63,64,true, 85));
        // Amp
        v.push_back(cont("amp_level","Level", S::Amp, Sc::Timbre, 25, 0,127,0,false, 7));
        v.push_back(cont("amp_pan","Panpot", S::Amp, Sc::Timbre, 26, -64,63,64,true, 10));
        v.push_back(enm("amp_sw","EG2 / Gate", S::Amp, Sc::Timbre, 27, 6,0x01, 86, {"EG2","Gate"}));
        v.push_back(boolean("amp_distortion","Distortion", S::Amp, Sc::Timbre, 27, 0, 92));
        // EG1 (filter)
        v.push_back(cont("eg1_attack","Attack", S::Eg1, Sc::Timbre, 30, 0,127,0,false, 23));
        v.push_back(cont("eg1_decay","Decay", S::Eg1, Sc::Timbre, 31, 0,127,0,false, 24));
        v.push_back(cont("eg1_sustain","Sustain", S::Eg1, Sc::Timbre, 32, 0,127,0,false, 25));
        v.push_back(cont("eg1_release","Release", S::Eg1, Sc::Timbre, 33, 0,127,0,false, 26));
        // EG2 (amp)
        v.push_back(cont("eg2_attack","Attack", S::Eg2, Sc::Timbre, 34, 0,127,0,false, 73));
        v.push_back(cont("eg2_decay","Decay", S::Eg2, Sc::Timbre, 35, 0,127,0,false, 75));
        v.push_back(cont("eg2_sustain","Sustain", S::Eg2, Sc::Timbre, 36, 0,127,0,false, 70));
        v.push_back(cont("eg2_release","Release", S::Eg2, Sc::Timbre, 37, 0,127,0,false, 72));
        // LFO1
        v.push_back(enm("lfo1_wave","Wave", S::Lfo1, Sc::Timbre, 38, 0,0x03, 87,
                        {"Saw","Squ","Tri","S/H"}));
        v.push_back(enm("lfo1_keysync","Key Sync", S::Lfo1, Sc::Timbre, 38, 4,0x03, -1,
                        {"Off","Timbre","Voice"}));
        v.push_back(cont("lfo1_freq","Frequency", S::Lfo1, Sc::Timbre, 39, 0,127,0,false, 27));
        v.push_back(boolean("lfo1_temposync","Tempo Sync", S::Lfo1, Sc::Timbre, 40, 7, -1));
        // LFO2
        v.push_back(enm("lfo2_wave","Wave", S::Lfo2, Sc::Timbre, 41, 0,0x03, 88,
                        {"Saw","Squ(+)","Sin","S/H"}));
        v.push_back(enm("lfo2_keysync","Key Sync", S::Lfo2, Sc::Timbre, 41, 4,0x03, -1,
                        {"Off","Timbre","Voice"}));
        v.push_back(cont("lfo2_freq","Frequency", S::Lfo2, Sc::Timbre, 42, 0,127,0,false, 76));
        v.push_back(boolean("lfo2_temposync","Tempo Sync", S::Lfo2, Sc::Timbre, 43, 7, -1));
        // Virtual Patch 1..4
        const std::vector<std::string> patchSrc =
            {"EG1","EG2","LFO1","LFO2","Velocity","KbdTrack","P.Bend","Mod"};
        const std::vector<std::string> patchDst =
            {"Pitch","OSC2Pitch","OSC1Ctrl1","NoiseLvl","Cutoff","Amp","Pan","LFO2Freq"};
        const int patchCc[4] = { 28, 29, 30, 31 };
        for (int i = 0; i < 4; ++i) {
            const int off = 44 + i * 2;
            const std::string n = std::to_string(i + 1);
            v.push_back(enm("patch"+n+"_src","P"+n+" Source", S::Patch, Sc::Timbre, off, 0,0x0F, -1, patchSrc));
            v.push_back(enm("patch"+n+"_dst","P"+n+" Dest", S::Patch, Sc::Timbre, off, 4,0x0F, -1, patchDst));
            v.push_back(cont("patch"+n+"_int","P"+n+" Intensity", S::Patch, Sc::Timbre, off+1, -63,63,64,true, patchCc[i]));
        }
        return v;
    }();
    return t;
}

const ParamSpec* findParam(const std::string& id) {
    static const std::unordered_map<std::string, const ParamSpec*> idx = [] {
        std::unordered_map<std::string, const ParamSpec*> m;
        for (const auto& s : parameterTable()) m[s.id] = &s;
        return m;
    }();
    auto it = idx.find(id);
    return it == idx.end() ? nullptr : it->second;
}

static int baseFor(const ParamSpec& s, int timbre) {
    return s.scope == Scope::Timbre ? Program::timbreBase(timbre) : 0;
}

int getRaw(const Program& p, int timbre, const ParamSpec& s) {
    return p.getRaw(baseFor(s, timbre) + s.offset, s.shift, s.mask);
}

int getDisplay(const Program& p, int timbre, const ParamSpec& s) {
    int raw = getRaw(p, timbre, s);
    if (s.bipolar && s.displayOffset == 0 && raw > 127) raw -= 256; // signed byte (swing, kbd octave)
    return raw - s.displayOffset;
}

void setDisplay(Program& p, int timbre, const ParamSpec& s, int displayValue) {
    const int d = std::clamp(displayValue, s.displayMin, s.displayMax);
    p.setRaw(baseFor(s, timbre) + s.offset, s.shift, s.mask, d + s.displayOffset);
}

int ccValueForRaw(const ParamSpec& s, int rawValue) {
    if (s.table != nullptr) {                 // non-linear param: invert the table
        const int target = rawValue - s.displayOffset; // display value to encode
        int lo = -1, hi = -1, best = 64, bestd = 1000;
        for (int i = 0; i < 128; ++i) {
            if (s.table[i] == target) { if (lo < 0) lo = i; hi = i; }
            const int d = s.table[i] - target, ad = d < 0 ? -d : d;
            if (ad < bestd) { bestd = ad; best = i; }
        }
        return lo >= 0 ? (lo + hi) / 2 : best;          // centre of the matching range, else nearest
    }
    switch (s.type) {
        case ValueType::Bool: return rawValue ? 127 : 0;
        case ValueType::Enum: {
            const int n = std::max<int>(1, (int)s.enums.size());
            return std::clamp((rawValue * 128 + 64) / n, 0, 127); // band centre
        }
        case ValueType::Continuous:
        default: return std::clamp(rawValue, 0, 127);
    }
}

int rawFromCcValue(const ParamSpec& s, int ccValue) {
    if (s.table != nullptr)                   // non-linear param: look it up
        return s.table[std::clamp(ccValue, 0, 127)] + s.displayOffset;
    switch (s.type) {
        case ValueType::Bool: return ccValue >= 64 ? 1 : 0;
        case ValueType::Enum: {
            const int n = std::max<int>(1, (int)s.enums.size());
            return std::clamp(ccValue * n / 128, 0, n - 1); // which band
        }
        case ValueType::Continuous:
        default: return std::clamp(ccValue, 0, 127);
    }
}

} // namespace ms2000
