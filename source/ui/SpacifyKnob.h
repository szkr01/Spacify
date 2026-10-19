#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "SpacifyLookAndFeel.h"

namespace spacify::ui
{

/**
    A rotary control with its name above and its current value below.

    Wraps juce::Slider so that dragging, the mouse wheel and double-click-to-
    reset all come for free, and attaches to an APVTS parameter so automation
    and preset recall stay in sync.
*/
class SpacifyKnob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String (float)>;

    SpacifyKnob (juce::AudioProcessorValueTreeState& state,
                 const juce::String& parameterID,
                 const juce::String& displayName,
                 Formatter valueFormatter,
                 float nameFontHeight,
                 float valueFontHeight,
                 int spacing);

    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    juce::Slider& getSlider() noexcept { return slider; }

private:
    void refreshValueText();

    juce::Slider slider;
    juce::Label nameLabel;
    juce::Label valueLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter format;

    float nameFontHeight = 12.0f;
    float valueFontHeight = 14.0f;
    int spacing = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpacifyKnob)
};

} // namespace spacify::ui
