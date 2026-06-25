# Korg MS2000 — Authoritative MIDI Parameter Map

Single source of truth for the editor. Every UI control and every MIDI message is generated
from the data here. Byte offsets are transcribed from the Korg *MS2000/2000R MIDI
Implementation* and the structurally-identical microKORG implementation (Rev 1.4). Where the
two diverge it is called out — **the MS2000 adds a 16-step Mod Sequencer that the microKORG
lacks**, stored in regions the microKORG marks "dummy"; those exact offsets must be confirmed
against the MS2000-specific PDF before the Mod-Seq codec is trusted (see §7).

Model/IDs: Manufacturer `0x42` (Korg), Model `0x58` (MS2000 series), global channel in the
low nibble of byte 3 (`0x30 | ch`).

---

## 1. Message types the synth understands

### 1.1 Channel messages
- **Note On/Off, Pitch Bend, Program Change** — standard.
- **Control Change `cc = 0..95`** — the assignable real-time control path. Map is per-Global
  (see §5 for the factory default and §6 for the Global CC-map bytes).
- **NRPN** (`Bn 63 msb`, `Bn 62 lsb`, `Bn 06 data`) — reaches a set of params that have **no
  CC**: arpeggio on/off, octaves, latch, type, gate; the four virtual-patch *sources* and
  *destinations*; and the 16 vocoder band levels & pans. See §5.2.

### 1.2 System Exclusive — function IDs
Requests (host → synth):
| Func | Meaning |
|------|---------|
| `0x10` | Current Program Data Dump **Request** |
| `0x1C` | Program Data Dump Request (a stored program) |
| `0x0E` | Global Data Dump Request |
| `0x0F` | All Data (Program+Global) Dump Request |
| `0x11` | Program Write Request (`00`, then dest prog `0..127`) |

Data dumps (bidirectional):
| Func | Meaning | Payload |
|------|---------|---------|
| `0x40` | **Current Program Data Dump** (edit buffer) | 254 B → 291 B packed |
| `0x4C` | Program Data Dump (one stored program) | 254 B → 291 B packed |
| `0x51` | Global Data Dump | 200 B → 229 B packed |
| `0x50` | All Data Dump (128 progs + global) | 254×128+200 B → 37386 B packed |

Acks (synth → host): `0x23` load completed, `0x24` load error, `0x26` data-format error,
`0x21` write completed, `0x22` write error.

Header for every SysEx: `F0 42 3g 58 <func> … F7`.

