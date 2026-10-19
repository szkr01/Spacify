#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <vector>

namespace spacify
{

/**
    Fixed one-octave-up pitch shifter.

    Implementation is the classic "two cross-faded, linearly ramping read taps"
    granular shifter:

      - the input is written into a circular delay line,
      - two read taps move through the line at twice the input speed,
      - the tap positions ramp from `grain` samples of delay down to zero and
        then wrap, which is exactly what produces the 2x playback rate,
      - a Hann window that is zero at the wrap point cross-fades the taps so the
        discontinuity is inaudible,
      - the two windows are offset by half a period and sum to unity.

    Because the read tap sweeps continuously there is no lookahead and the
    plug-in reports zero additional latency to the host.

    A gentle amplitude ripple is inherent to this technique (the two taps are at
    slightly different phases of the shifted signal); it is at most ~3 dB and
    runs at 1 / grainSize Hz. Downstream fuzz + reverb completely mask it, and
    the alternative (a phase vocoder) would smear the transients that give this
    effect its character.
*/
class OctaveUpPitchShifter
{
public:
    void prepare (double newSampleRate, int numChannels)
    {
        sampleRate = newSampleRate;
        grainSamples = juce::jmax (32, (int) std::round (grainSeconds * sampleRate));

        // +8 samples of slack so the interpolating read tap never steps behind
        // the write index, and one extra slot so a tap can land exactly on it.
        bufferSize = grainSamples + 8;
        delayLines.assign ((size_t) numChannels, std::vector<float> ((size_t) bufferSize, 0.0f));

        reset();
    }

    void reset()
    {
        for (auto& line : delayLines)
            std::fill (line.begin(), line.end(), 0.0f);

        writeIndex = 0;
        phase = 0.0;
    }

    int getGrainSamples() const noexcept { return grainSamples; }

    /** Replaces the contents of `buffer` with the octave-up version.
        Operates on channels [0, getNumChannels()).
    */
    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
    {
        const int numChannels = (int) delayLines.size();

        if (numChannels == 0 || buffer.getNumChannels() < numChannels)
            return;

        const double phaseIncrement = 1.0 / (double) grainSamples;

        for (int i = 0; i < numSamples; ++i)
        {
            const int sampleIndex = startSample + i;

            for (int ch = 0; ch < numChannels; ++ch)
                delayLines[(size_t) ch][(size_t) writeIndex] = buffer.getReadPointer (ch)[sampleIndex];

            auto wrapPhase = [] (double p) noexcept { return p >= 1.0 ? p - 1.0 : p; };

            const double phaseA = phase;
            const double phaseB = wrapPhase (phase + 0.5);

            const auto windowValue = [] (double p) noexcept
            {
                return 0.5f - 0.5f * std::cos ((float) (juce::MathConstants<double>::twoPi * p));
            };

            const float gainA = windowValue (phaseA);
            const float gainB = windowValue (phaseB);

            // Delay of each tap ramps linearly from `grainSamples` down to zero.
            const float delayA = (float) (grainSamples * (1.0 - phaseA));
            const float delayB = (float) (grainSamples * (1.0 - phaseB));

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* line = delayLines[(size_t) ch].data();

                buffer.getWritePointer (ch)[sampleIndex]
                    = gainA * readInterpolated (line, writeIndex, delayA)
                    + gainB * readInterpolated (line, writeIndex, delayB);
            }

            if (++writeIndex >= bufferSize)
                writeIndex = 0;

            phase += phaseIncrement;

            if (phase >= 1.0)
                phase -= 1.0;
        }
    }

private:
    float readInterpolated (const float* line, int readHead, float delay) const noexcept
    {
        float position = (float) readHead - delay;

        while (position < 0.0f)                 position += (float) bufferSize;
        while (position >= (float) bufferSize)  position -= (float) bufferSize;

        const int index0 = (int) position;
        const float fraction = position - (float) index0;

        int index1 = index0 + 1;
        if (index1 >= bufferSize)
            index1 -= bufferSize;

        return line[index0] + fraction * (line[index1] - line[index0]);
    }

    double sampleRate = 48000.0;
    double grainSeconds = 0.055;
    int grainSamples = 2640;
    int bufferSize = 2648;
    int writeIndex = 0;
    double phase = 0.0;
    std::vector<std::vector<float>> delayLines;
};

} // namespace spacify
