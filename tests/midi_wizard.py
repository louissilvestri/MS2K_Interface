"""MS2000 MIDI mapping wizard.

Walks every CC/NRPN-mappable hardware control one at a time, in signal-flow
order, telling you exactly which knob/button to move (incl. multiplexed ones).
Per control:
  - Listen : you move the control; the wizard captures + identifies the CC/NRPN.
  - Send   : the wizard sweeps the expected CC; you confirm the synth responds.

On launch you choose a NEW report or APPEND/resume the existing one, and you can
JUMP straight to any step. A "+ Custom" step lets you name and probe any control
that isn't in the list. Results save to docs/midi_validation_report.csv.

Synth setup (Global, then WRITE): SysEx = Enable, Ctrl Change = Enable, Ch 1.
Close the editor app first (MIDI ports are exclusive).
"""
import os, csv, collections
import tkinter as tk
from tkinter import ttk, messagebox
import rtmidi

CHAN = 0
PORT = "UM-ONE"
REPORT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "docs", "midi_validation_report.csv"))

C = lambda n, i, cc: {"name": n, "instr": i, "cc": cc, "nrpn": None, "custom": False}
N = lambda n, i, msb, lsb: {"name": n, "instr": i, "cc": None, "nrpn": (msb, lsb), "custom": False}

CONTROLS = [
    C("OSC1 Wave", "OSC-1: press the WAVE button to step through waveforms.", 77),
    C("OSC1 Control 1", "OSC-1: turn the CONTROL 1 knob.", 14),
    C("OSC1 Control 2", "OSC-1: turn the CONTROL 2 knob.", 15),
    C("OSC2 Wave", "OSC-2: press the WAVE button.", 78),
    C("OSC2 Osc Mod", "OSC-2: press the OSC MOD button.", 82),
    C("OSC2 Semitone", "OSC-2: turn the SEMITONE knob.", 18),
    C("OSC2 Tune", "OSC-2: turn the TUNE knob.", 19),
    C("Mixer OSC1 Level", "MIXER: turn the OSC 1 knob.", 20),
    C("Mixer OSC2 Level", "MIXER: turn the OSC 2 knob.", 21),
    C("Mixer Noise Level", "MIXER: turn the NOISE knob.", 22),
    C("Filter Type", "FILTER: press the TYPE button.", 83),
    C("Filter Cutoff", "FILTER: turn the CUTOFF knob.", 74),
    C("Filter Resonance", "FILTER: turn the RESONANCE knob.", 71),
    C("Filter EG1 Int", "FILTER: turn the EG1 INT knob.", 79),
    C("Filter KBD Track", "FILTER: turn the KBD TRACK knob.", 85),
    C("Amp Level", "AMP: turn the LEVEL knob.", 7),
    C("Amp Pan", "AMP: turn the PANPOT knob.", 10),
    C("Amp EG2/Gate", "AMP: press the EG2/GATE button.", 86),
    C("Amp Distortion", "AMP: press the DISTORTION button.", 92),
    C("EG1 Attack", "EG 1 (FILTER): turn ATTACK.", 23),
    C("EG1 Decay", "EG 1 (FILTER): turn DECAY.", 24),
    C("EG1 Sustain", "EG 1 (FILTER): turn SUSTAIN.", 25),
    C("EG1 Release", "EG 1 (FILTER): turn RELEASE.", 26),
    C("EG2 Attack", "EG 2 (AMP): turn ATTACK.", 73),
    C("EG2 Decay", "EG 2 (AMP): turn DECAY.", 75),
    C("EG2 Sustain", "EG 2 (AMP): turn SUSTAIN.", 70),
    C("EG2 Release", "EG 2 (AMP): turn RELEASE.", 72),
    C("LFO1 Wave", "LFO 1: press the WAVE button.", 87),
    C("LFO1 Frequency", "LFO 1: turn the FREQUENCY knob.", 27),
    C("LFO2 Wave", "LFO 2: press the WAVE button.", 88),
    C("LFO2 Frequency", "LFO 2: turn the FREQUENCY knob.", 76),
    C("Portamento Time", "PORTAMENTO: turn the TIME knob.", 5),
    C("Patch 1 Intensity", "VIRTUAL PATCH: turn the PATCH 1 knob.", 28),
    C("Patch 2 Intensity", "VIRTUAL PATCH: turn the PATCH 2 knob.", 29),
    C("Patch 3 Intensity", "VIRTUAL PATCH: turn the PATCH 3 knob.", 30),
    C("Patch 4 Intensity", "VIRTUAL PATCH: turn the PATCH 4 knob.", 31),
    C("Mod FX Speed", "EFFECTS: set MOD/DELAY to MOD, then turn SPEED/TIME.", 12),
    C("Mod FX Depth", "EFFECTS (MOD selected): turn DEPTH/FEEDBACK.", 93),
    C("Delay Time", "EFFECTS: set MOD/DELAY to DELAY, then turn SPEED/TIME.", 13),
    C("Delay Depth", "EFFECTS (DELAY selected): turn DEPTH/FEEDBACK.", 94),
    C("Mod-Seq On/Off", "MOD SEQUENCE: press the ON/OFF button.", 89),
    C("Timbre Select", "Press the TIMBRE SELECT button (cycles Timbre 1 / 1&2 / 2).", 95),
    N("Arp On/Off", "ARPEGGIATOR: press the ON/OFF button.", 0, 2),
    N("Arp Latch", "ARPEGGIATOR: press the LATCH button.", 0, 4),
    N("Arp Range (Octaves)", "ARPEGGIATOR: change the RANGE (octave) setting.", 0, 3),
    N("Arp Type", "ARPEGGIATOR: change the TYPE.", 0, 7),
    N("Arp Gate", "ARPEGGIATOR: turn the GATE knob.", 0, 0x0A),
    N("Patch 1 Source", "VIRTUAL PATCH: select PATCH 1, change its SOURCE.", 4, 0),
    N("Patch 1 Dest", "VIRTUAL PATCH: select PATCH 1, change its DESTINATION.", 4, 8),
]


