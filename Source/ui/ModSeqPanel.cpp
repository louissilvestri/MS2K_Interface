#include "ModSeqPanel.h"
#include "MS2000LookAndFeel.h"

namespace ms2000 {

namespace {
constexpr int kPad      = 10;
constexpr int kCommonH  = 46;
constexpr int kLaneLblW = 124;  // left column: label + dest + motion
constexpr int kValueH   = 13;   // numeric readout above each bar
constexpr int kNumH     = 13;   // step number below each bar
}

// ============================ StepBar ======================================
StepBar::StepBar(int index, std::function<int()> get, std::function<void(int)> set)
    : index_(index), get_(std::move(get)), set_(std::move(set)) {}

static juce::Rectangle<int> stepTrack(juce::Rectangle<int> local) {
    local.removeFromTop(kValueH);
    local.removeFromBottom(kNumH);
    return local.reduced(3, 2);
}

void StepBar::paint(juce::Graphics& g) {
    auto full = getLocalBounds();
    auto valueArea = full.removeFromTop(kValueH);
    auto numArea   = full.removeFromBottom(kNumH);
    auto track = stepTrack(getLocalBounds());

    const int v = get_();
    g.setColour(juce::Colour(active_ ? 0xff10343a : 0xff10262a));
    g.fillRoundedRectangle(track.toFloat(), 2.0f);
    g.setColour(juce::Colour(Palette::sectionLine));
    g.drawRoundedRectangle(track.toFloat(), 2.0f, 1.0f);

    const float cy = track.getCentreY();
    g.setColour(juce::Colour(0x66ffffff));
    g.drawHorizontalLine((int)cy, (float)track.getX(), (float)track.getRight());

    const float half = track.getHeight() * 0.5f - 1.0f;
    const float frac = juce::jlimit(-1.0f, 1.0f, v / 63.0f);
    const float barH = std::abs(frac) * half;
    if (barH > 0.5f) {
        juce::Rectangle<float> bar(track.getX() + 2.0f, 0, track.getWidth() - 4.0f, barH);
        bar.setY(frac >= 0 ? cy - barH : cy);
        const juce::uint32 c = active_ ? (v >= 0 ? Palette::accent : 0xff37b6c4) : Palette::accentDim;
        g.setColour(juce::Colour(c));
        g.fillRoundedRectangle(bar, 1.5f);
    }

    g.setColour(juce::Colour(active_ ? (juce::uint32)Palette::textValue : 0xff5a7e82));
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawText(juce::String(v), valueArea, juce::Justification::centred);
    g.setColour(juce::Colour(active_ ? 0xffbfe3e6 : 0xff42686c));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText(juce::String(index_ + 1), numArea, juce::Justification::centred);
}

void StepBar::setFromY(int y) {
    auto track = stepTrack(getLocalBounds());
    const float half = track.getHeight() * 0.5f - 1.0f;
    const float cy = track.getCentreY();
    int v = (int)std::lround((cy - y) / half * 63.0f);
    set_(juce::jlimit(-63, 63, v));
    repaint();
}

void StepBar::mouseDown(const juce::MouseEvent& e) { setFromY(e.y); }
void StepBar::mouseDrag(const juce::MouseEvent& e) { setFromY(e.y); }
void StepBar::mouseDoubleClick(const juce::MouseEvent&) { set_(0); repaint(); }

// ============================ LaneStrip ====================================
LaneStrip::LaneStrip(int lane, Program& prog, std::function<void()> onEdit)
    : lane_(lane), prog_(prog), onEdit_(std::move(onEdit)) {
    laneLabel_.setText("SEQ " + juce::String(lane + 1), juce::dontSendNotification);
    laneLabel_.setColour(juce::Label::textColourId, juce::Colour(Palette::textLight));
    laneLabel_.setFont(juce::Font(13.0f, juce::Font::bold));
    addAndMakeVisible(laneLabel_);

    int id = 1;
    for (auto* s : modseq::destLabels()) dest_.addItem(s, id++);
    dest_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff10343a));
    dest_.onChange = [this] {
        modseq::setDest(prog_, lane_, dest_.getSelectedId() - 1);
        onEdit_();
    };
    addAndMakeVisible(dest_);

    motion_.addItem("Smooth", 1);
    motion_.addItem("Step", 2);
    motion_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff10343a));
    motion_.onChange = [this] {
        modseq::setMotion(prog_, lane_, motion_.getSelectedId() - 1);
        onEdit_();
    };
    addAndMakeVisible(motion_);

    for (int s = 0; s < modseq::kSteps; ++s) {
        auto bar = std::make_unique<StepBar>(s,
            [this, s] { return modseq::step(prog_, lane_, s); },
            [this, s](int v) { modseq::setStep(prog_, lane_, s, v); onEdit_(); });
        addAndMakeVisible(*bar);
        bars_.push_back(std::move(bar));
    }
    refresh();
}

void LaneStrip::resized() {
    auto r = getLocalBounds().reduced(kPad / 2);
    auto left = r.removeFromLeft(kLaneLblW);
    laneLabel_.setBounds(left.removeFromTop(22));
    left.removeFromTop(2);
    dest_.setBounds(left.removeFromTop(24));
    left.removeFromTop(4);
    motion_.setBounds(left.removeFromTop(24));

    r.removeFromLeft(6);
    const int n = (int)bars_.size();
    const float w = r.getWidth() / (float)n;
    for (int i = 0; i < n; ++i)
        bars_[(size_t)i]->setBounds(r.getX() + (int)std::round(i * w), r.getY(),
                                    (int)std::round(w) - 2, r.getHeight());
}

