#include "SpacifyKnob.h"

namespace spacify::ui
{

namespace
{
    constexpr float nameTracking = 0.16f;
    constexpr float referenceKnobHeight = 180.0f;
}

SpacifyKnob::SpacifyKnob (juce::AudioProcessorValueTreeState& state,
                          const juce::String& parameterID,
                          const juce::String& displayName,
                          Formatter valueFormatter,
                          float nameFontHeightToUse,
                          float valueFontHeightToUse,
                          int spacingToUse)
    : labelText (displayName),
      attachment (state, parameterID, slider),
      format (std::move (valueFormatter)),
      baseNameFontHeight (nameFontHeightToUse),
      baseValueFontHeight (valueFontHeightToUse),
      currentNameFontHeight (nameFontHeightToUse),
      currentValueFontHeight (valueFontHeightToUse),
      spacing (spacingToUse)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (160);
    slider.setVelocityBasedMode (false);
    slider.setScrollWheelEnabled (true);
    slider.setWantsKeyboardFocus (false);
    slider.setName (displayName);

    if (auto* parameter = state.getParameter (parameterID))
        slider.setDoubleClickReturnValue (true,
                                         (double) parameter->convertFrom0to1 (parameter->getDefaultValue()));

    slider.onValueChange = [this] { refreshValueText(); };

    addAndMakeVisible (slider);

    refreshValueText();
}

void SpacifyKnob::paint (juce::Graphics& g)
{
    g.setColour (palette::textSecondary);
    drawTrackedText (g, labelText, nameArea, juce::Justification::centred,
                     makeFont (currentNameFontHeight, "Semibold", nameTracking), nameTracking);

    g.setColour (palette::textPrimary);
    drawTrackedText (g, valueText, valueArea, juce::Justification::centred,
                     makeFont (currentValueFontHeight, "Semibold", 0.0f), 0.0f);
}

void SpacifyKnob::resized()
{
    auto area = getLocalBounds();

    const float scale = juce::jlimit (0.55f, 2.5f,
                                      juce::jmin ((float) area.getHeight() / referenceKnobHeight,
                                                  (float) area.getWidth() / 160.0f));

    currentNameFontHeight  = baseNameFontHeight * scale;
    currentValueFontHeight = baseValueFontHeight * scale;

    nameArea  = area.removeFromTop (juce::roundToInt (currentNameFontHeight * 1.65f));
    valueArea = area.removeFromBottom (juce::roundToInt (currentValueFontHeight * 1.65f));

    const int padX = juce::roundToInt ((float) spacing * scale);
    const int padY = juce::jmax (2, juce::roundToInt (3.0f * scale));
    area.reduce (padX, padY);

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
    const auto nextText = format != nullptr ? format ((float) slider.getValue())
                                            : juce::String (slider.getValue(), 2);

    if (valueText != nextText)
    {
        valueText = nextText;
        repaint (valueArea);
    }
}

} // namespace spacify::ui
