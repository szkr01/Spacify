#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "PluginProcessor.h"

using namespace spacify;

//==============================================================================
namespace
{

int failures = 0;

void check (bool condition, const juce::String& what, const juce::String& detail = {})
{
    juce::String line;
    line << (condition ? "  PASS  " : "  FAIL  ") << what;

    if (detail.isNotEmpty())
        line << "    [" << detail << "]";

    std::cout << line << std::endl;

    if (! condition)
        ++failures;
}

void info (const juce::String& line)
{
    std::cout << "  ..    " << line << std::endl;
}

void heading (const juce::String& line)
{
    std::cout << std::endl << line << std::endl;
}

//==============================================================================
struct Harness
{
    Harness (double sampleRateToUse = 48000.0, int blockSizeToUse = 256)
        : sampleRate (sampleRateToUse), blockSize (blockSizeToUse)
    {
        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
    }

    void set (const juce::String& parameterID, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (parameterID))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
    }

    /** Runs `buffer` through the processor in chunks, in place. */
    void processInChunks (juce::AudioBuffer<float>& buffer)
    {
        juce::MidiBuffer midi;

        for (int position = 0; position < buffer.getNumSamples(); position += blockSize)
        {
            const int length = juce::jmin (blockSize, buffer.getNumSamples() - position);

            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(),
                                           buffer.getNumChannels(), position, length);
            processor.processBlock (view, midi);
        }
    }

    SpacifyAudioProcessor processor;
    double sampleRate;
    int blockSize;
};

juce::AudioBuffer<float> makeNoise (int numChannels, int numSamples, juce::Random& random, float amplitude)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    buffer.clear();

    for (int channel = 0; channel < numChannels; ++channel)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (channel, i, (random.nextFloat() * 2.0f - 1.0f) * amplitude);

    return buffer;
}

double windowedAmplitudeAt (const float* data, int numSamples, double sampleRate, double frequency)
{
    double real = 0.0, imaginary = 0.0, windowSum = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const double window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (double) (numSamples - 1));
        const double phase = juce::MathConstants<double>::twoPi * frequency * (double) i / sampleRate;

        real += (double) data[i] * window * std::cos (phase);
        imaginary -= (double) data[i] * window * std::sin (phase);
        windowSum += window;
    }

    return 2.0 * std::sqrt (real * real + imaginary * imaginary) / windowSum;
}

double rms (const float* data, int numSamples)
{
    double sum = 0.0;

    for (int i = 0; i < numSamples; ++i)
        sum += (double) data[i] * (double) data[i];

    return std::sqrt (sum / juce::jmax (1, numSamples));
}

