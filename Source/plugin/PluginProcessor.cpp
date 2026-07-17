#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ms2000 {

// ---- Def: program byte <-> raw value ---------------------------------------
void MS2KAudioProcessor::Def::applyRaw(Program& p, int raw) const {
    if (spec) { setDisplay(p, 0, *spec, raw - displayOffset); return; }
    switch (seqKind) {
        case 1: modseq::setOn(p, raw != 0);            break;
        case 2: modseq::setRunMode(p, raw);            break;
        case 3: modseq::setResolution(p, raw);         break;
        case 4: modseq::setLastStep(p, raw);           break;
        case 5: modseq::setSeqType(p, raw);            break;
        case 6: modseq::setKeySync(p, raw);            break;
        case 7: modseq::setDest(p, seqLane, raw);      break;
        case 8: modseq::setMotion(p, seqLane, raw);    break;
        case 9: modseq::setStep(p, seqLane, seqStep, raw); break;
        default: break;
    }
}

int MS2KAudioProcessor::Def::readRaw(const Program& p) const {
    if (spec) return getRaw(p, 0, *spec);
    switch (seqKind) {
        case 1: return modseq::on(p) ? 1 : 0;
        case 2: return modseq::runMode(p);
        case 3: return modseq::resolution(p);
        case 4: return modseq::lastStep(p);
        case 5: return modseq::seqType(p);
        case 6: return modseq::keySync(p);
        case 7: return modseq::dest(p, seqLane);
        case 8: return modseq::motion(p, seqLane);
        case 9: return modseq::step(p, seqLane, seqStep);
        default: return 0;
    }
}

// ---- NRPN map (mirrors MidiEngine) -----------------------------------------
namespace {
struct NrpnEntry { const char* id; uint8_t msb, lsb; };
const NrpnEntry kNrpn[] = {
    {"arp_on",0x00,0x02}, {"arp_range",0x00,0x03}, {"arp_latch",0x00,0x04},
    {"arp_type",0x00,0x07}, {"arp_gate",0x00,0x0A},
    {"patch1_src",0x04,0x00},{"patch2_src",0x04,0x01},{"patch3_src",0x04,0x02},{"patch4_src",0x04,0x03},
    {"patch1_dst",0x04,0x08},{"patch2_dst",0x04,0x09},{"patch3_dst",0x04,0x0A},{"patch4_dst",0x04,0x0B},
};
const NrpnEntry* findNrpn(const std::string& id) {
    for (auto& e : kNrpn) if (id == e.id) return &e;
    return nullptr;
}
void addRaw(juce::MidiBuffer& buf, const Bytes& b, int sample) {
    if (b.empty()) return;
    if (b[0] == 0xF0) { buf.addEvent(juce::MidiMessage(b.data(), (int)b.size()), sample); return; }
    size_t i = 0;
    while (i < b.size()) {
        size_t j = i + 1; while (j < b.size() && b[j] < 0x80) ++j;
        buf.addEvent(juce::MidiMessage(b.data() + (int)i, (int)(j - i)), sample);
        i = j;
    }
}
} // namespace

// ---- diagnostic log --------------------------------------------------------
static juce::File logFile() {
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("MS2K_Interface");
    dir.createDirectory();
    return dir.getChildFile("ms2k_vst.log");
}
juce::String MS2KAudioProcessor::logFilePath() const { return logFile().getFullPathName(); }
void MS2KAudioProcessor::logMsg(const juce::String& s) {
    if (log_) log_->logMessage(juce::Time::getCurrentTime().toString(false, true, true, true) + "  " + s);
}
static juce::String hexHead(const uint8_t* d, int n, int maxB = 12) {
    juce::String s;
    for (int i = 0; i < juce::jmin(n, maxB); ++i)
        s += juce::String::toHexString((int)d[i]).paddedLeft('0', 2) + " ";
    return s.trim();
}

// ---- construction ----------------------------------------------------------
MS2KAudioProcessor::MS2KAudioProcessor()
    : juce::AudioProcessor(BusesProperties()) {   // MIDI effect: no audio buses
    log_.reset(new juce::FileLogger(logFile(), "==== MS2K VST loaded ===="));
    buildParams();
}

MS2KAudioProcessor::~MS2KAudioProcessor() {
    logMsg("plugin unloaded");
    for (auto& d : defs_) if (d.param) d.param->removeListener(this);
}

