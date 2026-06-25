// MS2000LookAndFeel — hardware-inspired styling: dark teal panel, black knobs
// with white pointers, white silkscreen labels, amber LED toggles.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ms2000 {

struct Palette {
    static constexpr juce::uint32 panelDark   = 0xff123c42; // teal body (dark)
    static constexpr juce::uint32 panelLight  = 0xff1b545c; // teal body (light)
    static constexpr juce::uint32 sectionFill = 0xff194d54; // section box
    static constexpr juce::uint32 sectionLine = 0xff0b2b30; // section border
    static constexpr juce::uint32 knobBody    = 0xff232629; // near-black knob
    static constexpr juce::uint32 knobEdge    = 0xff0a0b0c;
    static constexpr juce::uint32 pointer      = 0xfff2f4f4; // white indicator
    static constexpr juce::uint32 textLight    = 0xffeef5f5; // labels
    static constexpr juce::uint32 textValue     = 0xffaad8dc; // value readouts (cyan)
    static constexpr juce::uint32 accent        = 0xffff8c1a; // amber value arc
    static constexpr juce::uint32 accentDim      = 0xff0d2f33; // dim
    static constexpr juce::uint32 rosewood       = 0xff5a3a26; // end cheeks
    static constexpr juce::uint32 ledOn          = 0xffe5392f; // red panel LED (selected)
    static constexpr juce::uint32 ledOff         = 0xff241712; // unlit LED
};

class MS2000LookAndFeel : public juce::LookAndFeel_V4 {
public:
    MS2000LookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool highlighted, bool down) override;

    void drawComboBox(juce::Graphics&, int w, int h, bool down,
                      int bx, int by, int bw, int bh, juce::ComboBox&) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
};

} // namespace ms2000
