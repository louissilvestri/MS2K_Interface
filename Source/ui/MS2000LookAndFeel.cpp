#include "MS2000LookAndFeel.h"

namespace ms2000 {

static juce::Colour C(juce::uint32 c) { return juce::Colour(c); }

MS2000LookAndFeel::MS2000LookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, C(Palette::panelDark));
    setColour(juce::Label::textColourId,                 C(Palette::textLight));
    setColour(juce::Slider::textBoxTextColourId,         C(Palette::textValue));
    setColour(juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId,        C(0xff0e3236));
    setColour(juce::ComboBox::textColourId,              C(Palette::textLight));
    setColour(juce::ComboBox::outlineColourId,           C(Palette::sectionLine));
    setColour(juce::ComboBox::arrowColourId,             C(Palette::textValue));
    setColour(juce::PopupMenu::backgroundColourId,       C(0xff0e3236));
    setColour(juce::PopupMenu::textColourId,             C(Palette::textLight));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, C(Palette::accent));
    setColour(juce::TextButton::buttonColourId,          C(0xff0e3236));
    setColour(juce::TextButton::textColourOnId,          C(Palette::textLight));
    setColour(juce::TextButton::textColourOffId,         C(Palette::textLight));
    setColour(juce::TextEditor::backgroundColourId,      C(0xff0e3236));
    setColour(juce::TextEditor::textColourId,            C(Palette::textLight));
    setColour(juce::TextEditor::outlineColourId,         C(Palette::sectionLine));
    setColour(juce::GroupComponent::textColourId,        C(Palette::textLight));
    setColour(juce::GroupComponent::outlineColourId,     C(Palette::sectionLine));
}

juce::Font MS2000LookAndFeel::getLabelFont(juce::Label&) {
    return juce::Font(13.0f, juce::Font::plain);
}
juce::Font MS2000LookAndFeel::getComboBoxFont(juce::ComboBox&) {
    return juce::Font(13.0f, juce::Font::plain);
}

void MS2000LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                         float pos, float startAngle, float endAngle,
                                         juce::Slider& s) {
    const auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(4.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = (bool)s.getProperties().getWithDefault("bipolar", false);

    // track arc
    juce::Path track;
    track.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour(C(Palette::accentDim));
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    // value arc (from centre detent if bipolar, else from start)
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path val;
    val.addCentredArc(cx, cy, radius, radius, 0.0f, juce::jmin(from, angle),
                      juce::jmax(from, angle), true);
    g.setColour(C(Palette::accent));
    g.strokePath(val, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
    // knob body
    const float kr = radius - 6.0f;
    juce::ColourGradient grad(C(0xff34383c), cx - kr * 0.5f, cy - kr * 0.5f,
                              C(Palette::knobBody), cx + kr, cy + kr, true);
    g.setGradientFill(grad);
    g.fillEllipse(cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
    g.setColour(C(Palette::knobEdge));
    g.drawEllipse(cx - kr, cy - kr, kr * 2.0f, kr * 2.0f, 1.5f);
    // pointer
    juce::Path ptr;
    const float pw = 2.4f;
    ptr.addRoundedRectangle(-pw * 0.5f, -kr + 2.0f, pw, kr * 0.62f, 1.0f);
    g.setColour(C(Palette::pointer));
    g.fillPath(ptr, juce::AffineTransform::rotation(angle).translated(cx, cy));
}

void MS2000LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                                         bool highlighted, bool) {
    auto r = b.getLocalBounds().toFloat();
    const float d = juce::jmin(r.getWidth(), r.getHeight()) - 4.0f;
    juce::Rectangle<float> led(0, 0, d, d);
    led.setCentre(r.getCentre());
    const bool on = b.getToggleState();
    g.setColour(on ? C(Palette::ledOn) : C(Palette::ledOff));
    g.fillEllipse(led);
    if (on) { // glow
        g.setColour(C(Palette::ledOn).withAlpha(0.35f));
        g.fillEllipse(led.expanded(3.0f));
    }
    g.setColour(C(Palette::knobEdge));
    g.drawEllipse(led, 1.0f);
    if (highlighted) { g.setColour(C(Palette::textLight).withAlpha(0.25f)); g.drawEllipse(led.expanded(1.5f), 1.0f); }
}

void MS2000LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                     int, int, int, int, juce::ComboBox& box) {
    auto r = juce::Rectangle<int>(0, 0, w, h).toFloat().reduced(0.5f);
    g.setColour(findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(r, 3.0f, 1.0f);
    // arrow
    juce::Path p;
    const float ax = (float)w - 12.0f, ay = (float)h * 0.5f;
    p.addTriangle(ax - 3, ay - 2, ax + 3, ay - 2, ax, ay + 3);
    g.setColour(findColour(juce::ComboBox::arrowColourId).withAlpha(box.isEnabled() ? 1.0f : 0.3f));
    g.fillPath(p);
}

} // namespace ms2000