void MS2KAudioProcessor::buildParams() {
    auto add = [this](Def d, const juce::String& id, const juce::String& name,
                      int lo, int hi, int def) {
        def = juce::jlimit(lo, hi, def);
        auto* p = new juce::AudioParameterInt(juce::ParameterID(id, 1), name, lo, hi, def);
        d.param = p;
        addParameter(p);
        p->addListener(this);
        idToDef_[id.toStdString()] = (int)defs_.size();
        defs_.push_back(d);
    };

    // Synth parameters, straight from the shared table.
    for (const auto& s : parameterTable()) {
        Def d; d.spec = &s; d.displayOffset = s.displayOffset;
        const int idx = (int)defs_.size();
        if (s.cc >= 0)                 { d.tx = Def::Tx::CC;   ccRecv_[s.cc] = idx; }
        else if (auto* n = findNrpn(s.id)) { d.tx = Def::Tx::NRPN; d.nrpnMsb = n->msb; d.nrpnLsb = n->lsb;
                                            nrpnRecv_[(n->msb << 8) | n->lsb] = idx; }
        else                           d.tx = Def::Tx::Dump;
        const int lo = s.rawMin(), hi = s.rawMax();
        add(d, juce::String(s.id), juce::String(s.label), lo, hi, getRaw(audioProg_, 0, s));
    }

    // Mod-sequencer parameters (all Dump-class — sent via coalesced full dump).
    auto seq = [](int kind, int lane = 0, int step = 0) {
        Def d; d.tx = Def::Tx::Dump; d.seqKind = kind; d.seqLane = lane; d.seqStep = step; return d;
    };
    add(seq(1), "seq_on",   "Seq On/Off",   0, 1,  modseq::on(audioProg_) ? 1 : 0);
    add(seq(2), "seq_run",  "Seq Run Mode", 0, 1,  modseq::runMode(audioProg_));
    add(seq(3), "seq_res",  "Seq Resolution", 0, 15, modseq::resolution(audioProg_));
    add(seq(4), "seq_last", "Seq Last Step", 0, 15, modseq::lastStep(audioProg_));
    add(seq(5), "seq_type", "Seq Type",     0, 3,  modseq::seqType(audioProg_));
    add(seq(6), "seq_sync", "Seq Key Sync", 0, 2,  modseq::keySync(audioProg_));
    for (int l = 0; l < modseq::kLanes; ++l) {
        const juce::String pre = "seq" + juce::String(l + 1) + "_";
        const juce::String nm  = "Seq" + juce::String(l + 1) + " ";
        add(seq(7, l), pre + "dest",   nm + "Dest",   0, 30, modseq::dest(audioProg_, l));
        add(seq(8, l), pre + "motion", nm + "Motion", 0, 1,  modseq::motion(audioProg_, l));
        for (int s = 0; s < modseq::kSteps; ++s)
            add(seq(9, l, s), pre + "s" + juce::String(s + 1).paddedLeft('0', 2),
                nm + "Step " + juce::String(s + 1), -63, 63, modseq::step(audioProg_, l, s));
    }

    dirtyN_ = (int)defs_.size();
    dirty_  = std::make_unique<std::atomic<bool>[]>((size_t)dirtyN_);
    for (int i = 0; i < dirtyN_; ++i) dirty_[i].store(false);
}

int MS2KAudioProcessor::defIndexForId(const juce::String& id) const {
    auto it = idToDef_.find(id.toStdString());
    return it == idToDef_.end() ? -1 : it->second;
}

// ---- parameter changes (UI on message thread, host on audio thread) --------
void MS2KAudioProcessor::parameterValueChanged(int index, float) {
    if (index >= 0 && index < dirtyN_) { dirty_[index].store(true); anyDirty_.store(true); }
}

void MS2KAudioProcessor::setParamRaw(int i, int raw) {
    if (i >= 0 && i < (int)defs_.size()) *defs_[(size_t)i].param = raw; // notifies host -> dirty
}
int MS2KAudioProcessor::paramRaw(int i) const {
    return (i >= 0 && i < (int)defs_.size()) ? defs_[(size_t)i].param->get() : 0;
}

// ---- MIDI emission ---------------------------------------------------------
void MS2KAudioProcessor::emitParamMidi(const Def& d, int raw, int ch, juce::MidiBuffer& out, int sample) {
    if (d.spec && d.tx == Def::Tx::CC)
        addRaw(out, realtimeForParam(*d.spec, raw, (uint8_t)ch), sample);
    else if (d.spec && d.tx == Def::Tx::NRPN)
        addRaw(out, buildNRPN((uint8_t)ch, d.nrpnMsb, d.nrpnLsb, (uint8_t)ccValueForRaw(*d.spec, raw)), sample);
    else
        needsDump_.store(true);   // no realtime path -> coalesced full dump
}

void MS2KAudioProcessor::loadFullProgram(const Program& p, bool sendToSynth) {
    const juce::ScopedLock sl(pendingLock_);
    pendingFull_ = p; pendingFullSend_ = sendToSynth; hasPendingFull_ = true;
}

