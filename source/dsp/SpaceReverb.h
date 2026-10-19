#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <vector>

namespace spacify
{

/**
    Schroeder / Freeverb style reverb used to build the "space layer".

    Differences from the classic Freeverb implementation that matter here:

      - The comb feedback is derived from the requested RT60 rather than from a
        normalised 0..1 "room size", so DECAY can be specified in seconds and
        stays accurate all the way up to 20 s. (Freeverb's room-size parameter
        saturates at roughly 12 s, which is why juce::dsp::Reverb cannot be used.)
      - The wet signal is normalised so that the perceived level is roughly
        independent of DECAY: the input is scaled by (1 - feedback), which keeps
        the steady state constant instead of growing with the tail length.
      - Left and right are processed through independent delay networks with the
        classic 23 sample stereo offset, so a stereo input keeps its image.

    The output is 100% wet; there is no dry signal anywhere inside.
*/
class SpaceReverb
{
public:
    static constexpr int numCombs = 8;
    static constexpr int numAllpasses = 4;

    void prepare (double newSampleRate, int newNumChannels)
    {
        sampleRate = newSampleRate;
        numChannels = juce::jlimit (1, 2, newNumChannels);

        const double scale = sampleRate / 44100.0;
        const int spread = (int) std::round (23.0 * scale);

        combDelaySamples.resize ((size_t) (numChannels * numCombs));
        allpassDelaySamples.resize ((size_t) (numChannels * numAllpasses));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const int offset = ch == 1 ? spread : 0;

            for (int i = 0; i < numCombs; ++i)
                combDelaySamples[(size_t) (ch * numCombs + i)]
                    = juce::jmax (8, (int) std::round (combTuning[i] * scale) + offset);

            for (int i = 0; i < numAllpasses; ++i)
                allpassDelaySamples[(size_t) (ch * numAllpasses + i)]
                    = juce::jmax (8, (int) std::round (allpassTuning[i] * scale) + offset);
        }

        combs.resize ((size_t) (numChannels * numCombs));
        allpasses.resize ((size_t) (numChannels * numAllpasses));

        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numCombs; ++i)
                combs[(size_t) (ch * numCombs + i)].prepare (combDelaySamples[(size_t) (ch * numCombs + i)]);

        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numAllpasses; ++i)
                allpasses[(size_t) (ch * numAllpasses + i)].prepare (allpassDelaySamples[(size_t) (ch * numAllpasses + i)]);

        const int preDelaySamples = juce::jmax (1, (int) std::round (preDelay * sampleRate));
        preDelayBuffers.assign ((size_t) numChannels, std::vector<float> ((size_t) preDelaySamples, 0.0f));
        preDelayWriteIndex = 0;

        // A short tone-independent average, used for the level compensation.
        double averageCombDelay = 0.0;

        for (int i = 0; i < numCombs; ++i)
            averageCombDelay += (double) combDelaySamples[(size_t) i] / sampleRate;

        averageCombDelay /= (double) numCombs;
        meanCombDelaySeconds = averageCombDelay;

        dampingCoefficient = defaultDamping;

        setDecaySeconds (decaySeconds);
        reset();
    }

    void reset()
    {
        for (auto& comb : combs)     comb.reset();
        for (auto& allpass : allpasses) allpass.reset();

        for (auto& buffer : preDelayBuffers)
            std::fill (buffer.begin(), buffer.end(), 0.0f);

        preDelayWriteIndex = 0;
    }

    /** Sets the approximate RT60 of the tail, in seconds. */
    void setDecaySeconds (float seconds) noexcept
    {
        decaySeconds = juce::jlimit (0.1f, 60.0f, seconds);

        // A little cushion, because the (deliberately light) damping in the comb
        // loops pulls the broadband decay in slightly below the low frequency
        // decay that the feedback is derived from.
        const double target = (double) decaySeconds * rt60Compensation;
        const double exponent = -3.0 * meanCombDelaySeconds / target;
        const float feedback = (float) std::pow (10.0, exponent);

        for (auto& comb : combs)
            comb.feedback = feedback;

        damping1 = dampingCoefficient;
        damping2 = 1.0f - dampingCoefficient;

        // Keep the steady-state level independent of the tail length.
        inputGain = inputTrim * (1.0f - feedback);
    }

    float getInputGain() const noexcept { return inputGain; }

    void process (juce::AudioBuffer<float>& stereoBuffer, int startSample, int numSamples) noexcept
    {
        if (numChannels <= 0 || stereoBuffer.getNumChannels() < numChannels)
            return;

        const int preDelaySize = preDelayBuffers.empty() ? 0 : (int) preDelayBuffers[0].size();

        if (preDelaySize <= 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const int index = startSample + i;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& preDelayBuffer = preDelayBuffers[(size_t) ch];
                const float delayed = preDelayBuffer[(size_t) preDelayWriteIndex];
                preDelayBuffer[(size_t) preDelayWriteIndex] = stereoBuffer.getReadPointer (ch)[index];

                float value = delayed * inputGain;

                float combined = 0.0f;

                for (int c = 0; c < numCombs; ++c)
                    combined += combs[(size_t) (ch * numCombs + c)].process (value, damping1, damping2);

                for (int a = 0; a < numAllpasses; ++a)
                    combined = allpasses[(size_t) (ch * numAllpasses + a)].process (combined, allpassFeedback);

                stereoBuffer.getWritePointer (ch)[index] = combined;
            }

            if (++preDelayWriteIndex >= preDelaySize)
                preDelayWriteIndex = 0;
        }
    }

