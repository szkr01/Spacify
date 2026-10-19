#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spacify::ui
{

/** The single place where Spacify's colours live. */
namespace palette
{
inline const juce::Colour canvasTop       { 0xfff7f8fb };
inline const juce::Colour canvasBottom    { 0xffeff1f6 };
inline const juce::Colour surface         { 0xffffffff };
inline const juce::Colour surfaceSubtle   { 0xfffafbfd };
inline const juce::Colour border          { 0xffe2e6ef };
inline const juce::Colour divider         { 0xffeaedf4 };
inline const juce::Colour track           { 0xffe2e6f0 };
inline const juce::Colour accentPrimary   { 0xff2563eb };
inline const juce::Colour accentSecondary { 0xff4f46e5 };
inline const juce::Colour knobFaceTop     { 0xffffffff };
inline const juce::Colour knobFaceBottom  { 0xfff3f5fa };
inline const juce::Colour knobBorder      { 0xffd5dbe7 };
inline const juce::Colour knobShadow      { 0x160f172a };
inline const juce::Colour textPrimary     { 0xff111318 };
inline const juce::Colour textSecondary   { 0xff646c7e };
inline const juce::Colour textMuted       { 0xff949cae };
}

/** Type helpers so every label uses the same voice. */
juce::Font makeFont (float height, const juce::String& style = {}, float tracking = 0.0f);

/** Draws text centred / aligned as normal, but compensating for the trailing
    kerning that juce::Font adds when letter spacing is switched on.
*/
void drawTrackedText (juce::Graphics& g,
                      const juce::String& text,
                      juce::Rectangle<int> area,
                      juce::Justification justification,
                      const juce::Font& font,
                      float tracking);

class SpacifyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SpacifyLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawCornerResizer (juce::Graphics&, int w, int h,
                            bool isMouseOver, bool isMouseDragging) override;
};

} // namespace spacify::ui
