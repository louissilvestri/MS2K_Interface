#include "PluginEditor.h"

namespace ms2000 {

MS2KPluginEditor::MS2KPluginEditor(MS2KAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), proc_(p) {
    setLookAndFeel(&laf_);

    canvas_ = std::make_unique<LayoutCanvas>(editProg_,
        [this](const ParamSpec& s, int raw) { onSynthEdit(s, raw); });
    viewport_.setViewedComponent(canvas_.get(), false);
    viewport_.setScrollBarsShown(true, true);
    addAndMakeVisible(viewport_);

    modSeq_ = std::make_unique<ModSeqPanel>(editProg_, [this] { onSeqEdit(); });
    addChildComponent(*modSeq_);
    modSeq_->setVisible(modSeqVisible_);

    title_.setText("MS2000  \xe2\x80\xa2  Editor (VST3)", juce::dontSendNotification);
    title_.setColour(juce::Label::textColourId, juce::Colour(Palette::textLight));
    title_.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(title_);

    addAndMakeVisible(channelBox_);
    for (int i = 1; i <= 16; ++i) channelBox_.addItem("Ch " + juce::String(i), i);
    channelBox_.setSelectedId(proc_.channel() + 1, juce::dontSendNotification);
    channelBox_.onChange = [this] { proc_.setChannel(channelBox_.getSelectedId() - 1); };

    addAndMakeVisible(getBtn_);
    getBtn_.setButtonText("Get Current");
    getBtn_.onClick = [this] { proc_.requestCurrentProgram(); };

    addAndMakeVisible(sendBtn_);
    sendBtn_.setButtonText("Send All");
    sendBtn_.onClick = [this] { proc_.loadFullProgram(editProg_, true); };

    addAndMakeVisible(listenBtn_);
    listenBtn_.setButtonText("Listen to synth");
    listenBtn_.setColour(juce::ToggleButton::textColourId, juce::Colour(Palette::textLight));
    listenBtn_.onClick = [this] { proc_.setListening(listenBtn_.getToggleState()); };

    addAndMakeVisible(bankBtn_);
    bankBtn_.setButtonText("Bank");
    bankBtn_.setClickingTogglesState(true);
    bankBtn_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Palette::accent));
    bankBtn_.onClick = [this] { bankVisible_ = bankBtn_.getToggleState(); resized(); };

    addAndMakeVisible(seqBtn_);
    seqBtn_.setButtonText("Mod Seq");
    seqBtn_.setClickingTogglesState(true);
    seqBtn_.setToggleState(modSeqVisible_, juce::dontSendNotification);
    seqBtn_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Palette::accent));
    seqBtn_.onClick = [this] { modSeqVisible_ = seqBtn_.getToggleState();
                               modSeq_->setVisible(modSeqVisible_); resized(); };

    // ---- librarian sidebar ----
    addChildComponent(getAllBtn_);
    getAllBtn_.setButtonText("Get All Patches");
    getAllBtn_.onClick = [this] { proc_.logMsg("[UI] Get All Patches clicked"); proc_.requestAllPrograms(); };
    addChildComponent(loadBtn_);
    loadBtn_.setButtonText("Load .syx");
    loadBtn_.onClick = [this] { loadSyxFile(); };
    addChildComponent(saveBtn_);
    saveBtn_.setButtonText("Save .syx");
    saveBtn_.onClick = [this] { saveSyxFile(); };
    bankList_.setModel(this);
    bankList_.setRowHeight(18);
    bankList_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0e3236));
    addChildComponent(bankList_);

    hint_.setText("Route this track's MIDI out to the MS2000. Edits & automation are sent as MIDI; loading a patch sends it.",
                  juce::dontSendNotification);
    hint_.setColour(juce::Label::textColourId, juce::Colour(Palette::textValue));
    hint_.setFont(juce::Font(11.0f, juce::Font::plain));
    addAndMakeVisible(hint_);

    // Reflect restored/initial parameter values in the display.
    for (int i = 0; i < (int)proc_.defs().size(); ++i)
        proc_.defs()[(size_t)i].applyRaw(editProg_, proc_.paramRaw(i));
    suppressEdits_ = true;
    canvas_->refreshAll();
    modSeq_->refresh();
    suppressEdits_ = false;

    setResizable(true, true);
    setSize(1180, 800);
    startTimerHz(20);
}