private:
    struct CombFilter
    {
        void prepare (int size)
        {
            buffer.assign ((size_t) juce::jmax (1, size), 0.0f);
            index = 0;
            filterState = 0.0f;
        }

        void reset()
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            index = 0;
            filterState = 0.0f;
        }

        inline float process (float input, float damp1, float damp2) noexcept
        {
            const float output = buffer[(size_t) index];

            filterState = output * damp2 + filterState * damp1;

            buffer[(size_t) index] = input + filterState * feedback;

            if (++index >= (int) buffer.size())
                index = 0;

            return output;
        }

        std::vector<float> buffer;
        int index = 0;
        float filterState = 0.0f;
        float feedback = 0.5f;
    };

    struct AllpassFilter
    {
        void prepare (int size)
        {
            buffer.assign ((size_t) juce::jmax (1, size), 0.0f);
            index = 0;
        }

        void reset()
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            index = 0;
        }

        inline float process (float input, float fb) noexcept
        {
            const float buffered = buffer[(size_t) index];
            const float output = -input + buffered;

            buffer[(size_t) index] = input + buffered * fb;

            if (++index >= (int) buffer.size())
                index = 0;

            return output;
        }

        std::vector<float> buffer;
        int index = 0;
    };

    static constexpr int combTuning[numCombs] =
        { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };

    static constexpr int allpassTuning[numAllpasses] = { 556, 441, 341, 225 };

    static constexpr float allpassFeedback = 0.5f;
    static constexpr float preDelay = (float) 0.020;
    static constexpr float inputTrim = 0.30f;

    // A comb loop that already runs at a gain of ~0.97 per pass colours the tail
    // very heavily for even a small amount of damping, so this is deliberately
    // light: the layer is meant to stay bright and airy, and DECAY should read as
    // the decay time you actually hear.
    static constexpr float defaultDamping = 0.05f;

    // Small correction factor so that the RT60 measured on a broadband burst
    // matches the value shown on the control.
    static constexpr float rt60Compensation = 1.12f;

    double sampleRate = 44100.0;
    int numChannels = 2;
    float decaySeconds = 8.0f;
    float dampingCoefficient = 0.25f;
    float damping1 = 0.25f;
    float damping2 = 0.75f;
    float inputGain = 0.01f;
    double meanCombDelaySeconds = 0.03;

    std::vector<int> combDelaySamples;
    std::vector<int> allpassDelaySamples;
    std::vector<CombFilter> combs;
    std::vector<AllpassFilter> allpasses;

    std::vector<std::vector<float>> preDelayBuffers;
    int preDelayWriteIndex = 0;
};

} // namespace spacify