void LaneStrip::paint(juce::Graphics& g) {
    g.setColour(juce::Colour(Palette::sectionFill).withAlpha(0.35f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 4.0f);
}

void LaneStrip::refresh() {
    dest_.setSelectedId(modseq::dest(prog_, lane_) + 1, juce::dontSendNotification);
    motion_.setSelectedId(modseq::motion(prog_, lane_) + 1, juce::dontSendNotification);
    for (auto& b : bars_) b->repaint();
}

void LaneStrip::setLastStep(int last) {
    for (int i = 0; i < (int)bars_.size(); ++i) bars_[(size_t)i]->setActive(i <= last);
}

// ============================ ModSeqPanel ==================================
ModSeqPanel::ModSeqPanel(Program& prog, std::function<void()> onEdit)
    : prog_(prog), onEdit_(std::move(onEdit)) {
    on_.setButtonText("MOD SEQ");
    on_.setColour(juce::ToggleButton::textColourId, juce::Colour(Palette::textLight));
    on_.onClick = [this] { modseq::setOn(prog_, on_.getToggleState()); pushEdit(); };
    addAndMakeVisible(on_);

    auto setupCombo = [this](juce::ComboBox& box, juce::Label& lbl, const juce::String& name) {
        lbl.setText(name, juce::dontSendNotification);
        lbl.setColour(juce::Label::textColourId, juce::Colour(Palette::textValue));
        lbl.setFont(juce::Font(11.0f, juce::Font::plain));
        lbl.setJustificationType(juce::Justification::centredBottom);
        addAndMakeVisible(lbl);
        box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff10343a));
        addAndMakeVisible(box);
    };
    setupCombo(resolution_, resLbl_,  "Resolution");
    setupCombo(lastStep_,   lastLbl_, "Last Step");
    setupCombo(type_,       typeLbl_, "Type");
    setupCombo(keySync_,    syncLbl_, "Key Sync");
    setupCombo(runMode_,    runLbl_,  "Run");

    int id = 1;
    for (auto* s : modseq::resolutionLabels()) resolution_.addItem(s, id++);
    for (int i = 1; i <= 16; ++i) lastStep_.addItem(juce::String(i), i);
    type_.addItem("Forward", 1); type_.addItem("Reverse", 2);
    type_.addItem("Alt 1", 3);   type_.addItem("Alt 2", 4);
    keySync_.addItem("Off", 1); keySync_.addItem("Timbre", 2); keySync_.addItem("Voice", 3);
    runMode_.addItem("1-Shot", 1); runMode_.addItem("Loop", 2);

    resolution_.onChange = [this] { modseq::setResolution(prog_, resolution_.getSelectedId() - 1); pushEdit(); };
    lastStep_.onChange   = [this] { modseq::setLastStep(prog_, lastStep_.getSelectedId() - 1); syncLastStepDimming(); pushEdit(); };
    type_.onChange       = [this] { modseq::setSeqType(prog_, type_.getSelectedId() - 1); pushEdit(); };
    keySync_.onChange    = [this] { modseq::setKeySync(prog_, keySync_.getSelectedId() - 1); pushEdit(); };
    runMode_.onChange    = [this] { modseq::setRunMode(prog_, runMode_.getSelectedId() - 1); pushEdit(); };

    for (int l = 0; l < modseq::kLanes; ++l) {
        auto lane = std::make_unique<LaneStrip>(l, prog_, [this] { pushEdit(); });
        addAndMakeVisible(*lane);
        lanes_.push_back(std::move(lane));
    }
    refresh();
}

void ModSeqPanel::pushEdit() { if (onEdit_) onEdit_(); }

void ModSeqPanel::syncLastStepDimming() {
    const int last = modseq::lastStep(prog_);
    for (auto& l : lanes_) l->setLastStep(last);
}

void ModSeqPanel::resized() {
    auto r = getLocalBounds().reduced(kPad);

    auto top = r.removeFromTop(kCommonH);
    on_.setBounds(top.removeFromLeft(96).withSizeKeepingCentre(96, 24));
    top.removeFromLeft(10);
    auto placeCombo = [&top](juce::Label& lbl, juce::ComboBox& box, int w) {
        auto col = top.removeFromLeft(w);
        lbl.setBounds(col.removeFromTop(16));
        box.setBounds(col.removeFromTop(24));
        top.removeFromLeft(8);
    };
    placeCombo(resLbl_,  resolution_, 86);
    placeCombo(lastLbl_, lastStep_,   70);
    placeCombo(typeLbl_, type_,       86);
    placeCombo(syncLbl_, keySync_,    86);
    placeCombo(runLbl_,  runMode_,    86);

    r.removeFromTop(6);
    const int n = (int)lanes_.size();
    const int laneH = r.getHeight() / juce::jmax(1, n);
    for (auto& l : lanes_) l->setBounds(r.removeFromTop(laneH));
}

void ModSeqPanel::paint(juce::Graphics& g) {
    g.setColour(juce::Colour(Palette::panelDark));
    g.fillAll();
    g.setColour(juce::Colour(Palette::accent));
    g.fillRect(0, 0, getWidth(), 2);
}

void ModSeqPanel::refresh() {
    on_.setToggleState(modseq::on(prog_), juce::dontSendNotification);
    resolution_.setSelectedId(modseq::resolution(prog_) + 1, juce::dontSendNotification);
    lastStep_.setSelectedId(modseq::lastStep(prog_) + 1, juce::dontSendNotification);
    type_.setSelectedId(modseq::seqType(prog_) + 1, juce::dontSendNotification);
    keySync_.setSelectedId(modseq::keySync(prog_) + 1, juce::dontSendNotification);
    runMode_.setSelectedId(modseq::runMode(prog_) + 1, juce::dontSendNotification);
    for (auto& l : lanes_) l->refresh();
    syncLastStepDimming();
}

} // namespace ms2000
