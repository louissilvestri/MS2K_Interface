#include "MidiEngine.h"

namespace ms2000 {

static void status(const std::function<void(juce::String)>& cb, const juce::String& s) {
    if (cb) cb(s);
}

static void rtCallback(double, std::vector<unsigned char>* msg, void* user) {
    if (msg && user) static_cast<MidiEngine*>(user)->onRtInput(*msg);
}

MidiEngine::MidiEngine() {
    try { out_ = std::make_unique<RtMidiOut>(); } catch (RtMidiError&) {}
    try {
        in_ = std::make_unique<RtMidiIn>();
        in_->ignoreTypes(false, true, true);   // receive SysEx; ignore timing/active-sensing
        in_->setCallback(&rtCallback, this);
    } catch (RtMidiError&) {}

    for (const auto& s : parameterTable())     // reverse map for live CC -> parameter
        if (s.cc >= 0) ccMap_[s.cc] = &s;

    // NRPN-addressable params (arpeggiator + virtual-patch routing).
    const struct { const char* id; uint8_t msb, lsb; } nrpns[] = {
        {"arp_on",0x00,0x02}, {"arp_range",0x00,0x03}, {"arp_latch",0x00,0x04},
        {"arp_type",0x00,0x07}, {"arp_gate",0x00,0x0A},
        {"patch1_src",0x04,0x00},{"patch2_src",0x04,0x01},{"patch3_src",0x04,0x02},{"patch4_src",0x04,0x03},
        {"patch1_dst",0x04,0x08},{"patch2_dst",0x04,0x09},{"patch3_dst",0x04,0x0A},{"patch4_dst",0x04,0x0B},
    };
    for (const auto& e : nrpns)
        if (auto* sp = findParam(e.id)) {
            nrpnSend_[sp] = { e.msb, e.lsb };
            nrpnRecv_[(e.msb << 8) | e.lsb] = sp;
        }
}

MidiEngine::~MidiEngine() {
    stopTimer();
    if (in_)  { in_->cancelCallback(); in_->closePort(); }
    if (out_) out_->closePort();
}

bool MidiEngine::hasOutput() const { return out_ && out_->isPortOpen(); }
bool MidiEngine::hasInput()  const { return in_  && in_->isPortOpen(); }

juce::StringArray MidiEngine::outputNames() const {
    juce::StringArray names;
    if (out_) try { for (unsigned i = 0; i < out_->getPortCount(); ++i) names.add(out_->getPortName(i)); }
              catch (RtMidiError&) {}
    return names;
}
juce::StringArray MidiEngine::inputNames() const {
    juce::StringArray names;
    if (in_) try { for (unsigned i = 0; i < in_->getPortCount(); ++i) names.add(in_->getPortName(i)); }
             catch (RtMidiError&) {}
    return names;
}

bool MidiEngine::openOutput(int index) {
    if (!out_) return false;
    try {
        if (out_->isPortOpen()) out_->closePort();
        if (index < 0) return false;
        out_->openPort((unsigned)index);
        return out_->isPortOpen();
    } catch (RtMidiError&) { return false; }
}

bool MidiEngine::openInput(int index) {
    if (!in_) return false;
    try {
        if (in_->isPortOpen()) in_->closePort();
        sysexAccum_.clear();
        if (index < 0) return false;
        in_->openPort((unsigned)index);
        return in_->isPortOpen();
    } catch (RtMidiError&) { return false; }
}

void MidiEngine::sendRaw(const Bytes& bytes) {
    if (bytes.empty()) return;
    if (!hasOutput()) { status(onStatus, "No MIDI output selected"); return; }
    try {
        if (bytes[0] == 0xF0) {                         // SysEx -> one long message
            std::vector<unsigned char> v(bytes.begin(), bytes.end());
            out_->sendMessage(&v);
            return;
        }
        // Channel data may be several messages back-to-back (e.g. an NRPN triple).
        // RtMidi/winmm needs each sent separately, so split at status bytes.
        size_t i = 0;
        while (i < bytes.size()) {
            size_t j = i + 1;
            while (j < bytes.size() && bytes[j] < 0x80) ++j;
            std::vector<unsigned char> v(bytes.begin() + i, bytes.begin() + j);
            out_->sendMessage(&v);
            i = j;
        }
    } catch (RtMidiError& e) { status(onStatus, "MIDI send failed: " + juce::String(e.what())); }
}

void MidiEngine::sendParam(const ParamSpec& spec, int rawValue, const Program& prog) {
    if (spec.cc >= 0) {                                   // CC path
        sendRaw(realtimeForParam(spec, rawValue, channel_));
        return;
    }
    auto it = nrpnSend_.find(&spec);
    if (it != nrpnSend_.end()) {                          // NRPN path (arp / patch routing)
        sendRaw(buildNRPN(channel_, it->second.first, it->second.second,
                          (uint8_t)ccValueForRaw(spec, rawValue)));
        return;
    }
    scheduleDump(prog);   // no CC/NRPN -> one (debounced) full dump, not one per pixel
}

void MidiEngine::scheduleDump(const Program& prog) {
    pendingDump_ = prog;
    hasPendingDump_ = true;
    startTimer(160);      // restarts while the user keeps dragging; fires once on settle
}

void MidiEngine::timerCallback() {
    stopTimer();
    if (hasPendingDump_) { hasPendingDump_ = false; sendCurrentProgram(pendingDump_); }
}

void MidiEngine::sendCurrentProgram(const Program& prog) {
    Bytes data(prog.bytes().begin(), prog.bytes().end());
    sendRaw(makeProgramDump(data, channel_, Func::CurrentProgramDump));
}

void MidiEngine::requestCurrentProgram() {
    sendRaw(makeRequest(Func::CurrentProgramDumpRequest, channel_));
    if (hasOutput()) status(onStatus, hasInput()
        ? "Requested current program (ch " + juce::String(channel_ + 1) + ")..."
        : "Sent, but NO MIDI INPUT selected - pick UM-ONE in 'MIDI In' to receive the reply.");
}

void MidiEngine::requestAllPrograms() {
    sendRaw(makeRequest(Func::ProgramDumpRequest, channel_));
    if (hasOutput()) status(onStatus, hasInput()
        ? "Requested all 128 programs (ch " + juce::String(channel_ + 1) + ") - waiting for ~37 KB dump..."
        : "Sent, but NO MIDI INPUT selected - pick UM-ONE in 'MIDI In' to receive the reply.");
}

static Program toProgram(const Bytes& b) {
    std::array<uint8_t, kProgramSize> arr{};
    std::copy_n(b.begin(), kProgramSize, arr.begin());
    return Program(arr);
}

// Reassemble SysEx across RtMidi callbacks: a large dump arrives as several
// fragments — the first starts 0xF0, continuations don't, the last ends 0xF7.
void MidiEngine::onRtInput(const std::vector<unsigned char>& bytes) {
    if (bytes.empty()) return;
    const uint8_t status = bytes[0];

    if (!sysexAccum_.empty() || status == 0xF0) {       // SysEx (re)assembly
        if (!sysexAccum_.empty()) sysexAccum_.insert(sysexAccum_.end(), bytes.begin(), bytes.end());
        else                      sysexAccum_.assign(bytes.begin(), bytes.end());
        if (sysexAccum_.back() == 0xF7) { dispatchSysex(sysexAccum_); sysexAccum_.clear(); }
        return;
    }

    // Control Change from the synth (a moved knob/selector) -> live UI update.
    if ((status & 0xF0) == 0xB0 && bytes.size() >= 3)
        handleCC(bytes[1], bytes[2]);
}

void MidiEngine::handleCC(uint8_t cc, uint8_t value) {
    if (cc == 99) { nrpnMsb_ = value; return; }
    if (cc == 98) { nrpnLsb_ = value; return; }
    if (cc == 6) {                                        // NRPN data entry MSB
        if (nrpnMsb_ >= 0 && nrpnLsb_ >= 0) {
            auto it = nrpnRecv_.find((nrpnMsb_ << 8) | nrpnLsb_);
            if (it != nrpnRecv_.end() && onParamFromSynth)
                onParamFromSynth(it->second, rawFromCcValue(*it->second, value));
        }
        return;
    }
    if (cc == 38) return;                                 // data entry LSB
    auto it = ccMap_.find(cc);
    if (it == ccMap_.end()) return;
    if (onParamFromSynth) onParamFromSynth(it->second, rawFromCcValue(*it->second, value));
}

void MidiEngine::dispatchSysex(const Bytes& full) {
    if (auto prog = parseProgramDump(full)) {
        if (onProgramReceived) onProgramReceived(toProgram(*prog));
        status(onStatus, "Received current program (" + juce::String(toProgram(*prog).name()) + ")");
    } else if (auto bank = parseBankDump(full)) {
        std::vector<Program> progs;
        progs.reserve(bank->size());
        for (const auto& b : *bank) progs.push_back(toProgram(b));
        if (onBankReceived) onBankReceived(progs);
        status(onStatus, "Received bank: " + juce::String((int)progs.size()) + " programs");
    } else if (isMs2000Sysex(full) && full.size() == 6) {
        status(onStatus, "Synth ack/err: func 0x" + juce::String::toHexString(&full[4], 1, 0).toUpperCase());
    } else if (full.size() > 5 && full[0] == 0xF0) {
        status(onStatus, "Received SysEx: " + juce::String((int)full.size()) +
               " bytes, func 0x" + juce::String::toHexString(&full[4], 1, 0).toUpperCase() +
               " (unrecognized)");
    }
}

} // namespace ms2000