MS2KPluginEditor::~MS2KPluginEditor() {
    stopTimer();
    bankList_.setModel(nullptr);
    setLookAndFeel(nullptr);
}

void MS2KPluginEditor::onSynthEdit(const ParamSpec& s, int raw) {
    if (suppressEdits_) return;
    proc_.setParamRaw(proc_.defIndexForId(juce::String(s.id)), raw);
}

void MS2KPluginEditor::onSeqEdit() {
    if (suppressEdits_) return;
    const auto& defs = proc_.defs();
    for (int i = 0; i < (int)defs.size(); ++i)
        if (defs[(size_t)i].spec == nullptr)               // mod-seq params only
            proc_.setParamRaw(i, defs[(size_t)i].readRaw(editProg_));
}

void MS2KPluginEditor::reconcileFromParams() {
    const auto& defs = proc_.defs();
    bool changed = false;
    for (int i = 0; i < (int)defs.size(); ++i) {
        const int raw = proc_.paramRaw(i);
        if (defs[(size_t)i].readRaw(editProg_) != raw) {
            defs[(size_t)i].applyRaw(editProg_, raw);
            changed = true;
        }
    }
    if (changed) {
        suppressEdits_ = true;
        canvas_->refreshAll();
        modSeq_->refresh();
        suppressEdits_ = false;
    }
}

void MS2KPluginEditor::loadProgramIntoEditor(const Program& prog) {
    editProg_ = prog;
    proc_.loadFullProgram(prog, true);   // seed full bytes (name incl.) + send to synth
    suppressEdits_ = true;
    canvas_->refreshAll();
    modSeq_->refresh();
    suppressEdits_ = false;
}

void MS2KPluginEditor::timerCallback() {
    Program inc;
    std::vector<Program> incBank;
    if (proc_.takeIncomingBank(incBank)) {       // synth replied to "Get All"
        bank_ = std::move(incBank);
        proc_.logMsg("[UI] bank received -> populating list with " + juce::String((int)bank_.size()) + " rows");
        bankList_.updateContent();
        bankList_.repaint();
        if (!bankVisible_) { bankVisible_ = true; bankBtn_.setToggleState(true, juce::dontSendNotification); resized(); }
    }
    if (proc_.takeIncoming(inc)) {                // synth replied to "Get Current"
        editProg_ = inc;
        proc_.loadFullProgram(inc, false);        // seed full bytes; don't echo back
        suppressEdits_ = true;
        canvas_->refreshAll();
        modSeq_->refresh();
        suppressEdits_ = false;
    } else {
        reconcileFromParams();                    // follow host automation / synth knobs
    }
}

// ---- librarian -------------------------------------------------------------
void MS2KPluginEditor::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (row < 0 || row >= (int)bank_.size()) return;
    if (selected) { g.setColour(juce::Colour(Palette::accent)); g.fillRect(0, 0, w, h); }
    g.setColour(juce::Colour(selected ? 0xff10262a : (juce::uint32)Palette::textLight));
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText(juce::String(row + 1).paddedLeft('0', 3) + "  " + juce::String(bank_[(size_t)row].name()),
               6, 0, w - 8, h, juce::Justification::centredLeft);
}

void MS2KPluginEditor::listBoxItemClicked(int row, const juce::MouseEvent&) {
    if (row >= 0 && row < (int)bank_.size())
        loadProgramIntoEditor(bank_[(size_t)row]);  // load + send to synth
}

static std::vector<std::vector<uint8_t>> splitSysex(const std::vector<uint8_t>& d) {
    std::vector<std::vector<uint8_t>> msgs;
    for (size_t i = 0; i < d.size();) {
        if (d[i] != 0xF0) { ++i; continue; }
        size_t j = i + 1; while (j < d.size() && d[j] != 0xF7) ++j;
        if (j < d.size()) { msgs.emplace_back(d.begin() + i, d.begin() + j + 1); i = j + 1; }
        else break;
    }
    return msgs;
}

