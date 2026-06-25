// UI building blocks generated from the ParameterModel table, styled to mimic
// the MS2000's panel. Sections live on a free-form LayoutCanvas that supports a
// drag-and-resize "UI Edit" mode.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <map>
#include "../model/ParameterModel.h"

namespace ms2000 {

using EditFn = std::function<void(const ParamSpec&, int)>;

// Panel-style option selector: a vertical list of labelled LEDs; the selected
// option's dot lights red (like the MS2000's front-panel selectors).
class RadioSelector : public juce::Component {
public:
    explicit RadioSelector(std::vector<juce::String> options);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void setSelectedIndex(int idx, juce::NotificationType);
    int  selectedIndex() const { return selected_; }
    std::function<void(int)> onChange;
private:
    std::vector<juce::String> options_;
    int selected_ = 0;
};

// One control bound to one ParamSpec. In UI Edit mode it can be dragged to a
// different cell within its section.
class ParamControl : public juce::Component {
public:
    ParamControl(const ParamSpec& spec, Program& prog, int timbre, EditFn onEdit);
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void refresh();
    const ParamSpec* specPtr() const { return &spec_; }
    juce::String paramId() const { return juce::String(spec_.id); }
    void setEditMode(bool on);
    void setApplyingLayout(bool b) { applyingLayout_ = b; }
    void setOverlapTest(std::function<bool(const juce::Rectangle<int>&)> f) { overlapTest_ = std::move(f); }
    void setCell(int c, int r) { col_ = c; row_ = r; }
    int  col() const { return col_; }
    int  row() const { return row_; }
    bool hasCell() const { return col_ >= 0; }
    std::function<void()> onMoved;

private:
    void pushEdit();
    const ParamSpec& spec_;
    Program&         prog_;
    int              timbre_;
    EditFn           onEdit_;
    juce::Label      label_;
    std::unique_ptr<juce::Slider>        slider_;
    std::unique_ptr<RadioSelector>       radio_;
    std::unique_ptr<juce::ToggleButton>  toggle_;
    bool editMode_ = false, applyingLayout_ = false;
    int  col_ = -1, row_ = -1;
    juce::ComponentDragger dragger_;
    std::unique_ptr<juce::ComponentBoundsConstrainer> cellConstrainer_;
    std::function<bool(const juce::Rectangle<int>&)> overlapTest_;
};

// A labeled section box (e.g. "FILTER") holding its controls in a compact grid.
// In edit mode it can be dragged to move and has a bottom-right resize corner.
class SectionPanel : public juce::Component {
public:
    SectionPanel(Section sec, const std::vector<const ParamSpec*>& specs,
                 Program& prog, int timbre, EditFn onEdit);
    void paint(juce::Graphics&) override;
    void resized() override;
    void moved() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void refreshAll();
    void refreshParam(const ParamSpec* s); // refresh just one control if present
    int  preferredWidth() const;
    int  preferredHeight() const;
    int  sectionId() const { return (int)section_; }
    void setEditMode(bool on);
    void setApplyingLayout(bool b) { applyingLayout_ = b; }
    void setOverlapTest(std::function<bool(const juce::Rectangle<int>&)> f);
    void appendControlLayout(juce::String& out) const;        // persist control cells
    bool applyControlCell(const juce::String& id, int c, int r);

    std::function<void()> onLayoutChanged; // fired when the user drags/resizes the section

private:
    int columns() const;        // reflows with current width
    int defaultColumns() const; // initial flow column count
    void onControlMoved(ParamControl* c);
    Section section_;
    juce::String title_;
    std::vector<std::unique_ptr<ParamControl>> controls_;
    bool manualLayout_ = false; // user has dragged a control here
    bool editMode_ = false, applyingLayout_ = false;
    juce::ComponentDragger dragger_;
    std::unique_ptr<juce::ComponentBoundsConstrainer> constrainer_; // grid-snapping
    std::unique_ptr<juce::ResizableCornerComponent> corner_;
    std::function<bool(const juce::Rectangle<int>&)> overlapTest_;
};

// Holds every section panel on one scrollable canvas. Default arrangement is the
// signal-flow flow; the user can rearrange/resize in edit mode and the layout
// is serialized to/from a string for persistence.
class LayoutCanvas : public juce::Component {
public:
    LayoutCanvas(Program& prog, EditFn onEdit);
    void resized() override;
    void refreshAll();
    void refreshParam(const ParamSpec* s); // live update of one control
    void setEditMode(bool on);
    juce::String getLayoutString() const;
    void setLayoutString(const juce::String& s);
    juce::String getControlLayoutString() const;
    void setControlLayoutString(const juce::String& s);

    std::function<void()> onLayoutChanged; // for persistence (mark dirty)

private:
    void ensureDefaults(int width);
    void applyLayout();
    void captureFromPanels();
    juce::Rectangle<int> contentExtent() const;
    bool overlapsOthers(const SectionPanel* self, const juce::Rectangle<int>& r) const;

    std::vector<std::unique_ptr<SectionPanel>> panels_;
    std::map<int, juce::Rectangle<int>> layout_;
    bool edit_ = false;
};

std::vector<const ParamSpec*> timbreSpecs();
std::vector<const ParamSpec*> commonSpecs();
const char* sectionName(Section s);

} // namespace ms2000
