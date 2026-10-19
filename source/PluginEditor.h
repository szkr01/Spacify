#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/SpacifyKnob.h"
#include "ui/SpacifyLookAndFeel.h"

namespace spacify
{

class SpacifyAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SpacifyAudioProcessorEditor (SpacifyAudioProcessor&);
    ~SpacifyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int editorWidth  = 720;
    static constexpr int editorHeight = 288;

private:
    SpacifyAudioProcessor& processorRef;

    ui::SpacifyLookAndFeel lookAndFeel;

    ui::SpacifyKnob spaceKnob;
    ui::SpacifyKnob fuzzKnob;
    ui::SpacifyKnob toneKnob;
    ui::SpacifyKnob decayKnob;

    juce::Rectangle<int> cardBounds;
    juce::Rectangle<int> headerBounds;
    int dividerX = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpacifyAudioProcessorEditor)
};

} // namespace spacify
