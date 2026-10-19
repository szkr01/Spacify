#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginInfo.h"
#include "dsp/Fuzz.h"
#include "dsp/OctaveUpPitchShifter.h"
#include "dsp/SmoothedParam.h"
#include "dsp/SpaceReverb.h"
#include "dsp/ToneHighPass.h"

namespace spacify
{

class SpacifyAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr const char* spaceParameterID  = "space";
    static constexpr const char* fuzzParameterID   = "fuzz";
    static constexpr const char* toneParameterID   = "tone";
    static constexpr const char* decayParameterID  = "decay";
    static constexpr const char* bypassParameterID = "bypass";

    static constexpr float spaceDefault = 35.0f;
    static constexpr float fuzzDefault  = 60.0f;
    static constexpr float toneDefault  = 500.0f;
    static constexpr float decayDefault = 8.0f;

    //==========================================================================
    SpacifyAudioProcessor();
    ~SpacifyAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return pluginName; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 21.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==========================================================================
    static juce::NormalisableRange<float> makeToneRange();
    static juce::NormalisableRange<float> makeDecayRange();

    static juce::String formatPercent (float value);
    static juce::String formatFrequency (float value);
    static juce::String formatDuration (float value);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void processChunk (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void resetWetChain();
    bool isBypassedByParameter() const noexcept;

    static constexpr int numWetChannels = 2;
    static constexpr int subBlockSize = 32;
    static constexpr double smoothingSeconds = 0.02;

    // wet layer trim, chosen so that SPACE = 100 % produces a conspicuous but
    // not deafening layer next to the untouched dry signal
    static constexpr float wetTrim = 0.85f;

    juce::AudioParameterFloat* spaceParameter  = nullptr;
    juce::AudioParameterFloat* fuzzParameter   = nullptr;
    juce::AudioParameterFloat* toneParameter   = nullptr;
    juce::AudioParameterFloat* decayParameter  = nullptr;
    juce::AudioParameterBool*  bypassParameter = nullptr;

    OctaveUpPitchShifter pitchShifter;
    Fuzz fuzz;
    ToneHighPass toneFilter;
    SpaceReverb reverb;

    juce::AudioBuffer<float> wetBuffer;
    int maxBlockSize = 512;
    double currentSampleRate = 48000.0;

    SmoothedParam spaceSmoother, fuzzSmoother, toneSmoother, decaySmoother;

    float appliedTone = -1.0f;
    float appliedDecay = -1.0f;
    float appliedFuzz = -1.0f;

    bool wetChainIdle = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpacifyAudioProcessor)
};

} // namespace spacify
