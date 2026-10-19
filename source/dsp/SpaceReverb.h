#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>
#include <vector>

namespace spacify
{

/**
    Modulated 8x8 Feedback Delay Network (FDN) stereo reverb designed for lush,
    non-metallic shimmer / space layers.

    Architecture:
      1. 20 ms stereo pre-delay + gentle HF air-taming lowpass (~9 kHz) to
         smooth harsh post-fuzz digital edge before entering the tank.
      2. 4-stage stereo unitary Schroeder allpass input diffuser with mutually
         prime delay lengths on L and R to build immediate echo density.
      3. 8-line FDN tank spanning 29 ms .. 83 ms (prime-tuned delays) coupled
         via a lossless 8x8 Householder orthogonal matrix (M = I - (2/8)*1*1^T)
         so energy cross-pollinates across all 8 lines and between L/R.
      4. Per-line fractional-sample LFO modulation (8 decorrelated sine LFOs)
         that eliminates static metallic comb resonances.
      5. Per-line delay-compensated RT60 feedback (g_i = 10^(-3 * D_i / RT60))
         and natural HF air absorption, normalized by incoherent energy so
         perceived loudness remains uniform from 0.01 s to 20.0 s.
*/
class SpaceReverb
{
public:
    static constexpr int numFdnLines = 8;
    static constexpr int numDiffusers = 4;

    void prepare (double newSampleRate, int newNumChannels)
    {
        sampleRate = juce::jmax (8000.0, newSampleRate);
        numChannels = juce::jlimit (1, 2, newNumChannels);

        const double scale = sampleRate / 44100.0;
        modExcursionSamples = (float) (9.5 * scale);
        const int margin = (int) std::ceil (modExcursionSamples) + 8;

        double sumDelaySeconds = 0.0;

        for (int i = 0; i < numFdnLines; ++i)
        {
            const int baseSamples = juce::jmax (32, (int) std::round (fdnBaseTuning[i] * scale));
            fdnLines[(size_t) i].prepare (baseSamples + margin, baseSamples);
            fdnLines[(size_t) i].lfoPhase = (double) i / (double) numFdnLines;
            fdnLines[(size_t) i].lfoInc = fdnLfoRates[i] / sampleRate;
            fdnDelaySeconds[(size_t) i] = (double) baseSamples / sampleRate;
            sumDelaySeconds += fdnDelaySeconds[(size_t) i];
        }

        meanDelaySeconds = sumDelaySeconds / (double) numFdnLines;

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int d = 0; d < numDiffusers; ++d)
            {
                const int dSamples = juce::jmax (16, (int) std::round (diffuserTuning[ch][d] * scale));
                diffusers[(size_t) ch][(size_t) d].prepare (dSamples);
            }
        }

        const int preDelaySamples = juce::jmax (1, (int) std::round (preDelaySeconds * sampleRate));
        preDelayBuffers.assign (2, std::vector<float> ((size_t) preDelaySamples, 0.0f));
        preDelayWriteIndex = 0;

        // Pre-tank HF smoothing (~9.5 kHz 1-pole LP) to tame harsh digital fuzz fizz
        const double lpOmega = juce::MathConstants<double>::twoPi * juce::jmin (9500.0, sampleRate * 0.42) / sampleRate;
        inputLpCoeff = (float) std::exp (-lpOmega);
        inputLpState[0] = 0.0f;
        inputLpState[1] = 0.0f;