void MS2KAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    buffer.clear();
    const int ch = channel_.load();
    juce::MidiBuffer out;

    // Install a freshly loaded patch (full bytes incl. name): make it the dump
    // base and sync every parameter to it without emitting per-param MIDI.
    {
        Program full; bool got = false, send = false;
        { const juce::ScopedLock sl(pendingLock_);
          if (hasPendingFull_) { full = pendingFull_; send = pendingFullSend_; hasPendingFull_ = false; got = true; } }
        if (got) {
            audioProg_ = full;
            for (int i = 0; i < dirtyN_; ++i) {
                *defs_[(size_t)i].param = defs_[(size_t)i].readRaw(audioProg_);
                dirty_[i].store(false);     // synced silently; don't echo each param
            }
            if (send) wantForceDump_ = true; // one complete dump (name included)
        }
    }

    // Pass through anything that isn't ours; consume SysEx (synth dump replies)
    // and, when listening, the synth's CC/NRPN (so knob moves sync the UI and
    // don't echo straight back out).
    //
    // A bank dump (~37 KB, ~12 s over DIN) reaches the plugin as SysEx fragments
    // spread across many process blocks, so reassemble until the closing F7
    // before parsing — exactly as the standalone's MidiEngine does.
    const bool listening = listening_.load();
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        const uint8_t* raw = m.getRawData();
        const int n = m.getRawDataSize();
        const bool startsSysex = (n > 0 && raw[0] == 0xF0);

        if (!sysexAccum_.empty() || startsSysex) {
            if (startsSysex && !sysexAccum_.empty()) {
                logMsg("SYSEX: new F0 while " + juce::String((int)sysexAccum_.size()) + " B pending - resetting");
                sysexAccum_.clear();
            }
            if (sysexAccum_.empty()) {                        // first fragment of a dump
                sysexFrags_ = 0; sysexLogged_ = 0;
                logMsg("SYSEX start: n=" + juce::String(n) + " head=[" + hexHead(raw, n) + "]");
            }
            sysexAccum_.insert(sysexAccum_.end(), raw, raw + n);
            ++sysexFrags_;
            if (sysexAccum_.size() - sysexLogged_ >= 4096) {  // progress, not every fragment
                logMsg("SYSEX accum=" + juce::String((int)sysexAccum_.size()) + " B, frags=" + juce::String(sysexFrags_));
                sysexLogged_ = sysexAccum_.size();
            }
            if (sysexAccum_.back() == 0xF7) {
                logMsg("SYSEX complete: total=" + juce::String((int)sysexAccum_.size()) +
                       " B, frags=" + juce::String(sysexFrags_) + ", tail=[" +
                       hexHead(sysexAccum_.data() + juce::jmax(0, (int)sysexAccum_.size() - 6), juce::jmin(6, (int)sysexAccum_.size())) + "]");
                handleIncomingBytes(sysexAccum_);
                sysexAccum_.clear();
            } else if (sysexAccum_.size() > 300000) {
                logMsg("SYSEX runaway reset at " + juce::String((int)sysexAccum_.size()) + " B");
                sysexAccum_.clear();
            }
            continue;
        }
        if (listening && m.isController() && m.getChannel() == ch + 1) {
            handleSynthCC((uint8_t)m.getControllerNumber(), (uint8_t)m.getControllerValue());
            continue;
        }
        out.addEvent(m, meta.samplePosition);
    }

    if (wantRequest_.exchange(false)) {
        addRaw(out, makeRequest(Func::CurrentProgramDumpRequest, (uint8_t)ch), 0);
        logMsg("-> sent Current Program request (0x10) on ch " + juce::String(ch + 1));
    }
    if (wantBankRequest_.exchange(false)) {
        addRaw(out, makeRequest(Func::ProgramDumpRequest, (uint8_t)ch), 0);
        logMsg("-> sent All Programs request (0x1C) on ch " + juce::String(ch + 1));
    }

    if (anyDirty_.exchange(false)) {
        for (int i = 0; i < dirtyN_; ++i) {
            if (!dirty_[i].exchange(false)) continue;
            const auto& d = defs_[(size_t)i];
            const int raw = d.param->get();
            d.applyRaw(audioProg_, raw);
            emitParamMidi(d, raw, ch, out, 0);
        }
    }

    samplesSinceDump_ += juce::jmax(1, buffer.getNumSamples());
    const bool force = wantForceDump_.exchange(false);
    if (needsDump_.load() || force) {
        const int minGap = (int)(sampleRate_ * 0.06); // <= ~16 dumps/sec
        if (force || samplesSinceDump_ >= minGap) {
            Bytes d(audioProg_.bytes().begin(), audioProg_.bytes().end());
            addRaw(out, makeProgramDump(d, (uint8_t)ch, Func::CurrentProgramDump), 0);
            samplesSinceDump_ = 0;
            needsDump_.store(false);
        }
    }

    midi.swapWith(out);
}

