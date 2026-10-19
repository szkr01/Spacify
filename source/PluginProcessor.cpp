#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace spacify
{

namespace
{
    juce::NormalisableRange<float> createToneRange()
    {
        juce::NormalisableRange<float> range { 100.0f, 4000.0f };
        range.setSkewForCentre (650.0f);
        return range;
    }

    juce::NormalisableRange<float> createDecayRange()
    {
        juce::NormalisableRange<float> range { 0.01f, 20.0f };
        range.setSkewForCentre (3.0f);
        return range;
    }
}

//==============================================================================
juce::NormalisableRange<float> SpacifyAudioProcessor::makeToneRange()  { return createToneRange(); }
juce::NormalisableRange<float> SpacifyAudioProcessor::makeDecayRange() { return createDecayRange(); }

juce::String SpacifyAudioProcessor::formatPercent (float value)
{
    return juce::String (juce::roundToInt (value)) + " %";
}

juce::String SpacifyAudioProcessor::formatFrequency (float value)
{
    if (value >= 1000.0f)
        return juce::String (value / 1000.0f, 2) + " kHz";

    return juce::String (juce::roundToInt (value)) + " Hz";
}

juce::String SpacifyAudioProcessor::formatDuration (float value)
{
    if (value < 2.0f)
        return juce::String (value, 2) + " s";

    return juce::String (value, 1) + " s";
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SpacifyAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    auto percentAttributes = [] (const juce::String& label)
    {
        return juce::AudioParameterFloatAttributes()
                   .withLabel (label)
                   .withStringFromValueFunction ([] (float value, int)
                                                 { return formatPercent (value); });
    };

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { spaceParameterID, 1 },
        "Space",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
        spaceDefault,
        percentAttributes ("%")));

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { fuzzParameterID, 1 },
        "Fuzz",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
        fuzzDefault,
        percentAttributes ("%")));

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { toneParameterID, 1 },
        "Tone",
        createToneRange(),
        toneDefault,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float value, int)
                                          { return formatFrequency (value); })));

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { decayParameterID, 1 },
        "Decay",
        createDecayRange(),
        decayDefault,
        juce::AudioParameterFloatAttributes()
            .withLabel ("s")
            .withStringFromValueFunction ([] (float value, int)
                                          { return formatDuration (value); })));

    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { bypassParameterID, 1 },
        "Bypass",
        false,
        juce::AudioParameterBoolAttributes().withLabel ("Bypass")));

    return { parameters.begin(), parameters.end() };
}

//==============================================================================
SpacifyAudioProcessor::SpacifyAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SpacifyState", createParameterLayout())
{
    spaceParameter  = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (spaceParameterID));
    fuzzParameter   = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (fuzzParameterID));
    toneParameter   = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (toneParameterID));
    decayParameter  = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (decayParameterID));
    bypassParameter = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter (bypassParameterID));

    jassert (spaceParameter != nullptr && fuzzParameter != nullptr
             && toneParameter != nullptr && decayParameter != nullptr
             && bypassParameter != nullptr);

    // The pitch shifter has no lookahead and the reverb is a pure feedback
    // network, so the effect introduces no latency that the host would need to
    // compensate for.
    setLatencySamples (0);
}

SpacifyAudioProcessor::~SpacifyAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorParameter* SpacifyAudioProcessor::getBypassParameter() const
{
    return bypassParameter;
}

bool SpacifyAudioProcessor::isBypassedByParameter() const noexcept
{
    return bypassParameter != nullptr && bypassParameter->get();
}

//==============================================================================
void SpacifyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = juce::jmax (subBlockSize, samplesPerBlock);

    wetBuffer.setSize (numWetChannels, maxBlockSize, false, true, true);
    wetBuffer.clear();

    pitchShifter.prepare (sampleRate, numWetChannels);
    toneFilter.prepare (sampleRate, numWetChannels);
    reverb.prepare (sampleRate, numWetChannels);

    spaceSmoother.reset (spaceParameter->get() * 0.01f);
    fuzzSmoother.reset (fuzzParameter->get() * 0.01f);
    toneSmoother.reset (toneParameter->get());
    decaySmoother.reset (decayParameter->get());

    appliedTone = -1.0f;
    appliedDecay = -1.0f;
    appliedFuzz = -1.0f;

    fuzz.setAmount (fuzzSmoother.getCurrent());
    toneFilter.setCutoff (toneSmoother.getCurrent());
    reverb.setDecaySeconds (decaySmoother.getCurrent());

    resetWetChain();
}

void SpacifyAudioProcessor::releaseResources()
{
    wetBuffer.setSize (0, 0);
}

bool SpacifyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainInput  = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput.isDisabled() || mainOutput.isDisabled())
        return false;

    if (mainInput != mainOutput)
        return false;

    return mainInput == juce::AudioChannelSet::mono()
        || mainInput == juce::AudioChannelSet::stereo();
}