void MS2KPluginEditor::loadSyxData(const std::vector<uint8_t>& data) {
    auto mk = [](const Bytes& b) { std::array<uint8_t, kProgramSize> a{}; std::copy_n(b.begin(), kProgramSize, a.begin()); return Program(a); };
    std::vector<Program> progs;
    for (auto& msg : splitSysex(data)) {
        if (auto bank = parseBankDump(msg)) for (auto& b : *bank) progs.push_back(mk(b));
        else if (auto one = parseProgramDump(msg)) progs.push_back(mk(*one));
    }
    if (progs.empty()) return;
    if (progs.size() == 1) { loadProgramIntoEditor(progs[0]); return; } // single patch: load + send
    bank_ = std::move(progs);
    bankList_.updateContent();
    bankList_.repaint();
    if (!bankVisible_) { bankVisible_ = true; bankBtn_.setToggleState(true, juce::dontSendNotification); resized(); }
}

void MS2KPluginEditor::loadSyxFile() {
    fileChooser_ = std::make_unique<juce::FileChooser>("Load MS2000 .syx", juce::File(), "*.syx;*.SYX");
    fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto f = fc.getResult(); if (f == juce::File()) return;
            juce::MemoryBlock mb; if (!f.loadFileAsData(mb)) return;
            loadSyxData(std::vector<uint8_t>((uint8_t*)mb.getData(), (uint8_t*)mb.getData() + mb.getSize()));
        });
}

void MS2KPluginEditor::saveSyxFile() {
    if (bank_.empty()) return;
    fileChooser_ = std::make_unique<juce::FileChooser>("Save bank as .syx",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MS2000_bank.syx"), "*.syx");
    fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc) {
            auto f = fc.getResult(); if (f == juce::File()) return;
            std::vector<std::vector<uint8_t>> pb;
            for (auto& pr : bank_) pb.emplace_back(pr.bytes().begin(), pr.bytes().end());
            auto syx = makeBankDump(pb, (uint8_t)proc_.channel(), Func::ProgramDump);
            f.replaceWithData(syx.data(), syx.size());
        });
}

// ---- paint / layout --------------------------------------------------------
void MS2KPluginEditor::paint(juce::Graphics& g) {
    g.setGradientFill(juce::ColourGradient(juce::Colour(Palette::panelLight), 0, 0,
                                           juce::Colour(Palette::panelDark), 0, (float)getHeight(), false));
    g.fillAll();
    g.setColour(juce::Colour(Palette::rosewood));
    g.fillRect(0, 0, kCheek, getHeight());
    g.fillRect(getWidth() - kCheek, 0, kCheek, getHeight());
    g.setColour(juce::Colour(0xff0e3236));
    g.fillRect(kCheek, 0, getWidth() - 2 * kCheek, kTopH);
    g.setColour(juce::Colour(Palette::accent));
    g.fillRect(kCheek, kTopH, getWidth() - 2 * kCheek, 2);
}

void MS2KPluginEditor::resized() {
    auto r = getLocalBounds().reduced(kCheek, 0);
    auto top = r.removeFromTop(kTopH).reduced(6, 6);
    title_.setBounds(top.removeFromLeft(170));
    auto place = [&top](juce::Component& c, int w) { c.setBounds(top.removeFromLeft(w)); top.removeFromLeft(6); };
    place(channelBox_, 64);
    place(getBtn_, 96);
    place(sendBtn_, 78);
    place(listenBtn_, 116);
    place(bankBtn_, 60);
    place(seqBtn_, 84);

    hint_.setBounds(r.removeFromBottom(kStatusH).reduced(4, 2));

    const bool sb = bankVisible_;
    getAllBtn_.setVisible(sb); loadBtn_.setVisible(sb); saveBtn_.setVisible(sb); bankList_.setVisible(sb);
    if (sb) {
        auto side = r.removeFromLeft(kSidebar).reduced(6, 6);
        getAllBtn_.setBounds(side.removeFromTop(26)); side.removeFromTop(6);
        auto row = side.removeFromTop(24);
        loadBtn_.setBounds(row.removeFromLeft((row.getWidth() - 6) / 2));
        row.removeFromLeft(6); saveBtn_.setBounds(row);
        side.removeFromTop(6);
        bankList_.setBounds(side);
        r.removeFromLeft(4);
    }

    if (modSeqVisible_) {
        const int seqH = juce::jlimit(220, 460, r.getHeight() * 42 / 100);
        modSeq_->setBounds(r.removeFromBottom(seqH));
        r.removeFromBottom(4);
    }
    viewport_.setBounds(r);
    canvas_->setSize(r.getWidth() - 14, juce::jmax(10, canvas_->getHeight()));
}

} // namespace ms2000