class Wizard:
    def __init__(self, root):
        self.root = root
        root.title("MS2000 MIDI Mapping Wizard")
        root.geometry("620x600")
        self.controls = [dict(c) for c in CONTROLS]
        self.results = {}           # idx -> {"detected":str, "send_ok":int}
        self.idx = 0
        self.listening = False
        self.cc_tally = collections.Counter(); self.cc_minmax = {}
        self.nrpn_tally = collections.Counter()
        self._nmsb = self._nlsb = None   # PERSISTENT NRPN address (key fix)

        self.out = rtmidi.MidiOut(); self.min = rtmidi.MidiIn()
        self.oi = self._find(self.out); self.ii = self._find(self.min)
        if self.oi is not None: self.out.open_port(self.oi)
        if self.ii is not None:
            self.min.open_port(self.ii)
            self.min.ignore_types(sysex=True, timing=True, active_sense=True)

        self._build_ui()
        self._maybe_resume()
        self._refresh_jump()
        self._show()
        self.root.after(15, self._poll)

    def _find(self, m):
        for i in range(m.get_port_count()):
            if PORT.lower() in m.get_port_name(i).lower():
                return i
        return None

    # ---------- startup: new vs append ----------
    def _maybe_resume(self):
        if not os.path.exists(REPORT):
            return
        if not messagebox.askyesno("Resume report?",
                "An existing report was found at:\n" + REPORT +
                "\n\nAppend / resume it?  (No = start a new report)"):
            return
        try:
            with open(REPORT, newline="") as f:
                rows = list(csv.reader(f))
        except Exception:
            return
        for row in rows[1:]:
            if len(row) < 5:
                continue
            try:
                step = int(row[0])
            except ValueError:
                continue
            name, expected, detected, sent = row[1], row[2], row[3], row[4]
            if step - 1 < len(CONTROLS):
                idx = step - 1
            else:  # restore a custom control
                self.controls.append({"name": name, "instr": "", "cc": None,
                                      "nrpn": None, "custom": True})
                idx = len(self.controls) - 1
            self.results[idx] = {"detected": detected, "send_ok": 1 if sent.strip().lower() == "yes" else 0}

    # ---------- UI ----------
    def _build_ui(self):
        pad = dict(padx=14, pady=4)
        top = ttk.Frame(self.root); top.pack(fill="x", **pad)
        self.prog = ttk.Label(top, text="", font=("Segoe UI", 10)); self.prog.pack(side="left")
        ttk.Label(top, text="Jump to:").pack(side="left", padx=(16, 4))
        self.jump = ttk.Combobox(top, width=34, state="readonly")
        self.jump.pack(side="left"); self.jump.bind("<<ComboboxSelected>>", self._on_jump)
        self.bar = ttk.Progressbar(self.root); self.bar.pack(fill="x", padx=14)

        self.name = ttk.Entry(self.root, font=("Segoe UI", 18, "bold"))
        self.name.pack(fill="x", padx=14, pady=(12, 2))
        self.instr = ttk.Label(self.root, text="", font=("Segoe UI", 11), wraplength=580, justify="left")
        self.instr.pack(anchor="w", padx=14)
        self.expect = ttk.Label(self.root, text="", font=("Segoe UI", 11, "bold"), foreground="#1a6")
        self.expect.pack(anchor="w", padx=14, pady=(6, 8))

        f1 = ttk.LabelFrame(self.root, text="1) You move it  (synth -> app)")
        f1.pack(fill="x", padx=14, pady=6)
        self.listen_btn = ttk.Button(f1, text="▶  Listen", command=self._toggle_listen)
        self.listen_btn.grid(row=0, column=0, padx=8, pady=8)
        self.detected = ttk.Label(f1, text="Detected: —", font=("Consolas", 11))
        self.detected.grid(row=0, column=1, sticky="w")
        self.lstatus = ttk.Label(f1, text="", font=("Segoe UI", 11, "bold"))
        self.lstatus.grid(row=1, column=0, columnspan=2, sticky="w", padx=8, pady=(0, 6))

        f2 = ttk.LabelFrame(self.root, text="2) I move it  (app -> synth)")
        f2.pack(fill="x", padx=14, pady=6)
        self.send_btn = ttk.Button(f2, text="▶  Send Test sweep", command=self._send_test)
        self.send_btn.grid(row=0, column=0, padx=8, pady=8)
        self.send_ok = tk.IntVar()
        ttk.Checkbutton(f2, text="Synth responded", variable=self.send_ok).grid(row=0, column=1, sticky="w")

        nav = ttk.Frame(self.root); nav.pack(fill="x", padx=14, pady=12)
        ttk.Button(nav, text="◀ Back", command=self._back).pack(side="left")
        ttk.Button(nav, text="Skip", command=self._skip).pack(side="left", padx=6)
        ttk.Button(nav, text="Record & Next ▶", command=self._next).pack(side="left", padx=6)
        ttk.Button(nav, text="＋ Custom", command=self._add_custom).pack(side="left", padx=6)
        ttk.Button(nav, text="Save Report", command=self._save).pack(side="right")

        port_txt = f"Out: {'UM-ONE' if self.oi is not None else 'NOT FOUND'}    " \
                   f"In: {'UM-ONE' if self.ii is not None else 'NOT FOUND'}    Report: {REPORT}"
        ttk.Label(self.root, text=port_txt, font=("Segoe UI", 8), foreground="#888",
                  wraplength=590, justify="left").pack(anchor="w", padx=14)

    def _refresh_jump(self):
        vals = []
        for i, c in enumerate(self.controls):
            done = "✓ " if i in self.results else "  "
            vals.append(f"{done}{i+1}. {c['name']}")
        self.jump["values"] = vals

    def _show(self):
        c = self.controls[self.idx]
        self.bar.config(maximum=len(self.controls), value=self.idx + 1)
        self.prog.config(text=f"Step {self.idx+1} / {len(self.controls)}")
        self.jump.set(f"{'✓ ' if self.idx in self.results else '  '}{self.idx+1}. {c['name']}")

        self.name.config(state="normal")
        self.name.delete(0, "end"); self.name.insert(0, c["name"])
        if c["custom"]:
            self.instr.config(text="Type a field name above, click Listen, then move ANY control.")
            self.expect.config(text="Expected: (unmapped — discover it)")
        else:
            self.name.config(state="readonly")
            self.instr.config(text=c["instr"])
            exp = f"CC {c['cc']}" if c["cc"] is not None else f"NRPN {c['nrpn'][0]:02X} {c['nrpn'][1]:02X}"
            self.expect.config(text=f"Expected: {exp}")

        self._reset_capture()
        prev = self.results.get(self.idx)
        self.send_ok.set(prev["send_ok"] if prev else 0)
        if prev and prev["detected"] not in ("", "(skipped)"):
            self.detected.config(text=f"Detected: {prev['detected']}  (saved)")
        else:
            self.detected.config(text="Detected: —")
        self.lstatus.config(text="Click Listen, then move the control.", foreground="#555")

    def _reset_capture(self):
        self.listening = False
        self.listen_btn.config(text="▶  Listen")
        self.cc_tally.clear(); self.cc_minmax.clear(); self.nrpn_tally.clear()
        # NOTE: _nmsb/_nlsb persist on purpose, so data-only NRPN updates resolve.

    # ---------- MIDI ----------
    def _poll(self):
        if self.ii is not None:
            while True:
                m = self.min.get_message()
                if not m: break
                if self.listening: self._consume(m[0])
        if self.listening: self._update_detected()
        self.root.after(15, self._poll)

    def _consume(self, d):
        if len(d) < 3 or (d[0] & 0xF0) != 0xB0: return
        cc, val = d[1], d[2]
        if cc == 99: self._nmsb = val; return
        if cc == 98: self._nlsb = val; return
        if cc == 6:
            if self._nmsb is not None:
                self.nrpn_tally[(self._nmsb, self._nlsb)] += 1
            return
        if cc == 38: return  # data-entry LSB, ignore
        self.cc_tally[cc] += 1
        lo, hi = self.cc_minmax.get(cc, (val, val))
        self.cc_minmax[cc] = (min(lo, val), max(hi, val))

    def _update_detected(self):
        c = self.controls[self.idx]
        best_cc = self.cc_tally.most_common(1)
        best_n = self.nrpn_tally.most_common(1)
        cc_n = best_cc[0][1] if best_cc else 0
        n_n = best_n[0][1] if best_n else 0
        if cc_n == 0 and n_n == 0:
            self.detected.config(text="Detected: (waiting…)"); return
        if cc_n >= n_n:
            cc = best_cc[0][0]; lo, hi = self.cc_minmax[cc]
            self.detected.config(text=f"Detected: CC {cc}   ({cc_n} msgs, val {lo}-{hi})")
            self._status(c, "CC", cc)
        else:
            a, b = best_n[0][0]
            self.detected.config(text=f"Detected: NRPN {a:02X} {b:02X}   ({n_n} msgs)")
            self._status(c, "NRPN", (a, b))

    def _status(self, c, kind, got):
        if c["custom"]:
            self.lstatus.config(text="Detected — click Record to save it.", foreground="#06c"); return
        if kind == "CC":
            ok = (c["cc"] == got); exp = f"CC {c['cc']}" if c["cc"] is not None else f"NRPN {c['nrpn'][0]:02X} {c['nrpn'][1]:02X}"
            gs = f"CC {got}"
        else:
            ok = (c["nrpn"] == got); exp = f"NRPN {c['nrpn'][0]:02X} {c['nrpn'][1]:02X}" if c["nrpn"] else f"CC {c['cc']}"
            gs = f"NRPN {got[0]:02X} {got[1]:02X}"
        if ok: self.lstatus.config(text=f"✓ MATCH  ({gs})", foreground="#1a7a1a")
        else:  self.lstatus.config(text=f"✗ got {gs}, expected {exp}", foreground="#b00")

    def _toggle_listen(self):
        self.listening = not self.listening
        if self.listening:
            self.cc_tally.clear(); self.cc_minmax.clear(); self.nrpn_tally.clear()
            self.listen_btn.config(text="■  Stop")
            self.lstatus.config(text="Listening… move the control now.", foreground="#06c")
        else:
            self.listen_btn.config(text="▶  Listen")

    def _send_test(self):
        if self.oi is None: return
        c = self.controls[self.idx]
        vals = list(range(0, 128, 6)) + list(range(126, 60, -6)) + [64]
        def step(i):
            if i >= len(vals): return
            v = vals[i]
            if c["cc"] is not None:
                self.out.send_message([0xB0 | CHAN, c["cc"], v])
            elif c["nrpn"] is not None:
                a, b = c["nrpn"]
                self.out.send_message([0xB0 | CHAN, 99, a])
                self.out.send_message([0xB0 | CHAN, 98, b])
                self.out.send_message([0xB0 | CHAN, 6, v])
            self.root.after(18, lambda: step(i + 1))
        step(0)

    # ---------- navigation ----------
    def _record(self):
        c = self.controls[self.idx]
        if c["custom"]:
            c["name"] = self.name.get().strip() or c["name"]
        det = self.detected.cget("text").replace("Detected: ", "").replace("  (saved)", "")
        if det in ("—", "(waiting…)"): det = ""
        self.results[self.idx] = {"detected": det, "send_ok": self.send_ok.get()}
        self._refresh_jump()

    def _next(self):
        self._record()
        if self.idx < len(self.controls) - 1:
            self.idx += 1; self._show()
        else:
            self._save()
            self.lstatus.config(text="Done — report saved.", foreground="#1a7a1a")

    def _back(self):
        if self.idx > 0:
            self._record(); self.idx -= 1; self._show()

    def _skip(self):
        self.results[self.idx] = {"detected": "(skipped)", "send_ok": 0}
        self._refresh_jump(); self._next()

    def _on_jump(self, _evt):
        sel = self.jump.current()
        if sel >= 0:
            self._record(); self.idx = sel; self._show()

    def _add_custom(self):
        self._record()
        n = sum(1 for c in self.controls if c["custom"]) + 1
        self.controls.append({"name": f"Custom {n}", "instr": "", "cc": None, "nrpn": None, "custom": True})
        self.idx = len(self.controls) - 1
        self._refresh_jump(); self._show()
        self.name.focus_set()

    def _save(self):
        self._record()
        with open(REPORT, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["Step", "Control", "Expected", "Detected", "Send Responded"])
            for i, c in enumerate(self.controls):
                if c["custom"]:
                    exp = "(unmapped)"
                elif c["cc"] is not None:
                    exp = f"CC {c['cc']}"
                else:
                    exp = f"NRPN {c['nrpn'][0]:02X} {c['nrpn'][1]:02X}"
                r = self.results.get(i, {"detected": "", "send_ok": 0})
                w.writerow([i + 1, c["name"], exp, r["detected"], "yes" if r["send_ok"] else ""])


def main():
    root = tk.Tk()
    try:
        Wizard(root)
    except Exception as e:
        tk.Label(root, text=f"Error: {e}", fg="red", wraplength=520).pack(padx=20, pady=20)
    root.mainloop()


if __name__ == "__main__":
    main()
