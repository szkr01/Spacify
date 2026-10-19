#include "PluginEditor.h"

namespace spacify
{

namespace
{
    constexpr int layoutMargin        = 40;
    constexpr int bigKnobSize         = 190;
    constexpr int smallKnobSize       = 96;
    constexpr int smallKnobSpacing    = 128;
    constexpr int smallKnobRowTop     = 362;
    constexpr int bypassRowTop        = 516;
    constexpr int bypassRowHeight     = 30;

    juce::String percentFormatter (float value)  { return SpacifyAudioProcessor::formatPercent (value); }
    juce::String frequencyFormatter (float value){ return SpacifyAudioProcessor::formatFrequency (value); }
    juce::String durationFormatter (float value) { return SpacifyAudioProcessor::formatDuration (value); }
}

//==============================================================================
SpacifyAudioProcessorEditor::SpacifyAudioProcessorEditor (SpacifyAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (&processorToEdit),
      processorRef (processorToEdit),
      spaceKnob (processorToEdit.apvts, SpacifyAudioProcessor::spaceParameterID, "SPACE",
                 percentFormatter, 13.0f, 26.0f, 0),
      fuzzKnob (processorToEdit.apvts, SpacifyAudioProcessor::fuzzParameterID, "FUZZ",
                percentFormatter, 11.0f, 13.0f, 0),
      toneKnob (processorToEdit.apvts, SpacifyAudioProcessor::toneParameterID, "TONE",
                frequencyFormatter, 11.0f, 13.0f, 0),
      decayKnob (processorToEdit.apvts, SpacifyAudioProcessor::decayParameterID, "DECAY",
                 durationFormatter, 11.0f, 13.0f, 0),
      bypassAttachment (processorToEdit.apvts, SpacifyAudioProcessor::bypassParameterID, bypassButton)
{
    setLookAndFeel (&lookAndFeel);

    bypassButton.setButtonText ("Bypass");
    bypassButton.setWantsKeyboardFocus (false);
    bypassButton.onStateChange = [this] { updateBypassVisualState(); };

    addAndMakeVisible (spaceKnob);
    addAndMakeVisible (fuzzKnob);
    addAndMakeVisible (toneKnob);
    addAndMakeVisible (decayKnob);
    addAndMakeVisible (bypassButton);

    setOpaque (true);
    setResizable (false, false);
    setSize (editorWidth, editorHeight);

    updateBypassVisualState();
}

SpacifyAudioProcessorEditor::~SpacifyAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void SpacifyAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (ui::palette::backgroundTop, 0.0f, 0.0f,
                                             ui::palette::backgroundBottom, 0.0f, bounds.getBottom(),
                                             false));
    g.fillRect (bounds);

    // Soft aura behind the main control.
    const auto glowCentre = juce::Point<float> (bounds.getCentreX(), (float) titleArea.getBottom() + 110.0f);
    const float glowRadius = bounds.getWidth() * 0.42f;

    juce::ColourGradient glow (ui::palette::accentViolet.withAlpha (bypassed ? 0.04f : 0.13f),
                               glowCentre.x, glowCentre.y,
                               juce::Colours::transparentBlack,
                               glowCentre.x, glowCentre.y + glowRadius, true);
    g.setGradientFill (glow);
    g.fillEllipse (juce::Rectangle<float> (glowRadius * 2.0f, glowRadius * 2.0f).withCentre (glowCentre));

    juce::ColourGradient hairline;
    hairline.addColour (0.0, juce::Colours::transparentBlack);
    hairline.addColour (0.5, ui::palette::hairline);
    hairline.addColour (1.0, juce::Colours::transparentBlack);
    hairline.point1 = { (float) layoutMargin, 0.0f };
    hairline.point2 = { (float) (getWidth() - layoutMargin), 0.0f };

    g.setGradientFill (hairline);
    g.fillRect (layoutMargin, hairlineY, getWidth() - layoutMargin * 2, 1);

    const float titleAlpha = bypassed ? 0.45f : 1.0f;

    g.setColour (ui::palette::textPrimary.withAlpha (0.86f * titleAlpha));
    ui::drawTrackedText (g, "SPACIFY", titleArea, juce::Justification::centred,
                         ui::makeFont (14.0f, {}, 0.34f), 0.34f);

    g.setColour (ui::palette::textDim.withAlpha (0.75f * titleAlpha));
    ui::drawTrackedText (g, juce::String::fromUTF8 ("OCTAVE UP  \xc2\xb7  WET 100 %"), footerArea,
                         juce::Justification::centredLeft,
                         ui::makeFont (10.0f, {}, 0.16f), 0.16f);
}

void SpacifyAudioProcessorEditor::resized()
{
    titleArea = { 0, 30, getWidth(), 22 };

    const int centreX = getWidth() / 2;

    const int bigKnobHeight = juce::roundToInt (13.0f * 1.6f) + bigKnobSize + juce::roundToInt (26.0f * 1.6f);
    spaceKnob.setBounds (centreX - bigKnobSize / 2, titleArea.getBottom() + 30,
                         bigKnobSize, bigKnobHeight);

    const int smallKnobHeight = juce::roundToInt (11.0f * 1.6f) + smallKnobSize + juce::roundToInt (13.0f * 1.6f);

    fuzzKnob.setBounds  (centreX - smallKnobSpacing - smallKnobSize / 2, smallKnobRowTop,
                         smallKnobSize, smallKnobHeight);
    toneKnob.setBounds  (centreX - smallKnobSize / 2, smallKnobRowTop,
                         smallKnobSize, smallKnobHeight);
    decayKnob.setBounds (centreX + smallKnobSpacing - smallKnobSize / 2, smallKnobRowTop,
                         smallKnobSize, smallKnobHeight);

    bypassButton.setBounds (getWidth() - layoutMargin - 160, bypassRowTop, 160, bypassRowHeight);
    footerArea = { layoutMargin, bypassRowTop, 400, bypassRowHeight };
}

void SpacifyAudioProcessorEditor::updateBypassVisualState()
{
    const bool nowBypassed = bypassButton.getToggleState();

    bypassed = nowBypassed;

    const float alpha = bypassed ? 0.35f : 1.0f;

    spaceKnob.setAlpha (alpha);
    fuzzKnob.setAlpha (alpha);
    toneKnob.setAlpha (alpha);
    decayKnob.setAlpha (alpha);

    repaint();
}

} // namespace spacify
