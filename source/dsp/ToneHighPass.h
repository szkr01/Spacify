#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

namespace spacify
{

/**
    Single second order high-pass (12 dB/octave, Butterworth Q) applied to the
    wet path before the reverb.

    Coefficients are recomputed from RBJ's audio EQ cook book rather than going
    through juce::dsp::IIR, so that changing TONE never allocates and is safe to
    do per processing sub-block.
*/
class ToneHighPass
{
public:
    void prepare (double newSampleRate, int numChannels)
    {
        sampleRate = newSampleRate;
        state.assign ((size_t) juce::jmax (1, numChannels), ChannelState {});
        setCutoff (cutoffHz);
    }

    void reset()
    {
        for (auto& s : state)
            s = ChannelState {};
    }

    void setCutoff (float newCutoffHz) noexcept
    {
        cutoffHz = juce::jlimit (20.0f, 20000.0f, newCutoffHz);

        const double nyquist = sampleRate * 0.5;
        const double frequency = juce::jlimit (10.0, nyquist * 0.95, (double) cutoffHz);
        const double omega = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const double cosOmega = std::cos (omega);
        const double sinOmega = std::sin (omega);
        const double alpha = sinOmega / (2.0 * 0.7071067811865476);

        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cosOmega;
        const double a2 = 1.0 - alpha;
        const double b0 = (1.0 + cosOmega) * 0.5;
        const double b1 = -(1.0 + cosOmega);
        const double b2 = (1.0 + cosOmega) * 0.5;

        c0 = (float) (b0 / a0);
        c1 = (float) (b1 / a0);
        c2 = (float) (b2 / a0);
        d1 = (float) (a1 / a0);
        d2 = (float) (a2 / a0);
    }

    float getCutoff() const noexcept { return cutoffHz; }

    void processSample (int channel, float& sample) noexcept
    {
        auto& s = state[(size_t) channel];

        const float output = c0 * sample + c1 * s.x1 + c2 * s.x2 - d1 * s.y1 - d2 * s.y2;

        s.x2 = s.x1;
        s.x1 = sample;
        s.y2 = s.y1;
        s.y1 = output;

        sample = output;
    }

private:
    struct ChannelState
    {
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    };

    double sampleRate = 44100.0;
    float cutoffHz = 500.0f;
    float c0 = 1.0f, c1 = 0.0f, c2 = 0.0f, d1 = 0.0f, d2 = 0.0f;
    std::vector<ChannelState> state;
};

} // namespace spacify
