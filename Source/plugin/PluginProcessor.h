// MS2K_Plugin — VST3 MIDI-effect version of the editor for DAW automation.
//
// Unlike the standalone (which drives the synth through RtMidi), the plugin
// emits MIDI on its *output bus*: the host records/plays the automation and the
// user routes the track's MIDI output to the MS2000. Every MS2000 parameter —
// including the mod sequencer — is exposed as an automatable AudioParameterInt,
// generated from the same parameterTable() the UI/standalone use.
//
// Threading: audioProg_ is touched only on the audio thread (processBlock). The
// editor owns its own display Program and talks to the synth purely by setting
// parameters; per-parameter atomic "dirty" flags bridge UI/host -> processBlock.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>
#include "../model/Program.h"
#include "../model/ParameterModel.h"
#include "../model/ModSeq.h"
#include "../midi/MidiMessages.h"
#include "../midi/SysexCodec.h"

namespace ms2000 {

class MS2KAudioProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorParameter::Listener {
public:
    // One automatable parameter and how it maps to MS2000 MIDI / program bytes.
    struct Def {
        juce::AudioParameterInt* param = nullptr;
        const ParamSpec* spec = nullptr;          // set for synth params
        enum class Tx { CC, NRPN, Dump } tx = Tx::Dump;
        uint8_t nrpnMsb = 0, nrpnLsb = 0;
        int  displayOffset = 0;
        int  seqKind = 0;                         // 0 = synth; 1..9 = mod-seq selector
        int  seqLane = 0, seqStep = 0;
        void applyRaw(Program&, int raw) const;   // write raw value into a program
        int  readRaw(const Program&) const;       // read raw value from a program
    };

    MS2KAudioProcessor();
    ~MS2KAudioProcessor() override;

    void prepareToPlay(double sampleRate, int) override { sampleRate_ = sampleRate; }
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MS2K Editor"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int sizeInBytes) override;

    // ---- editor interface ----
    const std::vector<Def>& defs() const { return defs_; }
    int  defIndexForId(const juce::String& id) const;
    int  channel() const { return channel_.load(); }
    void setChannel(int ch) { channel_ = ch; }
    void setParamRaw(int defIndex, int raw); // editor edit -> notify host
    int  paramRaw(int defIndex) const;       // current value for display reconcile
    // Seed the outgoing program with a complete patch (name + every byte, not
    // just the parameterised ones), sync all params to it, and optionally send
    // it. Used when loading a patch so the synth receives the real name.
    void loadFullProgram(const Program&, bool sendToSynth);
    void requestCurrentProgram()  { wantRequest_ = true; }
    void requestAllPrograms()     { wantBankRequest_ = true; }
    void forceFullDump()          { wantForceDump_ = true; }

    // Live "knob on the synth -> control in the plugin" sync. When on, the
    // plugin decodes incoming CC/NRPN from the synth and consumes it (so it is
    // not echoed back out). Off = incoming CC passes through untouched.
    void setListening(bool b) { listening_ = b; }
    bool listening() const    { return listening_.load(); }

    // Editor polls these to load what the synth sent back.
    bool takeIncoming(Program& out);                 // single program (Get Current)
    bool takeIncomingBank(std::vector<Program>& out); // 128-program bank (Get All)

private:
    void parameterValueChanged(int index, float newValue) override;
    void parameterGestureChanged(int, bool) override {}
    void buildParams();
    void emitParamMidi(const Def&, int raw, int ch, juce::MidiBuffer&, int sample);
    void handleIncoming(const juce::MidiMessage&);
    void handleSynthCC(uint8_t cc, uint8_t value);   // decode one CC from the synth
    void applyParamFromSynth(int defIndex, int raw); // set param without echoing back

    std::vector<Def> defs_;
    std::unordered_map<std::string, int> idToDef_;
    std::unordered_map<int, int> ccRecv_;   // CC#        -> def index (synth->plugin)
    std::unordered_map<int, int> nrpnRecv_; // msb<<8|lsb -> def index (synth->plugin)
    Program audioProg_;
    std::atomic<int> channel_ { 0 };

    std::unique_ptr<std::atomic<bool>[]> dirty_;
    int  dirtyN_ = 0;
    std::atomic<bool> anyDirty_   { false };
    std::atomic<bool> needsDump_   { false };
    std::atomic<bool> wantRequest_ { false };
    std::atomic<bool> wantBankRequest_ { false };
    std::atomic<bool> wantForceDump_ { false };
    std::atomic<bool> listening_   { false };

    int    samplesSinceDump_ = 1 << 20;
    double sampleRate_ = 44100.0;
    int    nrpnMsb_ = -1, nrpnLsb_ = -1;    // NRPN decode state (audio thread only)

    juce::CriticalSection incomingLock_;
    Program incomingProg_;
    bool    hasIncoming_ = false;
    std::vector<Program> incomingBank_;
    bool    hasIncomingBank_ = false;

    juce::CriticalSection pendingLock_;
    Program pendingFull_;
    bool    hasPendingFull_  = false;
    bool    pendingFullSend_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MS2KAudioProcessor)
};

} // namespace ms2000
