#include "SpacifyKnob.h"

namespace spacify::ui
{

namespace
{
    constexpr float nameTracking = 0.22f;
}

SpacifyKnob::SpacifyKnob (juce::AudioProcessorValueTreeState& state,
                          const juce::String& parameterID,
                          const juce::String& displayName,
                          Formatter valueFormatter,
                          float nameFontHeightToUse,
                          float valueFontHeightToUse,
                          int spacingToUse)
    : attachment (state, parameterID, slider),
      format (std::move (valueFormatter)),
      nameFontHeight (nameFontHeightToUse),
      valueFontHeight (valueFontHeightToUse),
      spacing (spacingToUse)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (170);
    slider.setVelocityBasedMode (false);
    slider.setScrollWheelEnabled (true);
    slider.setWantsKeyboardFocus (false);
    slider.setName (displayName);

    if (auto* parameter = state.getParameter (parameterID))
        slider.setDoubleClickReturnValue (true,
                                         (double) parameter->convertFrom0to1 (parameter->getDefaultValue()));

    slider.onValueChange = [this] { refreshValueText(); };

    nameLabel.setText (displayName, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (makeFont (nameFontHeight, {}, nameTracking));
    nameLabel.setColour (juce::Label::textColourId, palette::textDim);
    nameLabel.setInterceptsMouseClicks (false, false);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (makeFont (valueFontHeight, {}, 0.0f));
    valueLabel.setColour (juce::Label::textColourId, palette::textPrimary);
    valueLabel.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (slider);
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (valueLabel);

    refreshValueText();
}

void SpacifyKnob::resized()
{
    auto area = getLocalBounds();

    nameLabel.setBounds (area.removeFromTop (juce::roundToInt (nameFontHeight * 1.6f)));
    valueLabel.setBounds (area.removeFromBottom (juce::roundToInt (valueFontHeight * 1.6f)));

    area.reduce (spacing, 0);

    const int knobSize = juce::jmin (area.getWidth(), area.getHeight());

    slider.setBounds (area.withSizeKeepingCentre (knobSize, knobSize));
}

void SpacifyKnob::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (isMouseOverOrDragging (true))
        slider.mouseWheelMove (event, wheel);
}

void SpacifyKnob::refreshValueText()
{
    const auto text = format != nullptr ? format ((float) slider.getValue())
                                        : juce::String (slider.getValue(), 2);

    valueLabel.setText (text, juce::dontSendNotification);
}

} // namespace spacify::ui
