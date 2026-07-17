// MS2KPluginEditor — hosts the same de-multiplexed UI as the standalone app
// (LayoutCanvas + ModSeqPanel) but talks to the synth purely by setting the
// processor's automatable parameters. A timer reconciles the displayed Program
// from parameter values, so host automation moving a parameter updates the UI.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "PluginProcessor.h"
#include "../model/Program.h"
#include "../ui/Components.h"
#include "../ui/ModSeqPanel.h"
#include "../ui/MS2000LookAndFeel.h"

namespace ms2000 {

class MS2KPluginEditor : public juce::AudioProcessorEditor,
                         public juce::ListBoxModel,
                         private juce::Timer {
public:
    explicit MS2KPluginEditor(MS2KAudioProcessor&);
    ~MS2KPluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // ListBoxModel (bank librarian)
    int  getNumRows() override { return (int)bank_.size(); }
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void onSynthEdit(const ParamSpec&, int raw); // a control changed -> set its param
    void onSeqEdit();                            // mod-seq changed -> push seq params
    void reconcileFromParams();                  // pull param values into editProg_
    void loadProgramIntoEditor(const Program&);  // display + push params (no auto-send)
    void loadSyxFile();
    void saveSyxFile();
    void loadSyxData(const std::vector<uint8_t>&);

    MS2KAudioProcessor& proc_;
    Program editProg_;                           // message-thread display copy

    std::unique_ptr<LayoutCanvas> canvas_;
    std::unique_ptr<ModSeqPanel>  modSeq_;
    juce::Viewport viewport_;

    juce::ComboBox   channelBox_, midiInBox_;
    juce::TextButton getBtn_, sendBtn_, seqBtn_, bankBtn_;
    juce::ToggleButton listenBtn_;
    juce::Label      title_, hint_;

    // librarian sidebar
    std::vector<Program> bank_;
    juce::ListBox    bankList_;
    juce::TextButton getAllBtn_, loadBtn_, saveBtn_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    bool modSeqVisible_ = true;
    bool bankVisible_   = false;
    bool suppressEdits_ = false;                 // guard while reconciling

    MS2000LookAndFeel laf_;

    static constexpr int kTopH = 38, kCheek = 12, kStatusH = 22, kSidebar = 220;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MS2KPluginEditor)
};

} // namespace ms2000
