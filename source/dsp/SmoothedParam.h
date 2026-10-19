#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

namespace spacify
{

/** One-pole parameter smoother.

    Deliberately not juce::SmoothedValue: this is advanced once per processing
    sub-block (rather than once per sample), which is all the resolution the
    filter / fuzz / reverb coefficients need, and it keeps the per-sample loops
    free of smoothing work.
*/
struct SmoothedParam
{
    void reset (float value) noexcept
    {
        current = value;
        target = value;
    }

    void setTarget (float value) noexcept
    {
        target = value;
    }

    float getTarget() const noexcept { return target; }
    float getCurrent() const noexcept { return current; }

    bool isSmoothing() const noexcept { return current != target; }

    /** Moves the smoother on by `numSamples` and returns the value to use for
        that span.
    */
    float advance (int numSamples, double sampleRate, double timeConstantSeconds) noexcept
    {
        const double denominator = timeConstantSeconds * sampleRate;

        const float coefficient = denominator > 0.0
                                    ? (float) (1.0 - std::exp (-(double) numSamples / denominator))
                                    : 1.0f;

        current += (target - current) * juce::jlimit (0.0f, 1.0f, coefficient);

        if (std::abs (target - current) < 1.0e-5f)
            current = target;

        return current;
    }

    float current = 0.0f;
    float target = 0.0f;
};

} // namespace spacify
