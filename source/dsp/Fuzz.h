#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

namespace spacify
{

/**
    Soft-clipping waveshaper tuned for the "shhh / fshh" bright fuzz layer
    rather than for guitar-style distortion.

    A small DC bias is mixed in before the non-linearity (and removed again
    afterwards) so that even order harmonics appear alongside the dominant odd
    ones produced by tanh, which is what gives the layer its sizzle.

    The output is normalised so that a signal of `referenceLevel` is neither
    boosted nor cut, which keeps the wet layer at a predictable loudness
    regardless of how hard FUZZ is driven.
*/
class Fuzz
{
public:
    void setAmount (float newAmount01) noexcept
    {
        const float amount = juce::jlimit (0.0f, 1.0f, newAmount01);

        drive = 1.0f + amount * 24.0f;

        // Computed with the same call as the processing path so that a silent
        // input yields a bit-exact silent output.
        restOffset = std::tanh (dcOffset);

        const float reference = 0.30f;
        const float normalised = std::tanh (drive * reference + dcOffset) - restOffset;

        makeup = normalised > 1.0e-6f ? reference / normalised : 1.0f;
    }

    float getDrive() const noexcept { return drive; }

    inline float processSample (float x) const noexcept
    {
        return (std::tanh (x * drive + dcOffset) - restOffset) * makeup;
    }

private:
    static constexpr float dcOffset = 0.08f;

    float drive = 15.4f;
    float makeup = 0.3f;
    float restOffset = 0.07983006f;
};

} // namespace spacify
