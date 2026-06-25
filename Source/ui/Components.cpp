#include "Components.h"
#include "MS2000LookAndFeel.h"

namespace ms2000 {

// Cell + box metrics. One grid unit == one knob cell (kCtlW x kCtlH), which
// already includes room for the control's label and value readout.
static constexpr int kCtlW = 84, kCtlH = 104, kHeader = 22, kPad = 6;
static constexpr int kInsetX = 4, kInsetY = 2; // control inset within its cell

namespace {
// Snaps a section's bounds to the knob grid: position to (kCtlW, kCtlH)
// multiples, width to a whole number of control columns, and height to at least
// the rows needed for all controls (so nothing is clipped on the right/bottom).
class GridConstrainer : public juce::ComponentBoundsConstrainer {
public:
    int count = 1;
    std::function<bool(const juce::Rectangle<int>&)>* overlap = nullptr; // hits another section?
    void checkBounds(juce::Rectangle<int>& b, const juce::Rectangle<int>& prev,
                     const juce::Rectangle<int>&, bool, bool, bool, bool) override {
        const int n = juce::jmax(1, count);
        const int x = juce::jmax(0, ((b.getX() + kCtlW / 2) / kCtlW) * kCtlW);
        // Y is free (not snapped): a section's height includes the header, so it
        // never lands on a coarse Y grid — collision-prevention lets the user drag
        // sections flush against each other instead.
        const int y = juce::jmax(0, b.getY());
        const int cols = juce::jlimit(1, n, (b.getWidth() + kCtlW / 2) / kCtlW);
        const int reqRows = (n + cols - 1) / cols;                 // rows needed for all controls
        const int wantRows = juce::jmax(1, (b.getHeight() - kHeader + kCtlH / 2) / kCtlH);
        const int rows = juce::jmax(reqRows, wantRows);
        const juce::Rectangle<int> snapped(x, y, cols * kCtlW, kHeader + rows * kCtlH);
        // Reject a move/resize that would overlap another section (stay put).
        b = (overlap && *overlap && (*overlap)(snapped)) ? prev : snapped;
    }
};

// Snaps a control to a cell within its section (overlap allowed during the
// drag — controls SWAP on drop, since the section grid has no empty cells).
class CtrlGridConstrainer : public juce::ComponentBoundsConstrainer {
public:
    juce::Component* self = nullptr;
    void checkBounds(juce::Rectangle<int>& b, const juce::Rectangle<int>& prev,
                     const juce::Rectangle<int>&, bool, bool, bool, bool) override {
        auto* parent = self ? self->getParentComponent() : nullptr;
        const int pw = parent ? parent->getWidth() : b.getRight();
        const int ph = parent ? parent->getHeight() : b.getBottom();
        const int cols = juce::jmax(1, pw / kCtlW);
        const int rows = juce::jmax(1, (ph - kHeader) / kCtlH);
        const int col = juce::jlimit(0, cols - 1, (b.getX() - kInsetX + kCtlW / 2) / kCtlW);
        const int row = juce::jlimit(0, rows - 1, (b.getY() - kHeader - kInsetY + kCtlH / 2) / kCtlH);
        b.setBounds(col * kCtlW + kInsetX, kHeader + row * kCtlH + kInsetY,
                    prev.getWidth(), prev.getHeight());
    }
};
} // namespace

const char* sectionName(Section s) {
    switch (s) {
        case Section::Voice:      return "VOICE";
        case Section::Osc1:       return "OSC 1";
        case Section::Osc2:       return "OSC 2";
        case Section::Mixer:      return "MIXER";
        case Section::Filter:     return "FILTER";
        case Section::Amp:        return "AMP";
        case Section::Eg1:        return "EG 1  FILTER";
        case Section::Eg2:        return "EG 2  AMP";
        case Section::Lfo1:       return "LFO 1";
        case Section::Lfo2:       return "LFO 2";
        case Section::Patch:      return "VIRTUAL PATCH";
        case Section::Portamento: return "PORTAMENTO";
        case Section::Effects:    return "EFFECTS";
        case Section::Eq:         return "EQ";
        case Section::Arp:        return "ARPEGGIATOR";
    }
    return "?";
}

// Signal-flow ordering that mirrors the hardware panel (left->right, top->down).
static int flowRank(Section s) {
    switch (s) {
        case Section::Osc1:       return 0;
        case Section::Osc2:       return 1;
        case Section::Mixer:      return 2;
        case Section::Filter:     return 3;
        case Section::Amp:        return 4;
        case Section::Eg1:        return 5;
        case Section::Eg2:        return 6;
        case Section::Lfo1:       return 7;
        case Section::Lfo2:       return 8;
        case Section::Patch:      return 9;
        case Section::Effects:    return 10;
        case Section::Portamento: return 11;
        case Section::Arp:        return 12;
        case Section::Voice:      return 13;
        case Section::Eq:         return 14;
    }
    return 99;
}

std::vector<const ParamSpec*> timbreSpecs() {
    std::vector<const ParamSpec*> v;
    for (const auto& s : parameterTable())
        if (s.scope == Scope::Timbre) v.push_back(&s);
    return v;
}
std::vector<const ParamSpec*> commonSpecs() {
    std::vector<const ParamSpec*> v;
    for (const auto& s : parameterTable())
        if (s.scope == Scope::Program) v.push_back(&s);
    return v;
}

// ---------------- RadioSelector ----------------
RadioSelector::RadioSelector(std::vector<juce::String> opts) : options_(std::move(opts)) {}

void RadioSelector::paint(juce::Graphics& g) {
    const int n = juce::jmax(1, (int)options_.size());
    const float rowH = getHeight() / (float)n;
    g.setFont(juce::Font(juce::jlimit(10.0f, 14.0f, rowH - 4.0f), juce::Font::plain));
    for (int i = 0; i < (int)options_.size(); ++i) {
        const float cy = i * rowH + rowH * 0.5f;
        const float r = juce::jlimit(3.0f, 5.0f, rowH * 0.30f);
        juce::Rectangle<float> dot(4.0f, cy - r, r * 2.0f, r * 2.0f);
        const bool on = (i == selected_);
        if (on) { g.setColour(juce::Colour(Palette::ledOn).withAlpha(0.30f)); g.fillEllipse(dot.expanded(2.5f)); }
        g.setColour(juce::Colour(on ? Palette::ledOn : Palette::ledOff));
        g.fillEllipse(dot);
        g.setColour(juce::Colour(Palette::knobEdge));
        g.drawEllipse(dot, 1.0f);
        g.setColour(juce::Colour(on ? Palette::textLight : 0xff9fb6b6));
        g.drawText(options_[i], (int)(6 + r * 2 + 4), (int)(i * rowH),
                   getWidth() - (int)(10 + r * 2), (int)rowH, juce::Justification::centredLeft);
    }
}

void RadioSelector::mouseDown(const juce::MouseEvent& e) {
    const int n = juce::jmax(1, (int)options_.size());
    setSelectedIndex((int)(e.position.y / (getHeight() / (float)n)), juce::sendNotification);
}

void RadioSelector::setSelectedIndex(int idx, juce::NotificationType nt) {
    selected_ = juce::jlimit(0, juce::jmax(0, (int)options_.size() - 1), idx);
    repaint();
    if (nt == juce::sendNotification && onChange) onChange(selected_);
}

// ---------------- ParamControl ----------------
ParamControl::ParamControl(const ParamSpec& spec, Program& prog, int timbre, EditFn onEdit)
    : spec_(spec), prog_(prog), timbre_(timbre), onEdit_(std::move(onEdit)) {
    label_.setText(spec_.label, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(juce::Font(13.0f, juce::Font::plain));
    addAndMakeVisible(label_);

    switch (spec_.type) {
        case ValueType::Continuous: {
            slider_ = std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag,
                                                     juce::Slider::TextBoxBelow);
            slider_->setRange(spec_.displayMin, spec_.displayMax, 1.0);
            slider_->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
            slider_->getProperties().set("bipolar", spec_.bipolar);
            if (spec_.bipolar) slider_->setDoubleClickReturnValue(true, 0.0);
            slider_->onValueChange = [this] { pushEdit(); };
            addAndMakeVisible(*slider_);
            break;
        }
        case ValueType::Enum: {
            std::vector<juce::String> opts(spec_.enums.begin(), spec_.enums.end());
            radio_ = std::make_unique<RadioSelector>(std::move(opts));
            radio_->onChange = [this](int) { pushEdit(); };
            addAndMakeVisible(*radio_);
            break;
        }
        case ValueType::Bool: {
            toggle_ = std::make_unique<juce::ToggleButton>();
            toggle_->onClick = [this] { pushEdit(); };
            addAndMakeVisible(*toggle_);
            break;
        }
    }
    auto cc = std::make_unique<CtrlGridConstrainer>();
    cc->self = this;
    cellConstrainer_ = std::move(cc);
    refresh();
}

void ParamControl::setEditMode(bool on) {
    editMode_ = on;
    // In edit mode, disable the inner widgets so their drags don't change the
    // value — the clicks then fall through to this component, which drags.
    const bool childClicks = !on;
    if (slider_) slider_->setInterceptsMouseClicks(childClicks, childClicks);
    if (radio_)  radio_->setInterceptsMouseClicks(childClicks, childClicks);
    if (toggle_) toggle_->setInterceptsMouseClicks(childClicks, childClicks);
    setInterceptsMouseClicks(true, true); // this component always handles the drag
    setMouseCursor(on ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}
void ParamControl::mouseDown(const juce::MouseEvent& e) {
    if (editMode_) dragger_.startDraggingComponent(this, e);
}
void ParamControl::mouseDrag(const juce::MouseEvent& e) {
    if (editMode_) dragger_.dragComponent(this, e, cellConstrainer_.get());
}
void ParamControl::mouseUp(const juce::MouseEvent&) {
    if (editMode_ && onMoved) onMoved(); // drop -> section places/swaps this control
}

void ParamControl::refresh() {
    const int disp = getDisplay(prog_, timbre_, spec_);
    if (slider_) slider_->setValue(disp, juce::dontSendNotification);
    if (radio_)  radio_->setSelectedIndex(disp, juce::dontSendNotification);
    if (toggle_) toggle_->setToggleState(disp != 0, juce::dontSendNotification);
}

void ParamControl::pushEdit() {
    int disp = 0;
    if (slider_) disp = (int)slider_->getValue();
    else if (radio_) disp = radio_->selectedIndex();
    else if (toggle_) disp = toggle_->getToggleState() ? 1 : 0;
    setDisplay(prog_, timbre_, spec_, disp);
    if (onEdit_) onEdit_(spec_, getRaw(prog_, timbre_, spec_));
}

void ParamControl::resized() {
    auto r = getLocalBounds();
    label_.setBounds(r.removeFromTop(17));
    if (slider_) slider_->setBounds(r);
    else if (radio_) radio_->setBounds(r.reduced(2, 1));
    else if (toggle_) toggle_->setBounds(r.withSizeKeepingCentre(28, 28));
}

// ---------------- SectionPanel ----------------
SectionPanel::SectionPanel(Section sec, const std::vector<const ParamSpec*>& specs,
                           Program& prog, int timbre, EditFn onEdit)
    : section_(sec), title_(sectionName(sec)) {
    for (auto* s : specs) {
        auto pc = std::make_unique<ParamControl>(*s, prog, timbre, onEdit);
        auto* raw = pc.get();
        pc->onMoved = [this, raw] { onControlMoved(raw); };
        pc->setOverlapTest([this, raw](const juce::Rectangle<int>& r) {
            for (auto& o : controls_) if (o.get() != raw && o->getBounds().intersects(r)) return true;
            return false;
        });
        addAndMakeVisible(*pc);
        controls_.push_back(std::move(pc));
    }
    auto gc = std::make_unique<GridConstrainer>();
    gc->count = (int)controls_.size();
    gc->overlap = &overlapTest_;
    constrainer_ = std::move(gc);
    corner_ = std::make_unique<juce::ResizableCornerComponent>(this, constrainer_.get());
    addChildComponent(*corner_); // shown only in edit mode
}

void SectionPanel::setOverlapTest(std::function<bool(const juce::Rectangle<int>&)> f) {
    overlapTest_ = std::move(f);
}

// Columns reflow with the panel's current width: as it narrows, controls wrap
// down to the next row instead of being cut off on the right.
int SectionPanel::columns() const {
    const int n = juce::jmax(1, (int)controls_.size());
    return juce::jlimit(1, n, getWidth() / kCtlW);
}
int SectionPanel::defaultColumns() const {
    const int n = (int)controls_.size();
    return (section_ == Section::Patch) ? 3 : juce::jlimit(1, 4, juce::jmax(1, n));
}
int SectionPanel::preferredWidth() const { return defaultColumns() * kCtlW; }
int SectionPanel::preferredHeight() const {
    const int cols = defaultColumns();
    const int rows = ((int)controls_.size() + cols - 1) / cols;
    return kHeader + rows * kCtlH;
}

void SectionPanel::setEditMode(bool on) {
    editMode_ = on;
    corner_->setVisible(on);
    if (on) corner_->toFront(false);
    for (auto& c : controls_) c->setEditMode(on);   // controls become draggable
    setMouseCursor(on ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void SectionPanel::onControlMoved(ParamControl* c) {
    manualLayout_ = true;
    const int cols = juce::jmax(1, columns());
    const int rows = juce::jmax(1, (getHeight() - kHeader) / kCtlH);
    const auto b = c->getBounds();
    const int tcol = juce::jlimit(0, cols - 1, b.getCentreX() / kCtlW);
    const int trow = juce::jlimit(0, rows - 1, (b.getCentreY() - kHeader) / kCtlH);
    const int oldCol = c->col(), oldRow = c->row();
    // swap with whatever control currently occupies the drop cell
    for (auto& o : controls_)
        if (o.get() != c && o->col() == tcol && o->row() == trow) { o->setCell(oldCol, oldRow); break; }
    c->setCell(tcol, trow);
    resized();
}

void SectionPanel::appendControlLayout(juce::String& out) const {
    if (!manualLayout_) return;
    for (auto& c : controls_)
        out << c->paramId() << ":" << c->col() << "," << c->row() << ";";
}

bool SectionPanel::applyControlCell(const juce::String& id, int col, int row) {
    for (auto& c : controls_)
        if (c->paramId() == id) { c->setCell(col, row); manualLayout_ = true; return true; }
    return false;
}

void SectionPanel::mouseDown(const juce::MouseEvent& e) {
    if (editMode_) dragger_.startDraggingComponent(this, e);
}
void SectionPanel::mouseDrag(const juce::MouseEvent& e) {
    if (editMode_) dragger_.dragComponent(this, e, constrainer_.get());
}
void SectionPanel::moved() {
    if (editMode_ && !applyingLayout_ && onLayoutChanged) onLayoutChanged();
}

void SectionPanel::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(Palette::sectionFill));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(juce::Colour(editMode_ ? Palette::accent : Palette::sectionLine));
    g.drawRoundedRectangle(r, 5.0f, editMode_ ? 2.0f : 1.0f);
    // header
    auto hdr = juce::Rectangle<int>(0, 0, getWidth(), kHeader);
    g.setColour(juce::Colour(Palette::textLight));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText(title_, hdr.reduced(kPad, 2), juce::Justification::centredLeft);
    g.setColour(juce::Colour(Palette::accent));
    g.fillRect(kPad, kHeader - 3, getWidth() - 2 * kPad, 2);
}

void SectionPanel::resized() {
    const int cols = juce::jmax(1, columns());
    const int rows = juce::jmax(1, (getHeight() - kHeader) / kCtlH);
    int i = 0;
    for (auto& c : controls_) {
        int col, row;
        if (manualLayout_ && c->hasCell()) {        // user-placed cell
            col = juce::jlimit(0, cols - 1, c->col());
            row = juce::jlimit(0, rows - 1, c->row());
        } else {                                     // auto flow (row-major)
            col = i % cols; row = i / cols;
            c->setCell(col, row);
        }
        c->setApplyingLayout(true);
        c->setBounds(col * kCtlW + kInsetX, kHeader + row * kCtlH + kInsetY,
                     kCtlW - 2 * kInsetX, kCtlH - 2 * kInsetY);
        c->setApplyingLayout(false);
        ++i;
    }
    if (corner_) corner_->setBounds(getWidth() - 16, getHeight() - 16, 16, 16);
    if (editMode_ && !applyingLayout_ && onLayoutChanged) onLayoutChanged();
}

void SectionPanel::refreshAll() { for (auto& c : controls_) c->refresh(); }
void SectionPanel::refreshParam(const ParamSpec* s) {
    for (auto& c : controls_) if (c->specPtr() == s) { c->refresh(); return; }
}

// ---------------- LayoutCanvas ----------------
LayoutCanvas::LayoutCanvas(Program& prog, EditFn onEdit) {
    auto specs = timbreSpecs();
    auto cs = commonSpecs();
    specs.insert(specs.end(), cs.begin(), cs.end());
    std::stable_sort(specs.begin(), specs.end(),
        [](const ParamSpec* a, const ParamSpec* b){ return flowRank(a->section) < flowRank(b->section); });
    Section current = (Section)-1;
    std::vector<const ParamSpec*> bucket;
    auto flush = [&] {
        if (bucket.empty()) return;
        auto p = std::make_unique<SectionPanel>(bucket.front()->section, bucket, prog, 0, onEdit);
        p->onLayoutChanged = [this] { captureFromPanels(); };
        auto* raw = p.get();
        p->setOverlapTest([this, raw](const juce::Rectangle<int>& r) { return overlapsOthers(raw, r); });
        addAndMakeVisible(*p);
        panels_.push_back(std::move(p));
        bucket.clear();
    };
    for (auto* s : specs) {
        if (s->section != current) { flush(); current = s->section; }
        bucket.push_back(s);
    }
    flush();
}

void LayoutCanvas::ensureDefaults(int width) {
    // Flow on the grid: section widths/heights are whole knob-cell multiples and
    // tiles abut, so the default arrangement is already grid-aligned.
    int x = 0, y = 0, rowH = 0;
    for (auto& p : panels_) {
        const int id = p->sectionId(), pw = p->preferredWidth(), ph = p->preferredHeight();
        if (layout_.count(id)) continue;                 // keep custom positions
        if (x > 0 && x + pw > width) { x = 0; y += rowH; rowH = 0; }
        layout_[id] = { x, y, pw, ph };
        x += pw; rowH = juce::jmax(rowH, ph);
    }
}

void LayoutCanvas::applyLayout() {
    for (auto& p : panels_) {
        auto it = layout_.find(p->sectionId());
        if (it == layout_.end()) continue;
        p->setApplyingLayout(true);
        p->setBounds(it->second);
        p->setApplyingLayout(false);
    }
}

juce::Rectangle<int> LayoutCanvas::contentExtent() const {
    int right = 0, bottom = 0;
    for (auto& kv : layout_) {
        right = juce::jmax(right, kv.second.getRight());
        bottom = juce::jmax(bottom, kv.second.getBottom());
    }
    return { 0, 0, right, bottom };
}

bool LayoutCanvas::overlapsOthers(const SectionPanel* self, const juce::Rectangle<int>& r) const {
    for (auto& p : panels_)
        if (p.get() != self && p->getBounds().intersects(r)) return true;
    return false;
}

void LayoutCanvas::resized() {
    if (getWidth() <= 0) return;
    ensureDefaults(getWidth());
    applyLayout();
    auto ext = contentExtent();
    const int needW = juce::jmax(getWidth(), ext.getRight() + 8);
    const int needH = ext.getBottom() + 12;
    if (getWidth() != needW || getHeight() != needH) setSize(needW, needH); // guarded
}

void LayoutCanvas::captureFromPanels() {
    for (auto& p : panels_) layout_[p->sectionId()] = p->getBounds();
    auto ext = contentExtent();
    const int needW = juce::jmax(getWidth(), ext.getRight() + 8);
    const int needH = ext.getBottom() + 12;
    if (getWidth() != needW || getHeight() != needH) setSize(needW, needH);
    if (onLayoutChanged) onLayoutChanged();
}

void LayoutCanvas::setEditMode(bool on) {
    edit_ = on;
    for (auto& p : panels_) p->setEditMode(on);
}
void LayoutCanvas::refreshAll() { for (auto& p : panels_) p->refreshAll(); }
void LayoutCanvas::refreshParam(const ParamSpec* s) { for (auto& p : panels_) p->refreshParam(s); }

juce::String LayoutCanvas::getLayoutString() const {
    juce::String s;
    for (auto& kv : layout_) {
        const auto& r = kv.second;
        s << kv.first << ":" << r.getX() << "," << r.getY() << ","
          << r.getWidth() << "," << r.getHeight() << ";";
    }
    return s;
}

void LayoutCanvas::setLayoutString(const juce::String& str) {
    if (str.isEmpty()) return;
    layout_.clear();
    for (const auto& item : juce::StringArray::fromTokens(str, ";", "")) {
        if (item.isEmpty()) continue;
        auto kv = juce::StringArray::fromTokens(item, ":", "");
        if (kv.size() != 2) continue;
        auto nums = juce::StringArray::fromTokens(kv[1], ",", "");
        if (nums.size() != 4) continue;
        layout_[kv[0].getIntValue()] = { nums[0].getIntValue(), nums[1].getIntValue(),
                                         nums[2].getIntValue(), nums[3].getIntValue() };
    }
    if (getWidth() > 0) resized();
}

juce::String LayoutCanvas::getControlLayoutString() const {
    juce::String s;
    for (auto& p : panels_) p->appendControlLayout(s);
    return s;
}

void LayoutCanvas::setControlLayoutString(const juce::String& str) {
    if (str.isEmpty()) return;
    for (const auto& item : juce::StringArray::fromTokens(str, ";", "")) {
        if (item.isEmpty()) continue;
        auto kv = juce::StringArray::fromTokens(item, ":", "");
        if (kv.size() != 2) continue;
        auto nums = juce::StringArray::fromTokens(kv[1], ",", "");
        if (nums.size() != 2) continue;
        for (auto& p : panels_)
            if (p->applyControlCell(kv[0], nums[0].getIntValue(), nums[1].getIntValue())) break;
    }
    if (getWidth() > 0) resized();
}

} // namespace ms2000