void SpacifyAudioProcessor::resetWetChain()
{
    pitchShifter.reset();
    toneFilter.reset();
    reverb.reset();
    wetBuffer.clear();
    wetChainIdle = true;
}

//==============================================================================
void SpacifyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numInputs  = getTotalNumInputChannels();
    const int numOutputs = getTotalNumOutputChannels();

    for (int channel = numInputs; channel < numOutputs; ++channel)
        buffer.clear (channel, 0, numSamples);

    if (numInputs <= 0 || numOutputs <= 0 || numSamples <= 0)
        return;

    // Defensive: if the host calls processBlock without a matching prepareToPlay
    // there is nowhere to build the layer, so fall through to silence rather than
    // writing out of bounds.
    if (wetBuffer.getNumChannels() < numWetChannels || wetBuffer.getNumSamples() < maxBlockSize)
        return;

    if (isBypassedByParameter())
    {
        if (! wetChainIdle)
            resetWetChain();

        return;
    }

    spaceSmoother.setTarget (spaceParameter->get() * 0.01f);
    fuzzSmoother.setTarget  (fuzzParameter->get() * 0.01f);
    toneSmoother.setTarget  (toneParameter->get());
    decaySmoother.setTarget (decayParameter->get());

    // With SPACE at zero the output is bit-identical to the input, so the whole
    // wet path can be skipped rather than computed and thrown away.
    if (spaceSmoother.getTarget() <= 0.0f && ! spaceSmoother.isSmoothing())
    {
        if (! wetChainIdle)
            resetWetChain();

        return;
    }

    wetChainIdle = false;

    int position = 0;

    while (position < numSamples)
    {
        const int blockLength = juce::jmin (maxBlockSize, numSamples - position);
        processChunk (buffer, position, blockLength);
        position += blockLength;
    }
}

void SpacifyAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    const int numSamples = buffer.getNumSamples();

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    if (! wetChainIdle)
        resetWetChain();
}

//==============================================================================
void SpacifyAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const int numInputs  = juce::jmin (numWetChannels, getTotalNumInputChannels());
    const int numOutputs = juce::jmin (numWetChannels, getTotalNumOutputChannels());

    int offset = 0;

    while (offset < numSamples)
    {
        const int subLength = juce::jmin (subBlockSize, numSamples - offset);
        const int subStart = startSample + offset;

        const float spaceValue = spaceSmoother.advance (subLength, currentSampleRate, smoothingSeconds);
        const float fuzzValue  = fuzzSmoother.advance  (subLength, currentSampleRate, smoothingSeconds);
        const float toneValue  = toneSmoother.advance  (subLength, currentSampleRate, smoothingSeconds);
        const float decayValue = decaySmoother.advance (subLength, currentSampleRate, smoothingSeconds);

        if (std::abs (fuzzValue - appliedFuzz) > 5.0e-4f)
        {
            fuzz.setAmount (fuzzValue);
            appliedFuzz = fuzzValue;
        }

        if (std::abs (toneValue - appliedTone) > 0.5f)
        {
            toneFilter.setCutoff (toneValue);
            appliedTone = toneValue;
        }

        if (std::abs (decayValue - appliedDecay) > 5.0e-4f)
        {
            reverb.setDecaySeconds (decayValue);
            appliedDecay = decayValue;
        }

        // Build the wet scratch buffer from the (possibly mono) input.
        for (int channel = 0; channel < numWetChannels; ++channel)
            wetBuffer.copyFrom (channel, offset, buffer, juce::jmin (channel, numInputs - 1),
                                subStart, subLength);

        auto wetView = juce::AudioBuffer<float> (wetBuffer.getArrayOfWritePointers(),
                                                 numWetChannels, offset, subLength);

        pitchShifter.process (wetView, 0, subLength);

        for (int channel = 0; channel < numWetChannels; ++channel)
        {
            float* samples = wetView.getWritePointer (channel);

            for (int i = 0; i < subLength; ++i)
            {
                float value = samples[i];
                value = fuzz.processSample (value);
                toneFilter.processSample (channel, value);
                samples[i] = value;
            }
        }

        reverb.process (wetView, 0, subLength);

        const bool addLayer = spaceValue > 0.0f;

        if (addLayer)
        {
            const float gain = spaceValue * wetTrim;

            for (int channel = 0; channel < numOutputs; ++channel)
            {
                const float* wet = wetView.getReadPointer (juce::jmin (channel, numWetChannels - 1));
                float* output = buffer.getWritePointer (channel);

                for (int i = 0; i < subLength; ++i)
                    output[subStart + i] += wet[i] * gain;
            }
        }

        offset += subLength;
    }
}

//==============================================================================
void SpacifyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("spacifyVersion", versionString, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SpacifyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* SpacifyAudioProcessor::createEditor()
{
    return new SpacifyAudioProcessorEditor (*this);
}

} // namespace spacify

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new spacify::SpacifyAudioProcessor();
}
