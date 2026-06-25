"""Listen on the UM-ONE input and summarize whatever the MS2000 sends."""
import sys, time, rtmidi

secs = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0
mi = rtmidi.MidiIn()
ports = mi.get_ports()
idx = next((n for n, p in enumerate(ports) if 'UM-ONE' in p), None)
print("IN ports:", ports, "-> using", idx)
if idx is None:
    print("FAIL: UM-ONE input not found"); sys.exit(1)
mi.open_port(idx)
mi.ignore_types(sysex=False, timing=True, active_sense=True)

print(f"Listening {secs:.0f}s — play notes / move a knob on the MS2000 now...")
notes = cc = sysex = other = 0
samples = []
t0 = time.time()
while time.time() - t0 < secs:
    m = mi.get_message()
    if not m:
        time.sleep(0.005); continue
    data = m[0]
    s = data[0] & 0xF0 if data and data[0] < 0xF0 else data[0]
    if s == 0x90: notes += 1
    elif s == 0xB0: cc += 1
    elif data[0] == 0xF0: sysex += 1
    else: other += 1
    if len(samples) < 12:
        samples.append(' '.join(f'{b:02X}' for b in data[:8]))
mi.close_port()
print(f"\nReceived -> notes:{notes}  CC:{cc}  sysex:{sysex}  other:{other}")
for s in samples:
    print("  ", s)
print("RESULT:", "INPUT WORKS" if (notes+cc+sysex+other) else "NOTHING RECEIVED")
