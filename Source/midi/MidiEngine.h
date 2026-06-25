// MidiEngine — MIDI port I/O via RtMidi (JUCE's Windows MIDI input did not
// deliver callbacks on this setup; RtMidi — the lib the working probe uses —
// does). Reassembles fragmented SysEx before dispatching.
#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include "rtmidi/RtMidi.h"
#include "MidiMessages.h"
#include "SysexCodec.h"
#include "../model/Program.h"
#include "../model/ParameterModel.h"

namespace ms2000 {

class MidiEngine : private juce::Timer {
public:
    MidiEngine();
    ~MidiEngine() override;

    juce::StringArray outputNames() const;
    juce::StringArray inputNames() const;
    bool openOutput(int index);
    bool openInput(int index);
    void setChannel(int ch /*0..15*/) { channel_ = (uint8_t)juce::jlimit(0, 15, ch); }
    int  channel() const { return channel_; }
    bool hasOutput() const;
    bool hasInput() const;

    void sendParam(const ParamSpec& spec, int rawValue, const Program& prog);
    void sendCurrentProgram(const Program& prog);
    void sendProgramDebounced(const Program& prog) { scheduleDump(prog); } // coalesced full dump (e.g. mod-seq edits)
    void requestCurrentProgram();   // 0x10 -> synth replies 0x40 (edit buffer)
    void requestAllPrograms();      // 0x1C -> synth replies 0x4C (128 programs)

    // callbacks fire on the RtMidi thread; the UI marshals them to the message thread
    std::function<void(Program)>              onProgramReceived;
    std::function<void(std::vector<Program>)> onBankReceived;
    std::function<void(juce::String)>         onStatus;
    // A live CC from the synth decoded to a parameter + its raw value.
    std::function<void(const ParamSpec*, int)> onParamFromSynth;

    // RtMidi input callback target (raw bytes, possibly a SysEx fragment).
    void onRtInput(const std::vector<unsigned char>& bytes);

private:
    void sendRaw(const Bytes& bytes);
    void dispatchSysex(const Bytes& full);
    void handleCC(uint8_t cc, uint8_t value);
    void scheduleDump(const Program& prog); // coalesce rapid no-CC edits into one dump
    void timerCallback() override;

    std::unique_ptr<RtMidiOut> out_;
    std::unique_ptr<RtMidiIn>  in_;
    std::vector<uint8_t>       sysexAccum_; // reassembly buffer (RtMidi thread only)
    std::unordered_map<int, const ParamSpec*> ccMap_;   // CC# -> parameter
    std::unordered_map<const ParamSpec*, std::pair<uint8_t, uint8_t>> nrpnSend_; // param -> (msb,lsb)
    std::unordered_map<int, const ParamSpec*> nrpnRecv_; // (msb<<8|lsb) -> parameter
    int nrpnMsb_ = -1, nrpnLsb_ = -1;
    Program pendingDump_;        // latest program awaiting a debounced dump
    bool    hasPendingDump_ = false;
    uint8_t channel_ = 0;
};

} // namespace ms2000
