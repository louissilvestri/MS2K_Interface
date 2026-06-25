"""MS2000 MIDI mapping validator.

Modes:
  listen            decode + name every incoming CC/NRPN in real time
                    (you move a hardware control -> see what it transmits)
  sweep <cc>        send CC <cc> 0->127->64 so you can watch the synth respond
  send <cc> <val>   send a single CC value
  walk              sweep every mapped CC in order, labelled (you watch the synth)

Setup on the synth (Global mode, then WRITE):
  - MIDI Filter SystemEx = Enable
  - MIDI Filter Ctrl Change = Enable   (needed so knob moves TRANSMIT, and CCs are RECEIVED)
  - MIDI Ch = 1
"""
import sys, time
import rtmidi

CHAN = 0  # 0-based MIDI channel (Ch 1)

# Expected factory CC map: cc -> control name.
CC_MAP = {
    5:"Portamento Time", 7:"Amp Level (=Volume)", 10:"Amp Pan / Direct Level",
    12:"ModFX Speed", 13:"Delay Time", 14:"OSC1 Control 1", 15:"OSC1 Control 2",
    18:"OSC2 Semitone / Voc HPF Level", 19:"OSC2 Tune / Voc Threshold",
    20:"Mixer OSC1 Level", 21:"Mixer OSC2 / Inst Level", 22:"Mixer Noise Level",
    23:"EG1 Attack", 24:"EG1 Decay", 25:"EG1 Sustain", 26:"EG1 Release",
    27:"LFO1 Frequency", 28:"Patch1 Intensity", 29:"Patch2 Intensity",
    30:"Patch3 Intensity", 31:"Patch4 Intensity",
    70:"EG2 Sustain", 71:"Filter Resonance", 72:"EG2 Release", 73:"EG2 Attack",
    74:"Filter Cutoff", 75:"EG2 Decay", 76:"LFO2 Frequency",
    77:"OSC1 Wave (sel)", 78:"OSC2 Wave (sel)", 79:"Filter EG1 Int / Voc FC Mod Int",
    82:"OSC2 Mod (sel)", 83:"Filter Type / Voc Formant (sel)",
    85:"Filter KBD Track / Voc EF Sense", 86:"Amp EG2/Gate (sw)",
    87:"LFO1 Wave (sel)", 88:"LFO2 Wave (sel)", 89:"Mod-Seq On/Off (sw)",
    92:"Amp Distortion (sw)", 93:"ModFX Depth", 94:"Delay Depth",
    95:"Timbre Select (Global-assigned)",
    1:"Mod Wheel", 64:"Damper", 65:"Portamento Sw",
}
NRPN_MAP = {
    (0,2):"Arp On/Off", (0,3):"Arp Octaves", (0,4):"Arp Latch",
    (0,7):"Arp Type", (0,0x0A):"Arp Gate",
    (4,0):"Patch1 Source",(4,1):"Patch2 Source",(4,2):"Patch3 Source",(4,3):"Patch4 Source",
    (4,8):"Patch1 Dest",(4,9):"Patch2 Dest",(4,0x0A):"Patch3 Dest",(4,0x0B):"Patch4 Dest",
}

def port(cls, name):
    m = cls()
    for i in range(m.get_port_count()):
        if name.lower() in m.get_port_name(i).lower():
            return m, i
    return m, None

def open_out():
    m, i = port(rtmidi.MidiOut, "UM-ONE")
    if i is None: print("FAIL: UM-ONE output not found"); sys.exit(1)
    m.open_port(i); return m

def do_listen(secs=None):
    m, i = port(rtmidi.MidiIn, "UM-ONE")
    if i is None: print("FAIL: UM-ONE input not found"); sys.exit(1)
    m.open_port(i); m.ignore_types(sysex=True, timing=True, active_sense=True)
    end = (time.time() + secs) if secs else None
    print(f"Listening{f' for {secs:.0f}s' if secs else ''}. Move a hardware control (Ctrl rec.).\n")
    nrpn = {"msb":None,"lsb":None}
    seen = {}
    try:
        while end is None or time.time() < end:
            msg = m.get_message()
            if not msg: time.sleep(0.003); continue
            d = msg[0]
            if len(d) < 3 or (d[0] & 0xF0) != 0xB0:  # only CC
                continue
            cc, val = d[1], d[2]
            if cc == 99: nrpn["msb"] = val; continue
            if cc == 98: nrpn["lsb"] = val; continue
            if cc == 6 and nrpn["msb"] is not None:
                key = (nrpn["msb"], nrpn["lsb"]); name = NRPN_MAP.get(key, "??? unknown NRPN")
                print(f"  NRPN {nrpn['msb']:02X} {nrpn['lsb']:02X} = {val:<4} -> {name}")
                continue
            name = CC_MAP.get(cc, "??? UNMAPPED CC")
            tag = "" if cc in seen else "  <-- first"
            seen[cc] = name
            print(f"  CC {cc:<3} = {val:<4} -> {name}{tag}")
    except KeyboardInterrupt:
        pass
    print("\nDistinct CCs seen this session:")
    for cc in sorted(seen): print(f"  CC {cc:<3} {seen[cc]}")

def send_cc(out, cc, val):
    out.send_message([0xB0 | CHAN, cc & 0x7F, val & 0x7F])

def do_sweep(out, cc):
    name = CC_MAP.get(cc, "(unmapped)")
    print(f"Sweeping CC {cc} '{name}' 0 -> 127 -> 64 ...")
    for v in list(range(0,128,4)) + list(range(127,60,-4)):
        send_cc(out, cc, v); time.sleep(0.02)
    send_cc(out, cc, 64)

def do_walk(out):
    print("Watch the MS2000 LCD/sound. Each control sweeps for ~1.5 s.\n")
    for cc in sorted(CC_MAP):
        if cc in (1,64,65): continue
        print(f"  CC {cc:<3} {CC_MAP[cc]}")
        do_sweep(out, cc); time.sleep(0.4)
    print("\nDone. Note any that did NOT move on the synth.")

def main():
    if len(sys.argv) < 2:
        print(__doc__); return
    mode = sys.argv[1]
    if mode == "listen": do_listen(float(sys.argv[2]) if len(sys.argv) > 2 else None)
    elif mode == "sweep": do_sweep(open_out(), int(sys.argv[2]))
    elif mode == "send":  send_cc(open_out(), int(sys.argv[2]), int(sys.argv[3]))
    elif mode == "walk":  do_walk(open_out())
    else: print(__doc__)

if __name__ == "__main__":
    main()