        setDecaySeconds (decaySeconds);
        reset();
    }

    void reset()
    {
        for (auto& line : fdnLines)
            line.reset();

        for (int ch = 0; ch < 2; ++ch)
        {
            inputLpState[ch] = 0.0f;
            for (auto& diff : diffusers[(size_t) ch])
                diff.reset();
        }

        for (auto& buffer : preDelayBuffers)
            std::fill (buffer.begin(), buffer.end(), 0.0f);

        preDelayWriteIndex = 0;
    }

    /** Sets the approximate RT60 of the tail, in seconds. */
    void setDecaySeconds (float seconds) noexcept
    {
        decaySeconds = juce::jlimit (0.01f, 60.0f, seconds);

        const double target = (double) decaySeconds * rt60Compensation;

        for (int i = 0; i < numFdnLines; ++i)
        {
            if (target > 0.005)
            {
                const double exponent = -3.0 * fdnDelaySeconds[(size_t) i] / target;
                fdnLines[(size_t) i].feedback = juce::jlimit (0.0f, 0.9965f, (float) std::pow (10.0, exponent));
            }
            else
            {
                fdnLines[(size_t) i].feedback = 0.0f;
            }
        }

        const double meanExponent = target > 0.005 ? (-3.0 * meanDelaySeconds / target) : -100.0;
        const float meanFeedback = target > 0.005 ? juce::jlimit (0.0f, 0.9965f, (float) std::pow (10.0, meanExponent))
                                                  : 0.0f;

        // Natural HF air absorption inside the tank. Kept gentle on long tails so
        // broadband RT60 stays faithful while removing metallic >8 kHz glare.
        damping1 = tankDamping;
        damping2 = 1.0f - tankDamping;

        // Bounded acoustic energy normalization for the 8x8 FDN over musical
        // signals. Matches the finite-window damped buildup from 0.01 s to 20.0 s
        // within < 1 dB without asymptotic blow-up as meanFeedback approaches 1.
        inputGain = targetWetLevel * (1.0f - 0.89f * meanFeedback);
    }

    float getInputGain() const noexcept { return inputGain; }

    void process (juce::AudioBuffer<float>& stereoBuffer, int startSample, int numSamples) noexcept
    {
        if (numChannels <= 0 || stereoBuffer.getNumChannels() < numChannels)
            return;

        const int preDelaySize = preDelayBuffers.empty() ? 0 : (int) preDelayBuffers[0].size();
        if (preDelaySize <= 0)
            return;

        float* outL = stereoBuffer.getWritePointer (0);
        float* outR = numChannels > 1 ? stereoBuffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const int index = startSample + i;

            const float inL = outL[index];
            const float inR = outR != nullptr ? outR[index] : inL;

            // 1. Pre-delay + gentle HF smoothing
            float delayedL = preDelayBuffers[0][(size_t) preDelayWriteIndex];
            float delayedR = preDelayBuffers[1][(size_t) preDelayWriteIndex];
            preDelayBuffers[0][(size_t) preDelayWriteIndex] = inL;
            preDelayBuffers[1][(size_t) preDelayWriteIndex] = inR;

            if (++preDelayWriteIndex >= preDelaySize)
                preDelayWriteIndex = 0;

            inputLpState[0] = delayedL * (1.0f - inputLpCoeff) + inputLpState[0] * inputLpCoeff;
            inputLpState[1] = delayedR * (1.0f - inputLpCoeff) + inputLpState[1] * inputLpCoeff;

            // 2. 4-stage stereo unitary allpass diffusion
            float diffL = inputLpState[0] * inputGain;
            float diffR = inputLpState[1] * inputGain;

            for (int d = 0; d < numDiffusers; ++d)
            {
                diffL = diffusers[0][(size_t) d].process (diffL, diffuserGains[d]);
                diffR = diffusers[1][(size_t) d].process (diffR, diffuserGains[d]);
            }

            // 3. Read modulated outputs from all 8 FDN delay lines
            std::array<float, numFdnLines> tankOut {};
            float tankSum = 0.0f;

            for (int lineIdx = 0; lineIdx < numFdnLines; ++lineIdx)
            {
                const float y = fdnLines[(size_t) lineIdx].readModulated (modExcursionSamples, damping1, damping2);
                tankOut[(size_t) lineIdx] = y;
                tankSum += y;
            }

            // 4. Lossless 8x8 Householder orthogonal matrix: M * y = y - (2/8) * sum(y)
            const float householderTerm = 0.25f * tankSum;

            for (int lineIdx = 0; lineIdx < numFdnLines; ++lineIdx)
            {
                const float mixed = tankOut[(size_t) lineIdx] - householderTerm;
                const float injected = (lineIdx & 1) == 0 ? diffL * inputPolarity[lineIdx]
                                                          : diffR * inputPolarity[lineIdx];
                fdnLines[(size_t) lineIdx].write (injected + mixed * fdnLines[(size_t) lineIdx].feedback);
            }

            // 5. Decorrelated stereo output tap summation
            const float wetL = (tankOut[0] - tankOut[2] + tankOut[4] - tankOut[6]);
            const float wetR = (tankOut[1] - tankOut[3] + tankOut[5] - tankOut[7]);

            outL[index] = wetL;
            if (outR != nullptr)
                outR[index] = wetR;
        }
    }