### 1.3 Universal SysEx
- Device Inquiry: reply family LSB `0x58`. (MS2000 member id differs from microKORG's `0x11`.)
- Master Volume `F0 7F nn 04 01 vv mm F7`, Master Fine Tune `… 04 03 vv mm F7`.

---

## 2. The 7→8 (Korg) packing — REQUIRED for every dump

Synth stores 8-bit data; MIDI bytes must have bit7 = 0. Korg packs **7 data bytes into 8
transmitted bytes**: a leading byte collects the seven stripped MSBs, then the seven low-7-bit
bytes follow.

```
encode(group of up to 7 data bytes d[0..n-1], n<=7):
  lead = Σ ((d[i] >> 7) & 1) << i      // bit i = MSB of d[i]
  emit lead, then d[0]&0x7F .. d[n-1]&0x7F
```
254-byte program → 36 full groups (252 B) + 1 partial group of 2 B → `8*36 + (1+2) = 291`
encoded bytes. Decode is the inverse. **Forgetting this is the classic reason a dump "almost"
decodes.** Implemented in `Source/midi/SysexCodec.*` and covered by `tests/`.

---

## 3. Program data — TABLE 1 (254 bytes, 1 program / current program)

Offsets are into the **unpacked** 254-byte program.

| Byte | Bits | Parameter | Range / encoding |
|------|------|-----------|------------------|
| 0–11 | — | Program name | 12 ASCII chars |
| 12–13 | — | (dummy on µKORG; **MS2000 mod-seq area** — see §7) | |
| 14 | b0–2 | Arp **Trigger Length** | 0–7 = 1..8 steps |
| 15 | b0–7 | Arp **Trigger Pattern** | bit n = step n On/Off (1..8) |
| 16 | b4,5 | **Voice Mode** | **0=Single, 2=Layer, 3=Vocoder** (no Split) |
| 17 | b4–7 / b0–3 | Scale Key / Scale Type | 0=C / 0=Equal Temp |
| 18 | — | (dummy / MS2000 mod-seq) | |
| 19 | b7 / b0–3 | Delay **Sync** / **Time Base** | 0,1 / 0–14 (*T-1) |
| 20 | — | Delay **Time** | 0–127 |
| 21 | — | Delay **Depth** | 0–127 |
| 22 | — | Delay **Type** | 0–2 = Stereo/Cross/L-R |
| 23 | — | ModFX **LFO Speed** | 0–127 |
| 24 | — | ModFX **Depth** | 0–127 |
| 25 | — | ModFX **Type** | 0–2 = Cho-Flg/Ensemble/Phaser |
| 26 | — | EQ **Hi Freq** | 0–29 = 1.00..18.0 kHz (*T-10) |
| 27 | — | EQ **Hi Gain** | 64±12 = 0±12 dB |
| 28 | — | EQ **Low Freq** | 0–29 = 40..1000 Hz (*T-11) |
| 29 | — | EQ **Low Gain** | 64±12 = 0±12 dB |
| 30–31 | — | Arp/SEQ **Tempo** | MSB,LSB = 20–300 BPM |
| 32 | b7/b6/b4,5/b0 | Arp **On**/**Latch**/**Target**/**KeySync** | On·Latch 0/1; Target 0–2 Both/T1/T2; KeySync 0/1 |
| 33 | b0–3/b4–7 | Arp **Type** / **Range** | 0–5 Up..Trigger (*T-12) / 0–3 = 1..4 oct |
| 34 | — | Arp **Gate Time** | 0–100 % |
| 35 | — | Arp **Resolution** | 0–5 = 1/24,1/16,1/12,1/8,1/6,1/4 |
| 36 | — | Arp **Swing** | 0±100 % |
| 37 | — | **KBD Octave** | -3..+3 |
| 38–145 | — | **TIMBRE 1** | TABLE 2 |
| 146–253 | — | **TIMBRE 2** (Layer only) | TABLE 2 |
| 38–141 | — | **VOCODER** (Vocoder mode replaces T1/T2) | TABLE 3 |

## 4. Timbre data — TABLE 2 (offsets relative to timbre base, +0 = 38 or 146)

| +Off | Bits | Parameter | Range |
|------|------|-----------|-------|
| 0 | — | MIDI ch | -1 = use Global |
| 1 | b6,7 / b5 / b4 / b3 / b0,1 | Assign Mode / EG2 reset / EG1 reset / Trigger Mode / Key Priority | 0–2 Mono/Poly/Unison · 0/1 · 0/1 · 0/1 Single/Multi · 0=Last |
| 2 | — | Unison Detune | 0–99 cent |
| 3 | — | Pitch **Tune** | 64±50 cent |
| 4 | — | Pitch **Bend Range** | 64±12 semitone |
| 5 | — | **Transpose** | 64±24 semitone |
| 6 | — | **Vibrato Int** | 64±63 |
| 7 | — | **OSC1 Wave** | 0–7 (*T-2) Saw/Pulse/Tri/Sin/Vox/DWGS/Noise/AudioIn |
| 8 | — | OSC1 **Control1** | 0–127 |
| 9 | — | OSC1 **Control2** | 0–127 |
| 10 | — | OSC1 **DWGS Wave** | 0–63 = DWGS 1..64 (when Wave=DWGS) |
| 11 | — | (dummy) | |
| 12 | b4,5 / b0,1 | OSC2 **Mod Select** / **Wave** | 0–3 Off/Ring/Sync/RingSync · 0–2 Saw/Squ/Tri |
| 13 | — | OSC2 **Semitone** | 64±24 |
| 14 | — | OSC2 **Tune** | 64±63 |
| 15 | b0–6 | **Portamento Time** | 0–127 |
| 16 | — | Mixer **OSC1 Level** | 0–127 |
| 17 | — | Mixer **OSC2 Level** | 0–127 |
| 18 | — | Mixer **Noise Level** | 0–127 |
| 19 | — | Filter **Type** | 0–3 = 24LPF/12LPF/12BPF/12HPF |
| 20 | — | Filter **Cutoff** | 0–127 |
| 21 | — | Filter **Resonance** | 0–127 |
| 22 | — | Filter **EG1 Intensity** | 64±63 |
| 23 | — | Filter **Velocity Sense** | 64=0 |
| 24 | — | Filter **Keyboard Track** | 64±63 |
| 25 | — | Amp **Level** | 0–127 |
| 26 | — | Amp **Panpot** | 0–64–127 = L64..CNT..R63 |
| 27 | b6 / b0 | Amp **SW** (EG2/Gate) / **Distortion** | 0=EG2 / 0,1 Off/On |
| 28 | — | Amp **Velocity Sense** | 64=0 |
| 29 | — | Amp **Keyboard Track** | 64±63 |
| 30–33 | — | **EG1** A/D/S/R | 0–127 each |
| 34–37 | — | **EG2** A/D/S/R | 0–127 each |
| 38 | b4,5 / b0,1 | **LFO1** KeySync / Wave | 0–2 Off/Timbre/Voice · 0–3 Saw/Squ/Tri/S-H |
| 39 | — | LFO1 **Frequency** | 0–127 |
| 40 | b7 / b0–4 | LFO1 **Tempo Sync** / **Sync Note** | 0/1 · 0–14 (*T-5) |
| 41 | b4,5 / b0,1 | **LFO2** KeySync / Wave | 0–2 · 0–3 Saw/Squ(+)/Sin/S-H |
| 42 | — | LFO2 **Frequency** | 0–127 |
| 43 | b7 / b0–4 | LFO2 Tempo Sync / Sync Note | 0/1 · 0–14 (*T-5) |
| 44 | b4–7 / b0–3 | **Patch1** Dest / Source | 0–7 (*T-4) / 0–7 (*T-3) |
| 45 | — | Patch1 **Intensity** | 64±63 |
| 46–47 | — | **Patch2** Dest/Source, Intensity | as Patch1 |
| 48–49 | — | **Patch3** Dest/Source, Intensity | as Patch1 |
| 50–51 | — | **Patch4** Dest/Source, Intensity | as Patch1 |
| 52–107 | — | (dummy on µKORG; **MS2000 Mod-Seq** — §7) | |

## 5. Vocoder data — TABLE 3 (base = 38, Vocoder mode)
Shares +0..+10 with TABLE 2 (voice/pitch/OSC1). Then: +12 b0 Audio-In1 HPF Gate; +14
Portamento; +15 OSC1 Level, +16 **Ext (Inst) Level**, +17 Noise Level; +18 **HPF Level**,
+19 **Gate Sense**, +20 **Threshold**; +21 **Formant Shift** (0–4 = 0/+1/+2/-1/-2), +22
Filter **Cutoff** (64±63), +23 **Resonance**, +24 **Mod Source** (1–7, *T-13), +25 **Mod
Intensity** (64±63), +26 **E.F. Sense** (0–126, 127=Hold); +27 Amp Level, +28 **Direct
Level**, +29 b0 Distortion, +30 Vel Sense, +31 KeyTrack; +32–35 EG1 (fixed 0/0/127/0), +36–39
EG2 A/D/S/R; +40–45 LFO1/LFO2 (as TABLE 2); **+46–61 = 16 band Levels** (0–127); **+62–77 =
16 band Pans** (1–64–127 = L63..CNT..R63); +78–141 E.F. Hold levels (used when E.F.Sense=Hold).

---

## 5.1 Default CC map (factory)
Portamento 5 · OSC1 Wave 77, Ctrl1 14, Ctrl2 15 · OSC2 Wave 78, ModType 82, Semitone 18,
Tune 19 · Mixer OSC1 20, OSC2 21, Noise 22 · Filter Type 83, Cutoff 74, Reso 71, EG1Int 79,
KbdTrk 85 · Amp Level 7, Pan 10, EG2/Gate 86, Distortion 92 · EG1 A23 D24 S25 R26 · EG2 A73
D75 S70 R72 · LFO1 Wave 87 Freq 27 · LFO2 Wave 88 Freq 76 · Patch1-4 Int 28/29/30/31 ·
SEQ on/off 89 · ModFX Speed 12 Depth 93 · Delay Time 13 Depth 94. Banded selectors map their
index to the centre of the matching 0–127 band (e.g. OSC1 Wave: Saw 0–15 … AudioIn 112–127).
**Timbre Select** is assignable (Global byte 11 *TimbSel Ctrl No*) — **CC 95** on this unit
(value 0=Timbre 1, 1=Timbre 1&2, 2–127=Timbre 2). Validated 2026-06-23; see
`docs/midi_validation_report.csv`.

## 5.2 NRPN map (MSB,LSB hex)
`00 02` Arp On/Off · `00 03` Arp Octaves · `00 04` Arp Latch · `00 07` Arp Type · `00 0A`
Arp Gate · `04 00..03` Patch1–4 Source · `04 08..0B` Patch1–4 Destination · `04 10,12..1E`
Vocoder Band 1–8 Level · `04 20..2E` Band 1–8 Pan (bands 9–16 continue the pattern).

---

## 6. Global data — TABLE 6 (200 bytes) — relevant for the CC-map editor
Byte 9 b0–3 MIDI Ch; byte 10 Sync Ctrl No.; byte 11 Timbre-Sel Ctrl No.; bytes **18–59** =
the 42-entry **Knob & Switch CC-map** (`-1=OFF, 0–95=CC#`) in the order listed in *T-9
(Portamento, OSC1 Wave, … Delay Time). The editor reads this to learn the user's actual CC
assignments instead of assuming the factory map.

## 7. ✅ MS2000-specific Mod Sequencer (CONFIRMED — implemented in Source/model/ModSeq.h)
The MS2000 Mod Sequencer (3 lanes × 16 steps) lives in the **Timbre-1 parameter block** at
relative offset **+52..+107** (absolute bytes **90..145**). Confirmed byte-exact against the
Korg MS2000 MIDI Implementation (TABLE 2 "SEQ" + TABLE 3 "SEQ PARAMETER", transcribed in
`docs/ref/MS2000_MIDIimp.TXT`) **and** empirically against the real bank dump
(`docs/synth_bank.syx`): factory "Evolution" (prog 2) carries a live step curve here in
Timbre 1 while Timbre 2's mirror region stays at the init default — so the sequencer is
program-global and we read/write **Timbre 1 only**.

```
byte 90 (+52):  b7 On/Off · b6 Run Mode(0=1Shot,1=Loop) · b0-4 Resolution (0-15, *T-7)
byte 91 (+53):  b4-7 Last Step(0-15=1..16) · b2-3 Seq Type(0-3=Fwd,Rev,Alt1,Alt2) · b0-1 Key Sync(0-2=Off,Timbre,Voice)
lane L (0..2) base = 92 + L*18:
   +0  Destination (0-30, *T-10: None..Patch4Int)
   +1  b0 Motion Type (0=Smooth,1=Step)
   +2..+17  Step Value[0..15], stored 64±63 (display -63..+63, centre 0)
```
*T-7 resolution: 1/48,1/32,1/24,1/16,1/12,3/32,1/8,1/6,3/16,1/4,1/3,3/8,1/2,2/3,3/4,1/1
*T-10 dest (0-30): None,Pitch,StepLength,Portamento,OSC1Ctrl1,OSC1Ctrl2,OSC2Semi,OSC2Tune,
OSC1Level,OSC2Level,NoiseLevel,Cutoff,Resonance,EG1Int,KBDTrack,AmpLevel,Panpot,EG1A,EG1D,
EG1S,EG1R,EG2A,EG2D,EG2S,EG2R,LFO1Freq,LFO2Freq,Patch1Int,Patch2Int,Patch3Int,Patch4Int

NRPN exists for SEQ1 step levels (04 10–1F) and SEQ2 step pans (04 20–2F), but the data-entry
scaling (*2-5) is destination-dependent, so the editor sends mod-seq edits via a coalesced
full Current-Program dump (always byte-correct). Verified by `tests/verify_modseq_hw.cpp`
(60/60 values round-trip through the real synth).

## Scaling tables
- *T-1 Delay base: 0=1/32 … 14=1/1 · *T-5 LFO sync: 0=1/1 … 14=1/32 · *T-2 OSC1 wave list ·
  *T-3 Patch source {EG1,EG2,LFO1,LFO2,Velocity,KbdTrack,P.Bend,Mod} · *T-4 Patch dest
  {Pitch,OSC2Pitch,OSC1Ctrl1,NoiseLvl,Cutoff,Amp,Pan,LFO2Freq} · *T-12 Arp type
  {Up,Down,Alt1,Alt2,Random,Trigger} · *T-13 Vocoder filter-mod source {---,AEG,LFO1,LFO2,
  Velocity,KbdTrack,P.Bend,Mod}. Full numeric tables T-10/T-11 (EQ freq) in the Korg PDF.
