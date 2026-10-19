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

    static constexpr int editorWidth  = 760;
    static constexpr int editorHeight = 560;

private:
    void updateBypassVisualState();

    SpacifyAudioProcessor& processorRef;

    ui::SpacifyLookAndFeel lookAndFeel;

    ui::SpacifyKnob spaceKnob;
    ui::SpacifyKnob fuzzKnob;
    ui::SpacifyKnob toneKnob;
    ui::SpacifyKnob decayKnob;

    juce::ToggleButton bypassButton;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    juce::Rectangle<int> titleArea;
    juce::Rectangle<int> footerArea;
    int hairlineY = 62;

    bool bypassed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpacifyAudioProcessorEditor)
};

} // namespace spacify
