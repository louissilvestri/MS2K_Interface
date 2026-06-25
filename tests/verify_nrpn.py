"""Round-trip test of NRPN SEND: toggle Arp On/Off via NRPN (sent as three
separate CC messages, like the app now does), then read the program byte back."""
import rtmidi, time

def korg_unpack(p):
    out = bytearray(); i = 0
    while i < len(p):
        lead = p[i]; i += 1
        for j in range(7):
            if i >= len(p): break
            out.append(((lead >> j) & 1) << 7 | (p[i] & 0x7F)); i += 1
    return bytes(out)

def port(cls, n):
    m = cls()
    for i in range(m.get_port_count()):
        if n.lower() in m.get_port_name(i).lower(): return m, i
    return m, None

mo, oi = port(rtmidi.MidiOut, "UM-ONE"); mi, ii = port(rtmidi.MidiIn, "UM-ONE")
mo.open_port(oi); mi.open_port(ii); mi.ignore_types(sysex=False, timing=True, active_sense=True)
time.sleep(0.2)

def collect(s):
    flat = bytearray(); t = time.time()
    while time.time() - t < s:
        m = mi.get_message()
        if m: flat += bytes(m[0]); t = time.time()
        else: time.sleep(0.01)
    return bytes(flat)

def current():
    mo.send_message([0xF0,0x42,0x30,0x58,0x10,0xF7])
    d = collect(1.5); i = d.find(b'\xf0')
    while i >= 0:
        j = d.find(b'\xf7', i)
        if j < 0: break
        m = d[i:j+1]
        if len(m) > 6 and m[4] == 0x40: return korg_unpack(m[5:-1])[:254]
        i = d.find(b'\xf0', j)
    return None

def arp_on(prog): return (prog[32] >> 7) & 1   # byte 32 bit7 = Arp On/Off

def send_nrpn(msb, lsb, val):     # three separate CC messages
    mo.send_message([0xB0, 99, msb]); mo.send_message([0xB0, 98, lsb]); mo.send_message([0xB0, 6, val])

for val, label in [(127, "ON"), (0, "OFF")]:
    send_nrpn(0x00, 0x02, val); time.sleep(0.4)
    p = current()
    print(f"NRPN arp {label} (val {val}) -> program Arp-On bit = {arp_on(p) if p else '?'}")
print("PASS if ON->1 and OFF->0 above")
