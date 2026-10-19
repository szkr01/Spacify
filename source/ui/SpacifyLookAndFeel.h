#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spacify::ui
{

/** The single place where Spacify's colours live. */
namespace palette
{
inline const juce::Colour backgroundTop    { 0xff161522 };
inline const juce::Colour backgroundBottom { 0xff0a0a11 };
inline const juce::Colour accentBlue       { 0xff7b9dff };
inline const juce::Colour accentViolet     { 0xffb17cff };
inline const juce::Colour textPrimary      { 0xffecedf6 };
inline const juce::Colour textDim          { 0xff74758a };
inline const juce::Colour knobTop          { 0xff4e4d73 };
inline const juce::Colour knobBottom       { 0xff1b1a26 };
inline const juce::Colour track            { 0x1affffff };
inline const juce::Colour hairline         { 0x1cffffff };
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

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
};

} // namespace spacify::ui