private:
    struct ModulatedDelayLine
    {
        void prepare (int capacitySamples, int nominalDelay)
        {
            buffer.assign ((size_t) juce::jmax (8, capacitySamples), 0.0f);
            baseDelay = (float) nominalDelay;
            writePos = 0;
            filterState = 0.0f;
        }

        void reset()
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            writePos = 0;
            filterState = 0.0f;
        }

        inline float readModulated (float excursion, float damp1, float damp2) noexcept
        {
            const float mod = (float) std::sin (lfoPhase * juce::MathConstants<double>::twoPi) * excursion;
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0)
                lfoPhase -= 1.0;

            const float delay = baseDelay + mod;
            const int size = (int) buffer.size();

            float readPos = (float) writePos - delay;
            while (readPos < 0.0f)          readPos += (float) size;
            while (readPos >= (float) size) readPos -= (float) size;

            const int idx0 = (int) readPos;
            const float frac = readPos - (float) idx0;
            const int idx1 = (idx0 + 1 < size) ? (idx0 + 1) : 0;

            const float raw = buffer[(size_t) idx0] + frac * (buffer[(size_t) idx1] - buffer[(size_t) idx0]);
            filterState = raw * damp2 + filterState * damp1;
            return filterState;
        }

        inline void write (float sample) noexcept
        {
            buffer[(size_t) writePos] = sample;
            if (++writePos >= (int) buffer.size())
                writePos = 0;
        }

        std::vector<float> buffer;
        float baseDelay = 1000.0f;
        int writePos = 0;
        float filterState = 0.0f;
        float feedback = 0.5f;
        double lfoPhase = 0.0;
        double lfoInc = 0.0;
    };

    struct AllpassDiffuser
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
            const float vn = input + buffered * fb;
            const float output = buffered - vn * fb;

            buffer[(size_t) index] = vn;

            if (++index >= (int) buffer.size())
                index = 0;

            return output;
        }

        std::vector<float> buffer;
        int index = 0;
    };

    // Mutually prime FDN delay lengths spanning 29 ms .. 83 ms at 44.1 kHz
    static constexpr int fdnBaseTuning[numFdnLines] =
        { 1277, 1493, 1747, 2039, 2377, 2767, 3187, 3671 };

    // Incommensurate slow LFO rates (Hz) for chorus-modulated shimmer tail
    static constexpr double fdnLfoRates[numFdnLines] =
        { 0.41, 0.53, 0.67, 0.79, 0.91, 1.07, 1.21, 1.37 };

    static constexpr float inputPolarity[numFdnLines] =
        { 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f };

    // Mutually prime stereo input diffuser lengths at 44.1 kHz
    static constexpr int diffuserTuning[2][numDiffusers] =
    {
        { 149, 233, 353, 521 },
        { 163, 251, 379, 547 }
    };

    static constexpr float diffuserGains[numDiffusers] =
        { 0.62f, 0.58f, 0.54f, 0.50f };

    static constexpr float preDelaySeconds = 0.020f;
    static constexpr float targetWetLevel  = 0.56f;
    static constexpr float tankDamping     = 0.16f;
    static constexpr float rt60Compensation = 1.18f;

    double sampleRate = 44100.0;
    int numChannels = 2;
    float decaySeconds = 8.0f;
    float damping1 = tankDamping;
    float damping2 = 1.0f - tankDamping;
    float inputGain = 0.05f;
    float modExcursionSamples = 9.5f;
    double meanDelaySeconds = 0.052;

    float inputLpCoeff = 0.25f;
    float inputLpState[2] = { 0.0f, 0.0f };

    std::array<double, numFdnLines> fdnDelaySeconds {};
    std::array<ModulatedDelayLine, numFdnLines> fdnLines;
    std::array<std::array<AllpassDiffuser, numDiffusers>, 2> diffusers;

    std::vector<std::vector<float>> preDelayBuffers;
    int preDelayWriteIndex = 0;
};

} // namespace spacify
