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
    setColour (juce::ResizableWindow::backgroundColourId, palette::backgroundBottom);
    setColour (juce::TooltipWindow::backgroundColourId, palette::knobTop);
    setColour (juce::TooltipWindow::textColourId, palette::textPrimary);
}

void SpacifyLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                           int x, int y, int width, int height,
                                           float sliderPosProportional,
                                           float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto centre = bounds.getCentre();
    const float size = (float) juce::jmin (width, height);
    const float radius = size * 0.5f;

    const float trackRadius = radius * 0.90f;
    const float trackThickness = juce::jmax (2.0f, size * 0.038f);
    const float bodyRadius = radius * 0.70f;

    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const bool enabled = slider.isEnabled();

    // ---- knob body ---------------------------------------------------------
    const auto bodyRect = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);

    {
        // Offset radial gradient: reads as a lit sphere rather than a flat disc.
        const juce::Point<float> highlightCentre { centre.x - bodyRadius * 0.30f,
                                                   centre.y - bodyRadius * 0.38f };

        juce::ColourGradient bodyGradient (palette::knobTop.withAlpha (enabled ? 1.0f : 0.6f),
                                           highlightCentre.x, highlightCentre.y,
                                           palette::knobBottom.withAlpha (enabled ? 1.0f : 0.6f),
                                           highlightCentre.x, highlightCentre.y + bodyRadius * 1.45f,
                                           true);
        g.setGradientFill (bodyGradient);
        g.fillEllipse (bodyRect);
    }

    g.setColour (juce::Colours::white.withAlpha (enabled ? 0.11f : 0.04f));
    g.drawEllipse (bodyRect.reduced (0.5f), 1.0f);

    // ---- track and value arcs ---------------------------------------------
    g.setColour (palette::track);
    g.strokePath (makeArcPath (centre, trackRadius, rotaryStartAngle, rotaryEndAngle),
                  juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));

    if (sliderPosProportional > 0.0005f)
    {
        const auto valueArc = makeArcPath (centre, trackRadius, rotaryStartAngle, angle);

        const float arcAlpha = enabled ? 1.0f : 0.45f;

        juce::ColourGradient glowGradient (palette::accentViolet.withAlpha (arcAlpha * 0.20f),
                                           bounds.getX(), bounds.getBottom(),
                                           palette::accentBlue.withAlpha (arcAlpha * 0.20f),
                                           bounds.getRight(), bounds.getY(), false);

        juce::ColourGradient arcGradient (palette::accentViolet.withAlpha (arcAlpha),
                                          bounds.getX(), bounds.getBottom(),
                                          palette::accentBlue.withAlpha (arcAlpha),
                                          bounds.getRight(), bounds.getY(), false);

        g.setGradientFill (glowGradient);
        g.strokePath (valueArc, juce::PathStrokeType (trackThickness * 2.6f,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

        g.setGradientFill (arcGradient);
        g.strokePath (valueArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // ---- indicator ---------------------------------------------------------
    const juce::Point<float> direction { std::sin (angle), -std::cos (angle) };
    const float indicatorThickness = juce::jmax (2.0f, trackThickness * 0.85f);

    juce::Path indicator;
    indicator.startNewSubPath (centre + direction * (bodyRadius * 0.44f));
    indicator.lineTo (centre + direction * (bodyRadius * 0.94f));

    g.setColour (palette::textPrimary.withAlpha (enabled ? 0.92f : 0.35f));
    g.strokePath (indicator, juce::PathStrokeType (indicatorThickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
}

void SpacifyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const float pillWidth = juce::jmin (bounds.getWidth() * 0.46f, bounds.getHeight() * 1.95f);
    const auto pill = juce::Rectangle<float> (pillWidth, juce::jmin (bounds.getHeight(), pillWidth * 0.52f))
                          .withCentre ({ bounds.getCentreX() + (bounds.getWidth() * 0.5f) - (pillWidth * 0.5f),
                                         bounds.getCentreY() });

    auto textArea = bounds.withRight (pill.getX() - 10.0f);

    const bool on = button.getToggleState();
    const bool enabled = button.isEnabled();

    auto font = makeFont (juce::jmin (13.0f, bounds.getHeight() * 0.40f), {}, 0.16f);
    g.setColour (palette::textDim.withAlpha (enabled ? 1.0f : 0.5f));
    drawTrackedText (g, button.getButtonText().toUpperCase(), textArea.toNearestInt(),
                     juce::Justification::centredRight, font, 0.16f);

    const float cornerSize = pill.getHeight() * 0.5f;

    if (on)
    {
        juce::ColourGradient glow (palette::accentViolet, pill.getX(), pill.getY(),
                                   palette::accentBlue, pill.getRight(), pill.getBottom(), false);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (pill, cornerSize);

        juce::Graphics::ScopedSaveState save (g);
        g.setOpacity (0.35f);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (pill.expanded (1.5f), cornerSize + 1.5f);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (pill, cornerSize);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (pill.reduced (0.5f), cornerSize, 1.0f);
    }

    const float thumbRadius = pill.getHeight() * 0.5f - 3.5f;
    const float thumbCentreX = on ? pill.getRight() - pill.getHeight() * 0.5f
                                  : pill.getX() + pill.getHeight() * 0.5f;

    const auto thumb = juce::Point<float> (thumbCentreX, pill.getCentreY());
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillEllipse (juce::Rectangle<float> (thumbRadius * 2.0f, thumbRadius * 2.0f)
                       .withCentre (thumb.translated (0.0f, 1.0f)));

    g.setColour (on ? juce::Colours::white : juce::Colour (0xffb9bacb));
    g.fillEllipse (juce::Rectangle<float> (thumbRadius * 2.0f, thumbRadius * 2.0f).withCentre (thumb));

    if (shouldDrawButtonAsHighlighted && enabled)
    {
        g.setColour (juce::Colours::white.withAlpha (0.10f + (shouldDrawButtonAsDown ? 0.06f : 0.0f)));
        g.drawRoundedRectangle (pill.reduced (0.5f), cornerSize, 1.0f);
    }
}

} // namespace spacify::ui
