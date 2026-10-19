#include "SpacifyLookAndFeel.h"

namespace spacify::ui
{

namespace
{
    constexpr const char* uiTypeface = "Segoe UI";

    juce::Path makeArcPath (juce::Point<float> centre, float radius, float fromRadians, float toRadians)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, fromRadians, toRadians, true);
        return arc;
    }
}

juce::Font makeFont (float height, const juce::String& style, float tracking)
{
    auto options = juce::FontOptions (height).withName (uiTypeface);

    if (style.isNotEmpty())
        options = options.withStyle (style);

    auto font = juce::Font (options);

    if (tracking != 0.0f)
        font = font.withExtraKerningFactor (tracking);

    return font;
}

void drawTrackedText (juce::Graphics& g,
                      const juce::String& text,
                      juce::Rectangle<int> area,
                      juce::Justification justification,
                      const juce::Font& font,
                      float tracking)
{
    // withExtraKerningFactor leaves a trailing gap after the final glyph, which
    // shifts centred text slightly to the left. Nudge it back.
    if (tracking != 0.0f)
        area = area.translated (juce::roundToInt (tracking * font.getHeight() * 0.5f), 0);

    g.setFont (font);
    g.drawText (text, area, justification, false);
}

//==============================================================================
SpacifyLookAndFeel::SpacifyLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, palette::canvasBottom);
    setColour (juce::TooltipWindow::backgroundColourId, palette::surface);
    setColour (juce::TooltipWindow::textColourId, palette::textPrimary);
}

void SpacifyLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                           int x, int y, int width, int height,
                                           float sliderPosProportional,
                                           float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    juce::ignoreUnused (slider);

    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto centre = bounds.getCentre();
    const float size = (float) juce::jmin (width, height);
    const float radius = size * 0.5f;

    const float trackRadius = radius * 0.87f;
    const float trackThickness = juce::jmax (2.4f, size * 0.036f);
    const float bodyRadius = radius * 0.68f;
    const float rimThickness = juce::jmax (1.0f, size * 0.011f);

    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // ---- track and value arcs ---------------------------------------------
    g.setColour (palette::track);
    g.strokePath (makeArcPath (centre, trackRadius, rotaryStartAngle, rotaryEndAngle),
                  juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));

    if (sliderPosProportional > 0.0005f)
    {
        const auto valueArc = makeArcPath (centre, trackRadius, rotaryStartAngle, angle);

        juce::ColourGradient arcGradient (palette::accentPrimary,
                                          centre.x - trackRadius, centre.y + trackRadius,
                                          palette::accentSecondary,
                                          centre.x + trackRadius, centre.y - trackRadius, false);

        g.setGradientFill (arcGradient);
        g.strokePath (valueArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // ---- tactile ceramic knob body ----------------------------------------
    const auto bodyRect = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    const float shadowOffset = juce::jmax (1.2f, size * 0.022f);

    // Soft ambient shadow
    g.setColour (palette::knobShadow.withMultipliedAlpha (0.55f));
    g.fillEllipse (bodyRect.translated (0.0f, shadowOffset * 1.6f).expanded (shadowOffset * 0.3f));

    g.setColour (palette::knobShadow);
    g.fillEllipse (bodyRect.translated (0.0f, shadowOffset * 0.8f));

    // Matte ceramic face
    juce::ColourGradient bodyGradient (palette::knobFaceTop,
                                       centre.x, bodyRect.getY(),
                                       palette::knobFaceBottom,
                                       centre.x, bodyRect.getBottom(),
                                       false);
    g.setGradientFill (bodyGradient);
    g.fillEllipse (bodyRect);

    // Crisp outer rim & inner top specular edge
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawEllipse (bodyRect.reduced (rimThickness * 1.2f), rimThickness);

    g.setColour (palette::knobBorder);
    g.drawEllipse (bodyRect.reduced (rimThickness * 0.5f), rimThickness);

    // ---- high-contrast indicator line -------------------------------------
    const juce::Point<float> direction { std::sin (angle), -std::cos (angle) };
    const float indicatorThickness = juce::jmax (2.0f, size * 0.026f);

    juce::Path indicator;
    indicator.startNewSubPath (centre + direction * (bodyRadius * 0.38f));
    indicator.lineTo (centre + direction * (bodyRadius * 0.82f));

    g.setColour (palette::textPrimary);
    g.strokePath (indicator, juce::PathStrokeType (indicatorThickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
}

void SpacifyLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h,
                                            bool isMouseOver, bool isMouseDragging)
{
    const float alpha = (isMouseDragging || isMouseOver) ? 0.85f : 0.40f;
    g.setColour (palette::textMuted.withAlpha (alpha));

    const float pad = 4.0f;
    for (int i = 0; i < 2; ++i)
    {
        const float offset = pad + (float) i * 4.5f;
        juce::Path line;
        line.startNewSubPath ((float) w - pad, (float) h - offset);
        line.lineTo ((float) w - offset, (float) h - pad);
        g.strokePath (line, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }
}

} // namespace spacify::ui
