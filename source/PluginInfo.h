#pragma once

/**
    Plain compile-time constants for the plug-in identity.

    These intentionally do not use the JucePlugin_* macros so that every source
    file can also be compiled into the offline test / snapshot tooling without
    needing the juce_add_plugin target definitions.
*/
namespace spacify
{
inline constexpr const char* pluginName     = "Spacify";
inline constexpr const char* companyName    = "Spacify Audio";
inline constexpr const char* versionString  = "1.0.0";
inline constexpr int         versionNumber  = 0x010000;

/** Fixed, non user facing parameters of the effect. */
inline constexpr double pitchShiftSemitones = 12.0; // always one octave up
inline constexpr double reverbWetProportion = 1.0;  // reverb runs 100% wet
inline constexpr double preDelaySeconds     = 0.020; // short, fixed pre-delay
} // namespace spacify
