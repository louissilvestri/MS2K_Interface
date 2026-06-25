// ModSeqPanel — the MS2000 Mod Sequencer: 3 lanes x 16 steps of bipolar
// modulation, each lane routed to a destination, plus the common run controls.
// Edits write straight into the Program and fire onEdit(), which the host routes
// through MidiEngine's debounced full-dump (the mod-seq has no per-step CC that
// is destination-independent, so a coalesced dump is the safe path).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>
#include "../model/Program.h"
#include "../model/ModSeq.h"

namespace ms2000 {

// One step: a vertical bar filling from the centre line (up = +, down = -).
class StepBar : public juce::Component {
public:
    StepBar(int index, std::function<int()> get, std::function<void(int)> set);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override; // reset to 0
    void setActive(bool a) { if (active_ != a) { active_ = a; repaint(); } }
private:
    void setFromY(int y);
    int  index_;
    bool active_ = true;
    std::function<int()> get_;
    std::function<void(int)> set_;
};

// One lane: destination + motion selectors and the 16 step bars.
class LaneStrip : public juce::Component {
public:
    LaneStrip(int lane, Program& prog, std::function<void()> onEdit);
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();
    void setLastStep(int last);  // dims steps beyond the run length
private:
    int lane_;
    Program& prog_;
    std::function<void()> onEdit_;
    juce::Label   laneLabel_;
    juce::ComboBox dest_;
    juce::ComboBox motion_;
    std::vector<std::unique_ptr<StepBar>> bars_;
};

class ModSeqPanel : public juce::Component {
public:
    ModSeqPanel(Program& prog, std::function<void()> onEdit);
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();   // reload every control from the Program (after a dump)
private:
    void pushEdit();          // commit + notify host
    void syncLastStepDimming();
    Program& prog_;
    std::function<void()> onEdit_;

    juce::ToggleButton on_;
    juce::ComboBox resolution_, lastStep_, type_, keySync_, runMode_;
    juce::Label    resLbl_, lastLbl_, typeLbl_, syncLbl_, runLbl_;
    std::vector<std::unique_ptr<LaneStrip>> lanes_;
};

} // namespace ms2000