//==============================================================================
void testBypassIsBitExact()
{
    Harness h;
    h.set (SpacifyAudioProcessor::spaceParameterID, 100.0f);
    h.set (SpacifyAudioProcessor::fuzzParameterID, 100.0f);
    h.set (SpacifyAudioProcessor::decayParameterID, 20.0f);
    h.set (SpacifyAudioProcessor::bypassParameterID, 1.0f);

    juce::Random random (1234);
    auto buffer = makeNoise (2, 8192, random, 0.5f);
    auto expected = buffer;

    h.processInChunks (buffer);

    bool identical = true;

    for (int channel = 0; channel < 2 && identical; ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (buffer.getSample (channel, i) != expected.getSample (channel, i))
                identical = false;

    check (identical, "BYPASS passes the input through bit-exactly");
}

void testZeroSpaceIsBitExact()
{
    Harness h;
    h.set (SpacifyAudioProcessor::bypassParameterID, 0.0f);
    h.set (SpacifyAudioProcessor::fuzzParameterID, 100.0f);
    h.set (SpacifyAudioProcessor::decayParameterID, 20.0f);
    h.set (SpacifyAudioProcessor::spaceParameterID, 100.0f);

    // Let the parameter smoothing settle before checking the steady state.
    {
        juce::Random warmUpRandom (4321);
        auto warmUp = makeNoise (2, 16384, warmUpRandom, 0.3f);
        h.processInChunks (warmUp);
    }

    h.set (SpacifyAudioProcessor::spaceParameterID, 0.0f);
    {
        juce::Random settleRandom (8765);
        auto settle = makeNoise (2, (int) h.sampleRate, settleRandom, 0.3f);
        h.processInChunks (settle);
    }

    juce::Random random (99);
    auto buffer = makeNoise (2, 8192, random, 0.5f);
    auto expected = buffer;

    h.processInChunks (buffer);

    bool identical = true;

    for (int channel = 0; channel < 2 && identical; ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (buffer.getSample (channel, i) != expected.getSample (channel, i))
                identical = false;

    check (identical, "SPACE = 0 % leaves the dry signal bit-exactly untouched");
}

void testLayerScalesLinearly()
{
    const double sampleRate = 48000.0;
    const int blockSize = 256;
    const int warmUpSamples = (int) sampleRate;
    const int windowSamples = (int) (sampleRate * 0.5);
    const int totalSamples = warmUpSamples + windowSamples;

    auto renderLayer = [&] (float spaceAmount)
    {
        Harness h (sampleRate, blockSize);
        h.set (SpacifyAudioProcessor::bypassParameterID, 0.0f);
        h.set (SpacifyAudioProcessor::fuzzParameterID, 60.0f);
        h.set (SpacifyAudioProcessor::toneParameterID, 100.0f);
        h.set (SpacifyAudioProcessor::decayParameterID, 0.5f);
        h.set (SpacifyAudioProcessor::spaceParameterID, spaceAmount);

        juce::AudioBuffer<float> buffer (2, totalSamples);
        buffer.clear();

        for (int i = 0; i < totalSamples; ++i)
        {
            const float value = 0.4f * std::sin (juce::MathConstants<float>::twoPi * 330.0f * (float) i / (float) sampleRate);
            buffer.setSample (0, i, value);
            buffer.setSample (1, i, value);
        }

        auto dry = buffer;
        h.processInChunks (buffer);

        juce::AudioBuffer<float> layer (1, windowSamples);

        for (int i = 0; i < windowSamples; ++i)
            layer.setSample (0, i, buffer.getSample (0, warmUpSamples + i) - dry.getSample (0, warmUpSamples + i));

        return layer;
    };

    const auto full = renderLayer (100.0f);
    const auto half = renderLayer (50.0f);

    juce::AudioBuffer<float> residual (1, windowSamples);
    residual.clear();

    for (int i = 0; i < windowSamples; ++i)
        residual.setSample (0, i, half.getSample (0, i) - full.getSample (0, i) * 0.5f);

    const double fullRms = rms (full.getReadPointer (0), windowSamples);
    const double residualRms = rms (residual.getReadPointer (0), windowSamples);

    check (fullRms > 1.0e-4 && residualRms < fullRms * 0.01,
           "the layer scales linearly with SPACE, so the dry signal is never attenuated",
           "residual is " + juce::String (juce::Decibels::gainToDecibels (residualRms / juce::jmax (1.0e-9, fullRms)), 1)
               + " dB below the layer");
}

//==============================================================================
void testPitchShiftIsOneOctaveUp()
{
    const double sampleRate = 48000.0;
    const int blockSize = 256;
    const int totalSamples = (int) (sampleRate * 2.0);

    Harness h (sampleRate, blockSize);
    h.set (SpacifyAudioProcessor::bypassParameterID, 0.0f);
    h.set (SpacifyAudioProcessor::spaceParameterID, 100.0f);
    h.set (SpacifyAudioProcessor::fuzzParameterID, 60.0f);
    h.set (SpacifyAudioProcessor::toneParameterID, 100.0f);
    h.set (SpacifyAudioProcessor::decayParameterID, 0.5f);

    juce::AudioBuffer<float> buffer (2, totalSamples);
    buffer.clear();

    for (int i = 0; i < totalSamples; ++i)
    {
        const float value = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / (float) sampleRate);
        buffer.setSample (0, i, value);
        buffer.setSample (1, i, value);
    }

    h.processInChunks (buffer);

    // The dry signal is still present, so analyse the last 0.5 s of the input
    // minus the dry part to isolate the generated layer.
    const int analysisStart = totalSamples - (int) (sampleRate * 0.5);
    const int analysisLength = totalSamples - analysisStart;

    juce::AudioBuffer<float> layer (1, analysisLength);

    for (int i = 0; i < analysisLength; ++i)
    {
        const int n = analysisStart + i;
        const float dry = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) n / (float) sampleRate);
        layer.setSample (0, i, buffer.getSample (0, n) - dry);
    }

    const float* data = layer.getReadPointer (0);

    const double at220  = windowedAmplitudeAt (data, analysisLength, sampleRate, 220.0);
    const double at440  = windowedAmplitudeAt (data, analysisLength, sampleRate, 440.0);
    const double at660  = windowedAmplitudeAt (data, analysisLength, sampleRate, 660.0);
    const double at1320 = windowedAmplitudeAt (data, analysisLength, sampleRate, 1320.0);

    info ("wet layer spectrum at 220 Hz  = " + juce::String (at220, 5));
    info ("wet layer spectrum at 440 Hz  = " + juce::String (at440, 5));
    info ("wet layer spectrum at 660 Hz  = " + juce::String (at660, 5));
    info ("wet layer spectrum at 1320 Hz = " + juce::String (at1320, 5));

    check (at440 > 0.01 && at440 > at220 * 10.0,
           "the generated layer is an octave above the input",
           "440 Hz is " + juce::String (at440 / juce::jmax (1.0e-9, at220), 1) + "x the 220 Hz content");

    check (at1320 > at660,
           "fuzz adds strong upper harmonics to the layer",
           "1320 Hz " + juce::String (at1320, 5) + " vs 660 Hz " + juce::String (at660, 5));
}