// TODO: Add a direct hardware MIDI input (RtMidi) for in-plugin bank receive
//  Reaper doesn't pass real-time input SysEx to a track's FX chain, so the plugin's
//  Get All / Get Current can't receive dumps over the host bus (see the README
//  limitation and the wiki Troubleshooting page). Give the plugin an optional direct
//  RtMidi input like the standalone to receive dumps + Listen, bypassing the DAW,
//  and strip the diagnostic file logging once it lands.
//  labels: enhancement
void MS2KAudioProcessor::handleIncomingBytes(const Bytes& full) {
    auto toProgram = [](const Bytes& b) {
        std::array<uint8_t, kProgramSize> a{}; std::copy_n(b.begin(), kProgramSize, a.begin()); return Program(a);
    };
    if (auto prog = parseProgramDump(full)) {
        logMsg("PARSE ok: single program (Get Current)");
        const juce::ScopedLock sl(incomingLock_);
        incomingProg_ = toProgram(*prog); hasIncoming_ = true;
    } else if (auto bank = parseBankDump(full)) {
        logMsg("PARSE ok: BANK of " + juce::String((int)bank->size()) + " programs");
        std::vector<Program> progs; progs.reserve(bank->size());
        for (const auto& b : *bank) progs.push_back(toProgram(b));
        const juce::ScopedLock sl(incomingLock_);
        incomingBank_ = std::move(progs); hasIncomingBank_ = true;
    } else {
        logMsg("PARSE FAILED: len=" + juce::String((int)full.size()) +
               " head=[" + hexHead(full.data(), (int)full.size()) + "]");
    }
}

bool MS2KAudioProcessor::takeIncoming(Program& out) {
    const juce::ScopedLock sl(incomingLock_);
    if (!hasIncoming_) return false;
    out = incomingProg_; hasIncoming_ = false; return true;
}

bool MS2KAudioProcessor::takeIncomingBank(std::vector<Program>& out) {
    const juce::ScopedLock sl(incomingLock_);
    if (!hasIncomingBank_) return false;
    out = std::move(incomingBank_); hasIncomingBank_ = false; return true;
}

// Decode one CC from the synth into a parameter (mirrors MidiEngine::handleCC).
void MS2KAudioProcessor::handleSynthCC(uint8_t cc, uint8_t value) {
    if (cc == 99) { nrpnMsb_ = value; return; }
    if (cc == 98) { nrpnLsb_ = value; return; }
    if (cc == 6) {                                   // NRPN data entry MSB
        if (nrpnMsb_ >= 0 && nrpnLsb_ >= 0) {
            auto it = nrpnRecv_.find((nrpnMsb_ << 8) | nrpnLsb_);
            if (it != nrpnRecv_.end()) {
                const auto& d = defs_[(size_t)it->second];
                if (d.spec) applyParamFromSynth(it->second, rawFromCcValue(*d.spec, value));
            }
        }
        return;
    }
    if (cc == 38) return;                            // data entry LSB (ignored)
    auto it = ccRecv_.find(cc);
    if (it == ccRecv_.end()) return;
    const auto& d = defs_[(size_t)it->second];
    if (d.spec) applyParamFromSynth(it->second, rawFromCcValue(*d.spec, value));
}

// Update a parameter from the synth without scheduling an outgoing echo.
void MS2KAudioProcessor::applyParamFromSynth(int i, int raw) {
    auto& d = defs_[(size_t)i];
    d.applyRaw(audioProg_, raw);
    *d.param = raw;              // notifies host + editor (UI follows the synth)
    dirty_[i].store(false);      // ...but don't re-emit it back to the synth
}

// ---- state ------------------------------------------------------------------
void MS2KAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    juce::MemoryOutputStream os(dest, false);
    os.writeInt(0x4D533201);                 // 'MS2\1' magic/version
    os.writeInt(channel_.load());
    os.writeInt((int)defs_.size());
    for (auto& d : defs_) os.writeInt(d.param->get());
}

void MS2KAudioProcessor::setStateInformation(const void* data, int size) {
    juce::MemoryInputStream is(data, (size_t)size, false);
    if (is.readInt() != 0x4D533201) return;
    setChannel(is.readInt());
    const int n = is.readInt();
    for (int i = 0; i < n && i < (int)defs_.size(); ++i)
        *defs_[(size_t)i].param = is.readInt();
}

juce::AudioProcessorEditor* MS2KAudioProcessor::createEditor() {
    return new MS2KPluginEditor(*this);
}

} // namespace ms2000

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ms2000::MS2KAudioProcessor();
}
