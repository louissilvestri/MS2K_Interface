"""Round-trip test of SysEx SEND: push a different patch to the edit buffer,
then read the current program back to confirm it changed."""
import rtmidi, time

def korg_unpack(p):
    out = bytearray(); i = 0
    while i < len(p):
        lead = p[i]; i += 1
        for j in range(7):
            if i >= len(p): break
            out.append(((lead >> j) & 1) << 7 | (p[i] & 0x7F)); i += 1
    return bytes(out)

def korg_pack(d):
    out = bytearray()
    for i in range(0, len(d), 7):
        chunk = d[i:i+7]; lead = 0
        for j, b in enumerate(chunk): lead |= ((b >> 7) & 1) << j
        out.append(lead)
        for b in chunk: out.append(b & 0x7F)
    return bytes(out)

def nm(p): return bytes(b for b in p[:12] if b).decode('ascii', 'replace').rstrip()

def port(cls, n):
    m = cls()
    for i in range(m.get_port_count()):
        if n.lower() in m.get_port_name(i).lower(): return m, i
    return m, None

mo, oi = port(rtmidi.MidiOut, "UM-ONE"); mi, ii = port(rtmidi.MidiIn, "UM-ONE")
mo.open_port(oi); mi.open_port(ii); mi.ignore_types(sysex=False, timing=True, active_sense=True)
time.sleep(0.2)

def collect(secs):
    flat = bytearray(); t = time.time()
    while time.time() - t < secs:
        m = mi.get_message()
        if m: flat += bytes(m[0]); t = time.time()
        else: time.sleep(0.01)
    return bytes(flat)

def seg(d, func):
    i = d.find(b'\xf0')
    while i >= 0:
        j = d.find(b'\xf7', i)
        if j < 0: break
        m = d[i:j+1]
        if len(m) > 6 and m[1] == 0x42 and m[4] == func: return m
        i = d.find(b'\xf0', j)
    return None

def current():
    mo.send_message([0xF0,0x42,0x30,0x58,0x10,0xF7])
    m = seg(collect(1.5), 0x40)
    return korg_unpack(m[5:-1])[:254] if m else None

mo.send_message([0xF0,0x42,0x30,0x58,0x1C,0xF7])
bm = seg(collect(8.0), 0x4C)
data = korg_unpack(bm[5:-1])
progs = [data[k*254:(k+1)*254] for k in range(len(data)//254)]
print("bank:", len(progs), "programs")

before = current(); print("edit buffer BEFORE:", nm(before) if before else "?")
target = progs[20]
print("sending patch #21 '%s' to edit buffer (297-byte 0x40 dump)..." % nm(target))
mo.send_message(list(bytes([0xF0,0x42,0x30,0x58,0x40]) + korg_pack(target) + bytes([0xF7])))
time.sleep(0.6)
after = current(); print("edit buffer AFTER: ", nm(after) if after else "?")
print("RESULT:", "PASS - SysEx send works (edit buffer changed)"
      if after and nm(after) == nm(target) else "FAIL - edit buffer did not change")