//==============================================================================
/** Measures the -60 dB decay time of the wet layer using a T30 extrapolation. */
double measureRt60 (float decaySeconds)
{
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    const int burstSamples = (int) (0.05 * sampleRate);
    const int tailSeconds = (int) std::ceil (decaySeconds * 1.6) + 2;
    const int totalSamples = burstSamples + (int) (tailSeconds * sampleRate);

    Harness h (sampleRate, blockSize);
    h.set (SpacifyAudioProcessor::bypassParameterID, 0.0f);
    h.set (SpacifyAudioProcessor::spaceParameterID, 100.0f);
    h.set (SpacifyAudioProcessor::fuzzParameterID, 40.0f);
    h.set (SpacifyAudioProcessor::toneParameterID, 100.0f);
    h.set (SpacifyAudioProcessor::decayParameterID, decaySeconds);

    juce::Random random (4242);

    juce::AudioBuffer<float> input (2, totalSamples);
    input.clear();

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < burstSamples; ++i)
            input.setSample (channel, i, (random.nextFloat() * 2.0f - 1.0f) * 0.6f);

    auto output = input;
    h.processInChunks (output);

    juce::AudioBuffer<float> layer (1, totalSamples);

    for (int i = 0; i < totalSamples; ++i)
        layer.setSample (0, i, output.getSample (0, i) - input.getSample (0, i));

    // RMS envelope in 20 ms windows.
    const int window = (int) (0.02 * sampleRate);
    const int numWindows = totalSamples / window;

    std::vector<double> envelope ((size_t) numWindows, 0.0);

    for (int w = 0; w < numWindows; ++w)
        envelope[(size_t) w] = rms (layer.getReadPointer (0) + w * window, window);

    const auto peak = std::max_element (envelope.begin(), envelope.end());
    const int peakIndex = (int) std::distance (envelope.begin(), peak);

    const double peakDb = juce::Decibels::gainToDecibels (*peak);
    const double t5 = peakDb - 5.0;
    const double t35 = peakDb - 35.0;

    int index5 = -1, index35 = -1;

    for (int w = peakIndex; w < numWindows; ++w)
    {
        const double db = juce::Decibels::gainToDecibels (envelope[(size_t) w]);

        if (index5 < 0 && db <= t5)   index5 = w;
        if (index35 < 0 && db <= t35) { index35 = w; break; }
    }

    if (index5 < 0 || index35 < 0)
        return -1.0;

    const double seconds = (double) (index35 - index5) * 0.02;
    return seconds * 2.0;
}

void testDecayTimes()
{
    struct Target { float decay; double tolerance; };

    const Target targets[] = { { 0.5f, 0.45 }, { 3.0f, 0.45 }, { 8.0f, 0.45 }, { 20.0f, 0.5 } };

    for (const auto& target : targets)
    {
        const double measured = measureRt60 (target.decay);

        if (measured <= 0.0)
        {
            check (false, "RT60 measured for DECAY = " + juce::String (target.decay, 1) + " s",
                   "decay never reached -35 dB");
            continue;
        }

        const double relativeError = std::abs (measured - target.decay) / target.decay;

        check (relativeError <= target.tolerance,
               "DECAY = " + juce::String (target.decay, 1) + " s produces the requested tail",
               "measured " + juce::String (measured, 2) + " s  ("
                   + juce::String (relativeError * 100.0, 1) + " % error)");
    }
}

