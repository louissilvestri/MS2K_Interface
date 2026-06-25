// ModSeq — typed accessors for the MS2000 Mod Sequencer, which lives in the
// Timbre-1 parameter block at relative offset +52..+107 (absolute bytes 90..145).
//
// Confirmed byte-exact against the Korg MS2000 MIDI Implementation (TABLE 2 "SEQ"
// + TABLE 3 "SEQ PARAMETER") AND empirically against a real bank dump: patches
// with a live mod-seq (e.g. factory "Evolution") carry their step data here in
// Timbre 1, while Timbre 2's mirror region is left at the init default. So the
// sequencer is program-global and we read/write Timbre 1 only.
//
//   common  byte 90 : On/Off(b7) RunMode(b6) Resolution(b0-4)
//           byte 91 : LastStep(b4-7) SeqType(b2-3) KeySync(b0-1)
//   lane L  base = 92 + L*18 : Dest(+0) Motion(+1 b0) Step[0..15](+2..+17)
//           step value stored as 64 +/- 63  (display -63..+63, centre 0)
#pragma once

#include <array>
#include "Program.h"

namespace ms2000 {
namespace modseq {

constexpr int kCommon    = kTimbre1Base + 52; // 90
constexpr int kLane0     = kCommon + 2;       // 92
constexpr int kLaneStride = 18;
constexpr int kLanes     = 3;
constexpr int kSteps     = 16;
constexpr int kStepCentre = 64;

inline int laneBase(int lane) { return kLane0 + lane * kLaneStride; }
inline int clampStep(int x) { return x < 1 ? 1 : (x > 127 ? 127 : x); }

// ---- *T-7 resolution + *T-10 destination label tables (for the UI) ----------
inline const std::array<const char*, 16>& resolutionLabels() {
    static const std::array<const char*, 16> t{
        "1/48","1/32","1/24","1/16","1/12","3/32","1/8","1/6",
        "3/16","1/4","1/3","3/8","1/2","2/3","3/4","1/1" };
    return t;
}
inline const std::array<const char*, 31>& destLabels() {
    static const std::array<const char*, 31> t{
        "None","Pitch","StepLength","Portamento","OSC1 Ctrl1","OSC1 Ctrl2",
        "OSC2 Semi","OSC2 Tune","OSC1 Level","OSC2 Level","Noise Level","Cutoff",
        "Resonance","EG1 Int","KBD Track","Amp Level","Panpot","EG1 Attack",
        "EG1 Decay","EG1 Sustain","EG1 Release","EG2 Attack","EG2 Decay",
        "EG2 Sustain","EG2 Release","LFO1 Freq","LFO2 Freq","Patch1 Int",
        "Patch2 Int","Patch3 Int","Patch4 Int" };
    return t;
}

// ---- common parameters ------------------------------------------------------
inline bool on(const Program& p)          { return (p.bytes()[kCommon] >> 7) & 1; }
inline int  runMode(const Program& p)     { return (p.bytes()[kCommon] >> 6) & 1; }      // 0=1Shot,1=Loop
inline int  resolution(const Program& p)  { return p.bytes()[kCommon] & 0x1F; }          // 0-15 (*T-7)
inline int  lastStep(const Program& p)    { return (p.bytes()[kCommon + 1] >> 4) & 0x0F; }// 0-15 -> 1..16
inline int  seqType(const Program& p)     { return (p.bytes()[kCommon + 1] >> 2) & 0x03; }// 0-3
inline int  keySync(const Program& p)     { return p.bytes()[kCommon + 1] & 0x03; }       // 0-2

inline void setOn(Program& p, bool v)        { p.setRaw(kCommon, 7, 0x01, v ? 1 : 0); }
inline void setRunMode(Program& p, int v)    { p.setRaw(kCommon, 6, 0x01, v); }
inline void setResolution(Program& p, int v) { p.setRaw(kCommon, 0, 0x1F, v); }
inline void setLastStep(Program& p, int v)   { p.setRaw(kCommon + 1, 4, 0x0F, v); }
inline void setSeqType(Program& p, int v)    { p.setRaw(kCommon + 1, 2, 0x03, v); }
inline void setKeySync(Program& p, int v)    { p.setRaw(kCommon + 1, 0, 0x03, v); }

// ---- per-lane parameters (lane = 0..2) --------------------------------------
inline int  dest(const Program& p, int lane)   { return p.bytes()[laneBase(lane)]; }       // 0-30 (*T-10)
inline int  motion(const Program& p, int lane) { return p.bytes()[laneBase(lane) + 1] & 1; }// 0=Smooth,1=Step
inline void setDest(Program& p, int lane, int v)   { p.bytes()[laneBase(lane)] = (uint8_t)v; }
inline void setMotion(Program& p, int lane, int v) { p.setRaw(laneBase(lane) + 1, 0, 0x01, v); }

// step value as a centred signed quantity (-63..+63, 0 = no modulation)
inline int  step(const Program& p, int lane, int s) {
    return (int)p.bytes()[laneBase(lane) + 2 + s] - kStepCentre;
}
inline void setStep(Program& p, int lane, int s, int v) {
    p.bytes()[laneBase(lane) + 2 + s] = (uint8_t)clampStep(kStepCentre + v);
}

} // namespace modseq
} // namespace ms2000
