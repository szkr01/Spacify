#include "PluginEditor.h"

namespace spacify
{

namespace
{
    constexpr int baseOuterMargin  = 16;
    constexpr int baseHeaderHeight = 46;
    constexpr float baseCardRadius = 14.0f;

    juce::String percentFormatter (float value)   { return SpacifyAudioProcessor::formatPercent (value); }
    juce::String frequencyFormatter (float value) { return SpacifyAudioProcessor::formatFrequency (value); }
    juce::String durationFormatter (float value)  { return SpacifyAudioProcessor::formatDuration (value); }
}

//==============================================================================
SpacifyAudioProcessorEditor::SpacifyAudioProcessorEditor (SpacifyAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (&processorToEdit),
      processorRef (processorToEdit),
      spaceKnob (processorToEdit.apvts, SpacifyAudioProcessor::spaceParameterID, "SPACE",
                 percentFormatter, 13.5f, 16.5f, 6),
      fuzzKnob (processorToEdit.apvts, SpacifyAudioProcessor::fuzzParameterID, "FUZZ",
                percentFormatter, 13.5f, 16.5f, 6),
      toneKnob (processorToEdit.apvts, SpacifyAudioProcessor::toneParameterID, "TONE",
                frequencyFormatter, 13.5f, 16.5f, 6),
      decayKnob (processorToEdit.apvts, SpacifyAudioProcessor::decayParameterID, "DECAY",
                 durationFormatter, 13.5f, 16.5f, 6)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (spaceKnob);
    addAndMakeVisible (fuzzKnob);
    addAndMakeVisible (toneKnob);
    addAndMakeVisible (decayKnob);

    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (460, 184, 1440, 576);
    getConstrainer()->setFixedAspectRatio ((double) editorWidth / (double) editorHeight);
    setSize (editorWidth, editorHeight);
}

SpacifyAudioProcessorEditor::~SpacifyAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void SpacifyAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float scale = juce::jlimit (0.6f, 2.5f,
                                      juce::jmin (bounds.getWidth() / (float) editorWidth,
                                                  bounds.getHeight() / (float) editorHeight));
    const float cardRadius = baseCardRadius * scale;

    // Warm alabaster outer canvas
    g.setGradientFill (juce::ColourGradient (ui::palette::canvasTop, 0.0f, 0.0f,
                                             ui::palette::canvasBottom, 0.0f, bounds.getBottom(),
                                             false));
    g.fillRect (bounds);

    const auto card = cardBounds.toFloat();

    // Subtle elevation shadow beneath card
    g.setColour (juce::Colour (0x0c0f172a));
    g.fillRoundedRectangle (card.translated (0.0f, 2.5f * scale), cardRadius);

    // Pure white card surface
    g.setColour (ui::palette::surface);
    g.fillRoundedRectangle (card, cardRadius);

    // Subtle tint for the primary SPACE section on the left
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path cardClip;
        cardClip.addRoundedRectangle (card, cardRadius);
        g.reduceClipRegion (cardClip);

        g.setColour (ui::palette::surfaceSubtle);
        g.fillRect (juce::Rectangle<int> (cardBounds.getX(), headerBounds.getBottom(),
                                          dividerX - cardBounds.getX(),
                                          cardBounds.getBottom() - headerBounds.getBottom()));
    }

    // Header separator & vertical section divider
    const int dividerPad = juce::roundToInt (16.0f * scale);
    g.setColour (ui::palette::divider);
    g.fillRect (cardBounds.getX() + 1, headerBounds.getBottom(), cardBounds.getWidth() - 2, 1);
    g.fillRect (dividerX, headerBounds.getBottom() + dividerPad, 1,
                juce::jmax (1, cardBounds.getBottom() - headerBounds.getBottom() - dividerPad * 2));

    // Crisp 1px card border
    g.setColour (ui::palette::border);
    g.drawRoundedRectangle (card.reduced (0.5f), cardRadius, 1.0f);

    // Brand mark: cobalt status dot + tracked SPACIFY title
    const float dotSize = 7.5f * scale;
    const float dotX = (float) headerBounds.getX() + 20.0f * scale;
    const float dotY = (float) headerBounds.getCentreY() - dotSize * 0.5f;

    juce::ColourGradient dotGradient (ui::palette::accentPrimary, dotX, dotY,
                                      ui::palette::accentSecondary, dotX + dotSize, dotY + dotSize, false);
    g.setGradientFill (dotGradient);
    g.fillEllipse (dotX, dotY, dotSize, dotSize);

    auto titleRect = headerBounds.withTrimmedLeft (juce::roundToInt (36.0f * scale));
    g.setColour (ui::palette::textPrimary);
    ui::drawTrackedText (g, "SPACIFY", titleRect, juce::Justification::centredLeft,
                         ui::makeFont (14.5f * scale, "Bold", 0.24f), 0.24f);
}

void SpacifyAudioProcessorEditor::resized()
{
    const float scale = juce::jlimit (0.6f, 2.5f,
                                      juce::jmin ((float) getWidth() / (float) editorWidth,
                                                  (float) getHeight() / (float) editorHeight));

    const int outerMargin  = juce::roundToInt ((float) baseOuterMargin * scale);
    const int headerHeight = juce::roundToInt ((float) baseHeaderHeight * scale);

    cardBounds   = getLocalBounds().reduced (outerMargin);
    headerBounds = cardBounds.withHeight (headerHeight);

    const auto bodyArea = cardBounds.withTrimmedTop (headerHeight + 1);
    const int colWidth  = bodyArea.getWidth() / 4;

    dividerX = bodyArea.getX() + colWidth;

    const int topPad     = juce::roundToInt (12.0f * scale);
    const int bottomPad  = juce::roundToInt (10.0f * scale);
    const int knobTop    = bodyArea.getY() + topPad;
    const int knobHeight = juce::jmax (60, bodyArea.getHeight() - topPad - bottomPad);

    spaceKnob.setBounds (bodyArea.getX() + colWidth * 0, knobTop, colWidth, knobHeight);
    fuzzKnob.setBounds  (bodyArea.getX() + colWidth * 1, knobTop, colWidth, knobHeight);
    toneKnob.setBounds  (bodyArea.getX() + colWidth * 2, knobTop, colWidth, knobHeight);
    decayKnob.setBounds (bodyArea.getX() + colWidth * 3, knobTop, colWidth, knobHeight);
}

} // namespace spacify