//==============================================================================
void testWetLevel()
{
    const double sampleRate = 48000.0;
    const int totalSamples = (int) (sampleRate * 2.0);

    Harness h (sampleRate, 256);
    h.set (SpacifyAudioProcessor::bypassParameterID, 0.0f);
    h.set (SpacifyAudioProcessor::spaceParameterID, 100.0f);

    juce::AudioBuffer<float> buffer (2, totalSamples);
    buffer.clear();

    juce::Random random (7);

    for (int i = 0; i < totalSamples; ++i)
    {
        // A piano-ish pluck: a decaying tone with a couple of harmonics.
        const float t = (float) i / (float) sampleRate;
        const float envelope = 0.5f * std::exp (-2.5f * t) + 0.05f;
        const float value = envelope * (0.6f * std::sin (juce::MathConstants<float>::twoPi * 330.0f * t)
                                        + 0.3f * std::sin (juce::MathConstants<float>::twoPi * 660.0f * t)
                                        + 0.1f * (random.nextFloat() * 2.0f - 1.0f));
        buffer.setSample (0, i, value);
        buffer.setSample (1, i, value);
    }

    auto dry = buffer;
    h.processInChunks (buffer);

    juce::AudioBuffer<float> layer (1, totalSamples);

    for (int i = 0; i < totalSamples; ++i)
        layer.setSample (0, i, buffer.getSample (0, i) - dry.getSample (0, i));

    const int start = (int) (sampleRate * 0.3);
    const int length = totalSamples - start;

    const double dryRms = rms (dry.getReadPointer (0) + start, length);
    const double wetRms = rms (layer.getReadPointer (0) + start, length);

    info ("dry RMS  " + juce::String (juce::Decibels::gainToDecibels (dryRms), 1) + " dBFS");
    info ("layer RMS at SPACE = 100 %  " + juce::String (juce::Decibels::gainToDecibels (wetRms), 1) + " dBFS");
    info ("difference  " + juce::String (juce::Decibels::gainToDecibels (wetRms / juce::jmax (1.0e-9, dryRms)), 1) + " dB");

    check (wetRms > 1.0e-4, "the generated layer has a usable level");

    // Verify level consistency across the entire DECAY range (0.01 s .. 20.0 s),
    // specifically checking 0.16 s where the old (1 - feedback) + non-unitary allpass blew up by +30 dB.
    const float testDecays[] = { 0.01f, 0.05f, 0.16f, 0.50f, 2.0f, 8.0f, 20.0f };
    double minDb = 1000.0, maxDb = -1000.0;

    for (float d : testDecays)
    {
        Harness hd (sampleRate, 256);
        hd.set (SpacifyAudioProcessor::bypassParameterID, 0.0f);
        hd.set (SpacifyAudioProcessor::spaceParameterID, 100.0f);
        hd.set (SpacifyAudioProcessor::decayParameterID, d);

        auto buf = dry;
        hd.processInChunks (buf);

        juce::AudioBuffer<float> decLayer (1, totalSamples);
        for (int i = 0; i < totalSamples; ++i)
            decLayer.setSample (0, i, buf.getSample (0, i) - dry.getSample (0, i));

        const double decRms = rms (decLayer.getReadPointer (0) + start, length);
        const double db = juce::Decibels::gainToDecibels (decRms);
        minDb = juce::jmin (minDb, db);
        maxDb = juce::jmax (maxDb, db);

        info ("layer RMS at DECAY = " + juce::String (d, 2) + " s : " + juce::String (db, 1) + " dBFS");
    }

    check ((maxDb - minDb) < 6.0,
           "wet layer loudness stays consistent across all DECAY values (0.01 s .. 20.0 s)",
           "spread " + juce::String (maxDb - minDb, 1) + " dB");
}

