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
                 float nameFontHeight = 13.5f,
                 float valueFontHeight = 16.5f,
                 int spacing = 4);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    juce::Slider& getSlider() noexcept { return slider; }

private:
    void refreshValueText();

    juce::Slider slider;
    juce::String labelText;
    juce::String valueText;
    juce::Rectangle<int> nameArea;
    juce::Rectangle<int> valueArea;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    Formatter format;

    float baseNameFontHeight = 13.5f;
    float baseValueFontHeight = 16.5f;
    float currentNameFontHeight = 13.5f;
    float currentValueFontHeight = 16.5f;
    int spacing = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpacifyKnob)
};

} // namespace spacify::ui
