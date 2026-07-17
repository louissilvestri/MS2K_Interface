// MS2K_Interface — standalone JUCE app (Milestone 3: single-timbre editor).
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>
#include "midi/MidiEngine.h"
#include "ui/Components.h"
#include "ui/ModSeqPanel.h"
#include "ui/MS2000LookAndFeel.h"

using namespace ms2000;

class MainComponent : public juce::Component, public juce::ListBoxModel {
public:
    MainComponent() {
        // ---- engine + edit callback ----
        auto onEdit = [this](const ParamSpec& s, int raw) { engine_.sendParam(s, raw, prog_); };
        canvas_ = std::make_unique<LayoutCanvas>(prog_, onEdit);
        viewport_.setViewedComponent(canvas_.get(), false);
        viewport_.setScrollBarsShown(true, true);
        addAndMakeVisible(viewport_);

        // Mod Sequencer panel (its own area below the editor). Edits coalesce
        // into one debounced full dump — the seq has no destination-independent CC.
        modSeq_ = std::make_unique<ModSeqPanel>(prog_, [this] { engine_.sendProgramDebounced(prog_); });
        addChildComponent(*modSeq_);
        modSeq_->setVisible(modSeqVisible_);

        engine_.onProgramReceived = [this](Program p) {
            juce::MessageManager::callAsync([this, p] { loadIntoEditor(p); });
        };
        engine_.onBankReceived = [this](std::vector<Program> b) {
            juce::MessageManager::callAsync([this, b] {
                bank_ = b;
                bankList_.updateContent();
                bankList_.repaint();
                if (!bank_.empty()) { bankList_.selectRow(0); loadIntoEditor(bank_[0]); }
            });
        };
        engine_.onStatus = [this](juce::String s) {
            juce::MessageManager::callAsync([this, s] { setStatus(s); });
        };
        engine_.onParamFromSynth = [this](const ParamSpec* spec, int raw) {
            juce::MessageManager::callAsync([this, spec, raw] {
                setDisplay(prog_, 0, *spec, raw - spec->displayOffset); // update model
                canvas_->refreshParam(spec);                            // live UI update
            });
        };

        // ---- top bar ----
        addAndMakeVisible(nameEditor_);
        nameEditor_.setText("Init Program", juce::dontSendNotification);
        nameEditor_.onTextChange = [this] { prog_.setName(nameEditor_.getText().toStdString()); };

        addAndMakeVisible(voiceMode_);
        voiceMode_.addItem("Single", 1);
        voiceMode_.addItem("Layer", 2);
        voiceMode_.addItem("Vocoder", 3);
        voiceMode_.setSelectedId(1, juce::dontSendNotification);
        voiceMode_.onChange = [this] {
            prog_.setVoiceMode(voiceModeFor(voiceMode_.getSelectedId()));
            engine_.sendCurrentProgram(prog_); // voice mode has no CC
        };

        setupCombo(outBox_, engine_.outputNames(), "MIDI Out");
        outBox_.onChange = [this] { engine_.openOutput(outBox_.getSelectedId() - 2); };
        setupCombo(inBox_, engine_.inputNames(), "MIDI In");
        inBox_.onChange = [this] { engine_.openInput(inBox_.getSelectedId() - 2); };

        addAndMakeVisible(channelBox_);
        for (int i = 1; i <= 16; ++i) channelBox_.addItem("Ch " + juce::String(i), i);
        channelBox_.setSelectedId(1, juce::dontSendNotification);
        channelBox_.onChange = [this] { engine_.setChannel(channelBox_.getSelectedId() - 1); };

        addAndMakeVisible(requestBtn_);
        requestBtn_.setButtonText("Get Current");
        requestBtn_.onClick = [this] { engine_.requestCurrentProgram(); };

        addAndMakeVisible(sendBtn_);
        sendBtn_.setButtonText("Send to Synth");
        sendBtn_.onClick = [this] { engine_.sendCurrentProgram(prog_); };

        addAndMakeVisible(seqBtn_);
        seqBtn_.setButtonText("Mod Seq");
        seqBtn_.setClickingTogglesState(true);
        seqBtn_.setToggleState(modSeqVisible_, juce::dontSendNotification);
        seqBtn_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Palette::accent));
        seqBtn_.onClick = [this] {
            modSeqVisible_ = seqBtn_.getToggleState();
            modSeq_->setVisible(modSeqVisible_);
            resized();
        };

        addAndMakeVisible(editBtn_);
        editBtn_.setButtonText("UI Edit");
        editBtn_.setClickingTogglesState(true);
        editBtn_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Palette::accent));
        editBtn_.onClick = [this] {
            const bool on = editBtn_.getToggleState();
            canvas_->setEditMode(on);
            setStatus(on ? "UI Edit ON: drag a section to move it; drag its bottom-right corner to resize. Click UI Edit to finish."
                         : "UI Edit off - layout saved.");
        };

        // ---- librarian sidebar ----
        addAndMakeVisible(getAllBtn_);
        getAllBtn_.setButtonText("Get All Patches");
        getAllBtn_.onClick = [this] { engine_.requestAllPrograms(); };
        addAndMakeVisible(loadBtn_);
        loadBtn_.setButtonText("Load .syx");
        loadBtn_.onClick = [this] { loadSyxFile(); };
        addAndMakeVisible(saveBtn_);
        saveBtn_.setButtonText("Save .syx");
        saveBtn_.onClick = [this] { saveSyxFile(); };

        bankList_.setModel(this);
        bankList_.setRowHeight(18);
        bankList_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0e3236));
        addAndMakeVisible(bankList_);

        addAndMakeVisible(statusLabel_);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(Palette::textValue));
        statusLabel_.setFont(juce::Font(12.0f, juce::Font::plain));
        setStatus("Select MIDI Out + In and your synth's channel, then Get Current / Get All Patches.");

        setSize(1240, 820);
        restoreState();
    }

    ~MainComponent() override { saveState(); }

    // ---- ListBoxModel ----
    int getNumRows() override { return (int)bank_.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override {
        if (row < 0 || row >= (int)bank_.size()) return;
        if (selected) { g.setColour(juce::Colour(Palette::accent)); g.fillRect(0, 0, w, h); }
        g.setColour(juce::Colour(selected ? 0xff10262a : (juce::uint32)Palette::textLight));
        g.setFont(juce::Font(12.0f, juce::Font::plain));
        auto name = juce::String(bank_[(size_t)row].name());
        g.drawText(juce::String(row + 1).paddedLeft('0', 3) + "  " + name,
                   6, 0, w - 8, h, juce::Justification::centredLeft);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent&) override {
        if (row >= 0 && row < (int)bank_.size()) {
            loadIntoEditor(bank_[(size_t)row]);
            engine_.sendCurrentProgram(prog_);   // auto-send: the synth follows the click
        }
    }

    void paint(juce::Graphics& g) override {
        g.setGradientFill(juce::ColourGradient(juce::Colour(Palette::panelLight), 0, 0,
                                               juce::Colour(Palette::panelDark), 0, (float)getHeight(), false));
        g.fillAll();
        g.setColour(juce::Colour(Palette::rosewood));
        g.fillRect(0, 0, kCheek, getHeight());
        g.fillRect(getWidth() - kCheek, 0, kCheek, getHeight());
        // top-bar strip
        g.setColour(juce::Colour(0xff0e3236));
        g.fillRect(kCheek, 0, getWidth() - 2 * kCheek, kTopH);
        g.setColour(juce::Colour(Palette::accent));
        g.fillRect(kCheek, kTopH, getWidth() - 2 * kCheek, 2);
        // sidebar header
        g.setColour(juce::Colour(Palette::textLight));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("BANK  (" + juce::String((int)bank_.size()) + "/128)",
                   kCheek + 8, kTopH + 8, kSidebar - 16, 16, juce::Justification::centredLeft);
        // status strip
        g.setColour(juce::Colour(0xff0e3236));
        g.fillRect(kCheek, getHeight() - kStatusH, getWidth() - 2 * kCheek, kStatusH);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(kCheek, 0);
        auto top = r.removeFromTop(kTopH).reduced(6, 6);
        auto place = [&top](juce::Component& c, int w) { c.setBounds(top.removeFromLeft(w)); top.removeFromLeft(6); };
        place(nameEditor_, 150);
        place(voiceMode_, 96);
        place(outBox_, 160);
        place(inBox_, 160);
        place(channelBox_, 70);
        place(requestBtn_, 110);
        place(sendBtn_, 120);
        place(seqBtn_, 92);
        place(editBtn_, 92);

        statusLabel_.setBounds(r.removeFromBottom(kStatusH).reduced(8, 2));

        auto side = r.removeFromLeft(kSidebar).reduced(8, 8);
        side.removeFromTop(22); // header drawn in paint()
        getAllBtn_.setBounds(side.removeFromTop(28)); side.removeFromTop(6);
        auto row = side.removeFromTop(24);
        loadBtn_.setBounds(row.removeFromLeft((row.getWidth() - 6) / 2));
        row.removeFromLeft(6); saveBtn_.setBounds(row);
        side.removeFromTop(8);
        bankList_.setBounds(side);

        if (modSeqVisible_) {
            const int seqH = juce::jlimit(220, 460, r.getHeight() * 42 / 100);
            modSeq_->setBounds(r.removeFromBottom(seqH));
            r.removeFromBottom(4);
        }
        viewport_.setBounds(r);
        const int cw = r.getWidth() - 14; // room for the scrollbar
        canvas_->setSize(cw, juce::jmax(10, canvas_->getHeight()));
    }

private:
    void setStatus(const juce::String& s) { statusLabel_.setText(s, juce::dontSendNotification); }

    void loadIntoEditor(const Program& p) {
        prog_ = p;
        nameEditor_.setText(prog_.name(), juce::dontSendNotification);
        voiceMode_.setSelectedId(voiceIdFor(prog_.voiceMode()), juce::dontSendNotification);
        canvas_->refreshAll();
        modSeq_->refresh();
    }

    // ---- session persistence (MIDI selections, channel, loaded bank) ----
    static juce::File appDir() {
        auto d = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                     .getChildFile("MS2K_Interface");
        d.createDirectory();
        return d;
    }
    static void selectComboByText(juce::ComboBox& box, const juce::String& text) {
        if (text.isEmpty()) return;
        for (int i = 0; i < box.getNumItems(); ++i)
            if (box.getItemText(i) == text) { box.setSelectedId(box.getItemId(i)); return; }
    }

    void saveState() {
        juce::PropertiesFile props(appDir().getChildFile("settings.xml"), {});
        props.setValue("midiOut", outBox_.getText());
        props.setValue("midiIn", inBox_.getText());
        props.setValue("channel", channelBox_.getSelectedId() - 1);
        props.setValue("bankRow", bankList_.getSelectedRow());
        props.setValue("layout3", canvas_->getLayoutString());
        props.setValue("ctrlLayout", canvas_->getControlLayoutString());
        props.setValue("modSeqVisible", modSeqVisible_);
        props.saveIfNeeded();

        auto bankFile = appDir().getChildFile("last_bank.syx");
        if (!bank_.empty()) {
            std::vector<std::vector<uint8_t>> pb;
            for (auto& p : bank_) pb.emplace_back(p.bytes().begin(), p.bytes().end());
            auto syx = makeBankDump(pb, (uint8_t)engine_.channel(), Func::ProgramDump);
            bankFile.replaceWithData(syx.data(), syx.size());
        } else {
            bankFile.deleteFile();
        }
    }

    void restoreState() {
        juce::PropertiesFile props(appDir().getChildFile("settings.xml"), {});
        const int ch = props.getIntValue("channel", 0);
        channelBox_.setSelectedId(ch + 1, juce::dontSendNotification);
        engine_.setChannel(ch);
        selectComboByText(outBox_, props.getValue("midiOut")); // fires onChange -> openOutput
        selectComboByText(inBox_, props.getValue("midiIn"));   // fires onChange -> openInput
        canvas_->setLayoutString(props.getValue("layout3"));   // custom section arrangement
        canvas_->setControlLayoutString(props.getValue("ctrlLayout")); // custom control positions
        modSeqVisible_ = props.getBoolValue("modSeqVisible", true);
        seqBtn_.setToggleState(modSeqVisible_, juce::dontSendNotification);
        modSeq_->setVisible(modSeqVisible_);

        auto bankFile = appDir().getChildFile("last_bank.syx");
        if (bankFile.existsAsFile()) {
            juce::MemoryBlock mb;
            if (bankFile.loadFileAsData(mb)) {
                loadSyxData(std::vector<uint8_t>((uint8_t*)mb.getData(),
                                                 (uint8_t*)mb.getData() + mb.getSize()));
                const int row = props.getIntValue("bankRow", 0);
                if (row >= 0 && row < (int)bank_.size()) {
                    bankList_.selectRow(row);
                    loadIntoEditor(bank_[(size_t)row]); // load only — never auto-send on launch
                }
            }
        }
    }

    static std::vector<std::vector<uint8_t>> splitSysex(const std::vector<uint8_t>& d) {
        std::vector<std::vector<uint8_t>> msgs;
        for (size_t i = 0; i < d.size();) {
            if (d[i] != 0xF0) { ++i; continue; }
            size_t j = i + 1;
            while (j < d.size() && d[j] != 0xF7) ++j;
            if (j < d.size()) { msgs.emplace_back(d.begin() + i, d.begin() + j + 1); i = j + 1; }
            else break;
        }
        return msgs;
    }

    void loadSyxData(const std::vector<uint8_t>& data) {
        std::vector<Program> progs;
        for (auto& msg : splitSysex(data)) {
            if (auto bank = parseBankDump(msg))
                for (auto& b : *bank) { std::array<uint8_t, kProgramSize> a{}; std::copy_n(b.begin(), kProgramSize, a.begin()); progs.emplace_back(a); }
            else if (auto one = parseProgramDump(msg)) { std::array<uint8_t, kProgramSize> a{}; std::copy_n(one->begin(), kProgramSize, a.begin()); progs.emplace_back(a); }
        }
        if (progs.empty()) { setStatus("No MS2000 programs found in that file."); return; }
        if (progs.size() == 1) { loadIntoEditor(progs[0]); setStatus("Loaded 1 program from file."); return; }
        bank_ = progs; bankList_.updateContent(); bankList_.repaint();
        bankList_.selectRow(0); loadIntoEditor(bank_[0]);
        setStatus("Loaded " + juce::String((int)bank_.size()) + " programs from file.");
        repaint();
    }

    void loadSyxFile() {
        fileChooser_ = std::make_unique<juce::FileChooser>("Load MS2000 .syx", juce::File(), "*.syx;*.SYX");
        fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                juce::MemoryBlock mb;
                if (!f.loadFileAsData(mb)) { setStatus("Could not read file."); return; }
                loadSyxData(std::vector<uint8_t>((uint8_t*)mb.getData(), (uint8_t*)mb.getData() + mb.getSize()));
            });
    }

    void saveSyxFile() {
        if (bank_.empty()) { setStatus("No bank to save — Get All Patches or load a .syx first."); return; }
        fileChooser_ = std::make_unique<juce::FileChooser>("Save bank as .syx",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MS2000_bank.syx"),
            "*.syx");
        fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                std::vector<std::vector<uint8_t>> progBytes;
                for (auto& p : bank_) progBytes.emplace_back(p.bytes().begin(), p.bytes().end());
                auto syx = makeBankDump(progBytes, (uint8_t)engine_.channel(), Func::ProgramDump);
                if (f.replaceWithData(syx.data(), syx.size()))
                    setStatus("Saved " + juce::String((int)bank_.size()) + " programs to " + f.getFileName());
                else setStatus("Could not write file.");
            });
    }

    static VoiceMode voiceModeFor(int id) {
        return id == 2 ? VoiceMode::Layer : id == 3 ? VoiceMode::Vocoder : VoiceMode::Single;
    }
    static int voiceIdFor(VoiceMode m) {
        return m == VoiceMode::Layer ? 2 : m == VoiceMode::Vocoder ? 3 : 1;
    }
    void setupCombo(juce::ComboBox& box, const juce::StringArray& items, const juce::String& placeholder) {
        addAndMakeVisible(box);
        box.setTextWhenNothingSelected(placeholder);
        box.addItem("(none)", 1);
        int id = 2;
        for (auto& it : items) box.addItem(it, id++);
    }

    static constexpr int kCheek = 14;    // rosewood end-cheek width
    static constexpr int kTopH = 40;     // top bar height
    static constexpr int kSidebar = 230; // librarian sidebar width
    static constexpr int kStatusH = 24;  // status bar height

    Program     prog_;
    MidiEngine  engine_;
    juce::Viewport viewport_;
    std::unique_ptr<LayoutCanvas> canvas_;
    std::unique_ptr<ModSeqPanel>  modSeq_;
    bool modSeqVisible_ = true;

    juce::TextEditor nameEditor_;
    juce::ComboBox   voiceMode_, outBox_, inBox_, channelBox_;
    juce::TextButton requestBtn_, sendBtn_, seqBtn_, editBtn_;

    // librarian
    std::vector<Program> bank_;
    juce::ListBox    bankList_;
    juce::TextButton getAllBtn_, loadBtn_, saveBtn_;
    juce::Label      statusLabel_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

class MainWindow : public juce::DocumentWindow {
public:
    MainWindow() : juce::DocumentWindow("Korg MS2000 Editor",
                                        juce::Colour(ms2000::Palette::panelDark),
                                        juce::DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class MS2KApp : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "MS2K_Interface"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    void initialise(const juce::String&) override {
        juce::LookAndFeel::setDefaultLookAndFeel(&laf_);
        window_ = std::make_unique<MainWindow>();
    }
    void shutdown() override {
        window_ = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }
private:
    MS2000LookAndFeel laf_;
    std::unique_ptr<MainWindow> window_;
};

START_JUCE_APPLICATION(MS2KApp)
