#pragma once
#include <JuceHeader.h>

class PsyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PsyLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff07050c));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff140e26));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff00f5ff).withAlpha(0.55f));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xff00f5ff));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff120c22));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffff00cc).withAlpha(0.45f));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff3ecff));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b1430));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffff00cc));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff3ecff));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(6.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto cx = bounds.getCentreX();
        auto cy = bounds.getCentreY();
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        bool isFx = slider.getProperties().getWithDefault("isFx", false);
        auto accentCol = isFx ? juce::Colour(0xffff00cc) : juce::Colour(0xff00f5ff);

        // Background track arc
        juce::Path bgArc;
        bgArc.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff9d4edd).withAlpha(0.28f));
        g.strokePath(bgArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active value neon arc
        if (sliderPosProportional > 0.001f)
        {
            juce::Path valArc;
            valArc.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(accentCol.withAlpha(0.32f));
            g.strokePath(valArc, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(accentCol);
            g.strokePath(valArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Knob body
        auto bodyR = radius * 0.78f;
        juce::ColourGradient grad(juce::Colour(0xff3c2b66), cx - bodyR * 0.4f, cy - bodyR * 0.4f,
                                  juce::Colour(0xff090612), cx + bodyR * 0.6f, cy + bodyR * 0.6f, true);
        g.setGradientFill(grad);
        g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        g.setColour(juce::Colour(0xff9d4edd).withAlpha(0.6f));
        g.drawEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.4f);

        // Pointer
        juce::Path ptr;
        auto ptrLen = bodyR * 0.65f;
        ptr.addRoundedRectangle(-2.0f, -bodyR + 3.0f, 4.0f, ptrLen, 1.8f);
        ptr.applyTransform(juce::AffineTransform::rotation(angle).translated(cx, cy));
        g.setColour(accentCol);
        g.fillPath(ptr);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        auto col = backgroundColour;
        if (button.getToggleState())
            col = button.findColour(juce::TextButton::buttonOnColourId);
        if (shouldDrawButtonAsDown)
            col = col.brighter(0.25f);
        else if (shouldDrawButtonAsHighlighted)
            col = col.brighter(0.12f);

        g.setColour(col);
        g.fillRoundedRectangle(bounds, 6.0f);

        auto borderCol = button.getToggleState()
            ? juce::Colour(0xffaaff00)
            : (shouldDrawButtonAsHighlighted ? juce::Colour(0xff00f5ff) : juce::Colour(0xff9d4edd).withAlpha(0.6f));
        g.setColour(borderCol);
        g.drawRoundedRectangle(bounds, 6.0f, 1.2f);
    }
};