void testStability()
{
    {
        // Silence in, silence (and no denormal/NaN garbage) out.
        Harness h;
        h.set (SpacifyAudioProcessor::bypassParameterID, 0.0f);
        h.set (SpacifyAudioProcessor::spaceParameterID, 100.0f);
        h.set (SpacifyAudioProcessor::fuzzParameterID, 100.0f);
        h.set (SpacifyAudioProcessor::decayParameterID, 20.0f);

        juce::AudioBuffer<float> buffer (2, (int) h.sampleRate * 3);
        buffer.clear();
        h.processInChunks (buffer);

        bool silent = true;

        for (int channel = 0; channel < 2 && silent; ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (buffer.getSample (channel, i) != 0.0f)
                    silent = false;

        check (silent, "silence in produces exact silence out (no denormal build-up)");
    }

    {
        // Full-scale input at the most extreme settings must stay finite.
        Harness h;
        h.set (SpacifyAudioProcessor::bypassParameterID, 0.0f);
        h.set (SpacifyAudioProcessor::spaceParameterID, 100.0f);
        h.set (SpacifyAudioProcessor::fuzzParameterID, 100.0f);
        h.set (SpacifyAudioProcessor::toneParameterID, 100.0f);
        h.set (SpacifyAudioProcessor::decayParameterID, 20.0f);

        juce::Random random (31337);
        auto buffer = makeNoise (2, (int) h.sampleRate * 4, random, 1.0f);

        h.processInChunks (buffer);

        bool finite = true;
        float peak = 0.0f;

        for (int channel = 0; channel < 2 && finite; ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float value = buffer.getSample (channel, i);

                if (! std::isfinite (value))
                    finite = false;

                peak = juce::jmax (peak, std::abs (value));
            }

        check (finite && peak < 4.0f, "extreme settings stay finite and bounded",
               "peak " + juce::String (peak, 3));
    }
}

void testMonoAndSampleRates()
{
    {
        Harness h;

        juce::AudioProcessor::BusesLayout monoLayout;
        monoLayout.inputBuses.add (juce::AudioChannelSet::mono());
        monoLayout.outputBuses.add (juce::AudioChannelSet::mono());
        h.processor.setBusesLayout (monoLayout);
        h.processor.prepareToPlay (h.sampleRate, h.blockSize);

        h.set (SpacifyAudioProcessor::bypassParameterID, 0.0f);
        h.set (SpacifyAudioProcessor::spaceParameterID, 100.0f);

        juce::Random random (5);
        auto buffer = makeNoise (1, 8192, random, 0.4f);
        h.processInChunks (buffer);

        bool finite = true;
        float peak = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));

        finite = std::isfinite (peak) && peak > 0.0f;

        check (finite, "mono in / mono out is processed without exploding",
               "peak " + juce::String (peak, 3));
    }

    for (const double sampleRate : { 44100.0, 88200.0, 96000.0 })
    {
        Harness h (sampleRate, 512);
        h.set (SpacifyAudioProcessor::bypassParameterID, 0.0f);
        h.set (SpacifyAudioProcessor::spaceParameterID, 80.0f);

        juce::Random random (11);
        auto buffer = makeNoise (2, 8192, random, 0.5f);
        h.processInChunks (buffer);

        float peak = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));

        check (std::isfinite (peak) && peak > 0.0f,
               "runs at " + juce::String (sampleRate / 1000.0, 1) + " kHz",
               "peak " + juce::String (peak, 3));
    }
}

void testStateRoundTrip()
{
    Harness h;
    h.set (SpacifyAudioProcessor::spaceParameterID, 72.0f);
    h.set (SpacifyAudioProcessor::fuzzParameterID, 13.0f);
    h.set (SpacifyAudioProcessor::toneParameterID, 1234.0f);
    h.set (SpacifyAudioProcessor::decayParameterID, 3.5f);
    h.set (SpacifyAudioProcessor::bypassParameterID, 1.0f);

    juce::MemoryBlock state;
    h.processor.getStateInformation (state);

    Harness other;
    other.processor.setStateInformation (state.getData(), (int) state.getSize());

    const bool ok = std::abs (other.processor.apvts.getParameter (SpacifyAudioProcessor::spaceParameterID)->getValue() * 100.0f - 72.0f) < 0.2f
                 && std::abs (other.processor.apvts.getParameter (SpacifyAudioProcessor::fuzzParameterID)->getValue() * 100.0f - 13.0f) < 0.2f
                 && other.processor.apvts.getParameter (SpacifyAudioProcessor::bypassParameterID)->getValue() > 0.5f;

    check (ok, "state save / restore round-trips every parameter");
}

} // namespace

//==============================================================================
int main()
{
    std::cout << "Spacify DSP verification" << std::endl;
    std::cout << "========================" << std::endl;

    heading ("Null tests");
    testBypassIsBitExact();
    testZeroSpaceIsBitExact();
    testLayerScalesLinearly();

    heading ("Pitch shift and fuzz");
    testPitchShiftIsOneOctaveUp();

    heading ("Reverb decay");
    testDecayTimes();

    heading ("Levels");
    testWetLevel();

    heading ("Stability");
    testStability();

    heading ("Buses and sample rates");
    testMonoAndSampleRates();

    heading ("State");
    testStateRoundTrip();

    std::cout << std::endl
              << (failures == 0 ? "ALL CHECKS PASSED" : (juce::String (failures) + " CHECK(S) FAILED"))
              << std::endl;

    return failures == 0 ? 0 : 1;
}
