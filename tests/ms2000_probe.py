"""Probe the MS2000 over MIDI: request a dump and capture the reply.

Usage:  python tests/ms2000_probe.py [channel] [out.syx]
  channel : MIDI channel 1-16 (default 1) -- must match the synth's Global MIDI Ch.
  out.syx : where to save a full bank if one is received (default docs/synth_bank.syx)
"""
import sys, time
import rtmidi

KORG, MODEL = 0x42, 0x58

def korg_unpack(packed):
    out = bytearray()
    i = 0
    while i < len(packed):
        lead = packed[i]; i += 1
        for j in range(7):
            if i >= len(packed): break
            out.append(((lead >> j) & 1) << 7 | (packed[i] & 0x7F)); i += 1
    return bytes(out)

def find_port(ports, name):
    for i, p in enumerate(ports):
        if name.lower() in p.lower():
            return i
    return None

def name_of(prog_bytes):
    return bytes(b for b in prog_bytes[:12] if b != 0).decode('ascii', 'replace').rstrip()

def main():
    chan = (int(sys.argv[1]) - 1) if len(sys.argv) > 1 else 0
    out_path = sys.argv[2] if len(sys.argv) > 2 else "docs/synth_bank.syx"
    g = 0x30 | (chan & 0x0F)

    mo, mi = rtmidi.MidiOut(), rtmidi.MidiIn()
    oi = find_port(mo.get_ports(), "UM-ONE")
    ii = find_port(mi.get_ports(), "UM-ONE")
    print("OUT ports:", mo.get_ports(), "-> using", oi)
    print("IN  ports:", mi.get_ports(), "-> using", ii)
    if oi is None or ii is None:
        print("FAIL: UM-ONE not found on both in and out"); return 1
    mo.open_port(oi); mi.open_port(ii)
    mi.ignore_types(sysex=False, timing=True, active_sense=True)
    time.sleep(0.2)

    def collect(seconds):
        msgs, t0 = [], time.time()
        while time.time() - t0 < seconds:
            m = mi.get_message()
            if m:
                msgs.append(m[0]); t0 = time.time()  # keep waiting while data flows
            else:
                time.sleep(0.01)
        return msgs

    # 1) Current program dump request (0x10)
    print(f"\n[1] Requesting CURRENT program on ch {chan+1} (header F0 42 {g:02X} 58 10 F7)...")
    mo.send_message([0xF0, KORG, g, MODEL, 0x10, 0xF7])
    for m in collect(2.0):
        if len(m) > 5 and m[0] == 0xF0 and m[1] == KORG and m[4] == 0x40:
            prog = korg_unpack(bytes(m[5:-1]))
            print(f"    OK current program: '{name_of(prog)}'  ({len(m)} bytes)")
            break
    else:
        print("    no current-program reply.")

    # 2) All 128 programs (0x1C -> 0x4C). The ~37 KB dump arrives in several MME
    #    buffers, so flatten everything received and reassemble the F0..F7 frame.
    print(f"\n[2] Requesting ALL 128 programs (0x1C)...")
    mo.send_message([0xF0, KORG, g, MODEL, 0x1C, 0xF7])
    got = collect(10.0)
    total = sum(len(m) for m in got)
    print(f"    received {len(got)} buffer(s), {total} bytes total")
    flat = bytearray()
    for m in got:
        flat += bytes(m)
    # extract complete F0..F7 segments from the flattened stream
    segs, i = [], 0
    while i < len(flat):
        if flat[i] == 0xF0:
            j = i + 1
            while j < len(flat) and flat[j] != 0xF7:
                j += 1
            if j < len(flat):
                segs.append(bytes(flat[i:j+1])); i = j + 1
            else:
                break
        else:
            i += 1
    bank = next((s for s in segs if len(s) > 100 and s[1] == KORG and s[4] in (0x4C, 0x50)), None)
    if bank:
        data = korg_unpack(bytes(bank[5:-1]))
        n = len(data) // 254
        print(f"    OK bank: {n} programs ({len(bank)} sysex bytes). First='{name_of(data[:254])}' Last='{name_of(data[(n-1)*254:])}'")
        with open(out_path, "wb") as f:
            f.write(bytes(bank))
        print(f"    saved -> {out_path}")
    else:
        print(f"    no complete bank reassembled. sysex funcs seen:",
              [hex(s[4]) for s in segs if len(s) > 4])

    mo.close_port(); mi.close_port()
    print("\nIf you saw 'no reply': on the synth enable Global > MIDI Filter SystemEx, and make")
    print("sure the channel above matches the synth's Global MIDI Ch.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
