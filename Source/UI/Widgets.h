#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CrtLookAndFeel.h"

namespace labx10::ui
{
    using APVTS = juce::AudioProcessorValueTreeState;

    // Rotary control with a caption above and its value below.
    class LabeledKnob final : public juce::Component
    {
    public:
        LabeledKnob (APVTS& state, const juce::String& paramID, const juce::String& caption, bool accent = false)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
            slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            if (accent)
                slider.getProperties().set ("accent", true);

            if (auto* param = state.getParameter (paramID))
            {
                const auto range = param->getNormalisableRange();
                if (range.start < 0.0f && range.end > 0.0f)
                    slider.getProperties().set ("bipolar", true);
                slider.setTooltip (param->getName (64));
            }

            label.setText (caption, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setColour (juce::Label::textColourId, accent ? Palette::red : Palette::dim);
            label.setInterceptsMouseClicks (false, false);
            label.setMinimumHorizontalScale (0.6f);

            addAndMakeVisible (label);
            addAndMakeVisible (slider);

            attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, slider);

            if (auto* param = state.getParameter (paramID))
                slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        }

        void resized() override
        {
            auto b = getLocalBounds();
            label.setBounds (b.removeFromTop (13));
            slider.setBounds (b);
        }

        juce::Slider slider;
        juce::Label label;

    private:
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    // Choice parameter as a combo box with a caption above.
    class LabeledChoice final : public juce::Component
    {
    public:
        LabeledChoice (APVTS& state, const juce::String& paramID, const juce::String& caption)
        {
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
                box.addItemList (choice->choices, 1);
            if (auto* param = state.getParameter (paramID))
                box.setTooltip (param->getName (64));

            label.setText (caption, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centredLeft);
            label.setInterceptsMouseClicks (false, false);

            addAndMakeVisible (label);
            addAndMakeVisible (box);
            attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramID, box);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            label.setBounds (b.removeFromTop (13));
            b.removeFromTop (2);
            box.setBounds (b.removeFromTop (22));
        }

        static constexpr int naturalHeight = 37;

        juce::ComboBox box;
        juce::Label label;

    private:
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    // Up to two choices stacked in one column, so a panel with several switches stays narrow.
    class ChoiceStack final : public juce::Component
    {
    public:
        ChoiceStack (APVTS& state, const juce::String& idA, const juce::String& captionA,
                     const juce::String& idB = {}, const juce::String& captionB = {})
            : top (state, idA, captionA)
        {
            addAndMakeVisible (top);
            if (idB.isNotEmpty())
            {
                bottom = std::make_unique<LabeledChoice> (state, idB, captionB);
                addAndMakeVisible (*bottom);
            }
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (3, 0);
            const int h = LabeledChoice::naturalHeight;
            if (bottom == nullptr)
            {
                top.setBounds (b.withHeight (h).withY (b.getY() + 8));
                return;
            }
            const int spare = std::max (0, b.getHeight() - 2 * h);
            top.setBounds (b.removeFromTop (h + spare / 3).withHeight (h).translated (0, spare / 6));
            bottom->setBounds (b.withHeight (h).translated (0, spare / 6));
        }

    private:
        LabeledChoice top;
        std::unique_ptr<LabeledChoice> bottom;
    };

    // POWER gauge: the Rainbow emitter's output, driven by SCORCH and the output level.
    class PowerGauge final : public juce::Component
    {
    public:
        void setTarget (float v) noexcept { target = std::clamp (v, 0.0f, maxValue); }

        void tick()
        {
            const float coef = target > value ? 0.35f : 0.07f;
            value += (target - value) * coef;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (6.0f);
            const auto readout = bounds.removeFromBottom (22.0f);
            const float radius = std::min (bounds.getWidth() * 0.5f, bounds.getHeight() * 0.95f) - 4.0f;
            const juce::Point<float> centre (bounds.getCentreX(), bounds.getBottom() - 4.0f);
            const float start = -juce::MathConstants<float>::halfPi * 0.95f;
            const float end = juce::MathConstants<float>::halfPi * 0.95f;

            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
            g.setColour (Palette::faint);
            g.strokePath (arc, juce::PathStrokeType (3.0f));

            const float danger = start + (end - start) * 0.7f;
            juce::Path hot;
            hot.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, danger, end, true);
            g.setColour (Palette::red.withAlpha (0.55f));
            g.strokePath (hot, juce::PathStrokeType (3.0f));

            g.setFont (monoFont (10.0f));
            for (int i = 0; i <= 10; ++i)
            {
                const float a = start + (end - start) * (float) i / 10.0f;
                const float inner = radius - (i % 2 == 0 ? 10.0f : 6.0f);
                const auto p1 = centre.getPointOnCircumference (inner, a);
                const auto p2 = centre.getPointOnCircumference (radius, a);
                g.setColour (i >= 7 ? Palette::red.withAlpha (0.8f) : Palette::dim);
                g.drawLine ({ p1, p2 }, 1.2f);
                if (i % 2 == 0)
                {
                    const auto tp = centre.getPointOnCircumference (radius - 20.0f, a);
                    g.drawText (juce::String (i * 10), juce::Rectangle<float> (26.0f, 12.0f).withCentre (tp), juce::Justification::centred);
                }
            }

            const float needleAngle = start + (end - start) * (value / maxValue);
            const auto tip = centre.getPointOnCircumference (radius - 4.0f, needleAngle);
            const auto needle = value > maxValue * 0.7f ? Palette::red : Palette::bright;
            g.setColour (needle.withAlpha (0.25f));
            g.drawLine ({ centre, tip }, 5.0f);
            g.setColour (needle);
            g.drawLine ({ centre, tip }, 1.8f);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (centre));

            g.setColour (value > maxValue * 0.7f ? Palette::red : Palette::phosphor);
            g.setFont (monoFont (14.0f, true));
            g.drawText (juce::String (value, 1) + utf8 (" МВт"), readout, juce::Justification::centred);
        }

    private:
        static constexpr float maxValue = 100.0f;
        float value = 0.0f, target = 0.0f;
    };

    // SUB MONITOR: level below 60 Hz and the stereo correlation of the low band (it should read +1).
    class SubMonitor final : public juce::Component
    {
    public:
        void setValues (float subDb, float correlation)
        {
            level += (std::clamp ((subDb + 60.0f) / 60.0f, 0.0f, 1.0f) - level) * 0.3f;
            corr += (std::clamp (correlation, -1.0f, 1.0f) - corr) * 0.3f;
            dbText = subDb <= -99.0f ? juce::String ("-INF") : juce::String (subDb, 1);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.0f, 2.0f);
            g.setFont (monoFont (10.5f));

            auto row = [&g] (juce::Rectangle<float> r, const juce::String& caption, float fill, bool centreZero,
                             const juce::String& text, juce::Colour colour)
            {
                g.setColour (Palette::dim);
                g.drawText (caption, r.removeFromLeft (34.0f), juce::Justification::centredLeft);
                auto readout = r.removeFromRight (52.0f);
                auto bar = r.reduced (2.0f, 4.0f);
                g.setColour (Palette::faint);
                g.fillRect (bar);
                g.setColour (colour);
                if (centreZero)
                {
                    const float mid = bar.getCentreX();
                    const float x = bar.getX() + bar.getWidth() * (fill * 0.5f + 0.5f);
                    g.fillRect (juce::Rectangle<float> (std::min (mid, x), bar.getY(), std::abs (x - mid), bar.getHeight()));
                    g.setColour (Palette::border);
                    g.drawVerticalLine ((int) mid, bar.getY() - 2.0f, bar.getBottom() + 2.0f);
                }
                else
                {
                    g.fillRect (bar.withWidth (bar.getWidth() * fill));
                }
                g.setColour (colour);
                g.drawText (text, readout, juce::Justification::centredRight);
            };

            const float h = b.getHeight() / 2.0f;
            row (b.removeFromTop (h), "SUB", level, false, dbText + " dB", Palette::phosphor);
            row (b, "CORR", corr, true, (corr >= 0.0f ? "+" : "") + juce::String (corr, 2),
                 corr < 0.5f ? Palette::red : Palette::phosphor);
        }

    private:
        float level = 0.0f, corr = 1.0f;
        juce::String dbText { "-INF" };
    };

    // Phosphor oscilloscope with a rising-edge trigger and a short persistence trail.
    class ScopeView final : public juce::Component
    {
    public:
        static constexpr int points = 512;

        void setSamples (const float* source, int numSource)
        {
            int start = 0;
            const int searchEnd = std::max (0, numSource - points * 2);
            for (int i = 1; i < searchEnd; ++i)
                if (source[i - 1] <= 0.0f && source[i] > 0.0f)
                {
                    start = i;
                    break;
                }

            history[2] = history[1];
            history[1] = history[0];
            for (int i = 0; i < points; ++i)
            {
                const int idx = start + i * 2;
                history[0][(size_t) i] = idx < numSource ? source[idx] : 0.0f;
            }
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            g.setColour (Palette::background);
            g.fillRect (b);

            g.setColour (Palette::faint);
            for (int i = 1; i < 8; ++i)
                g.drawVerticalLine ((int) (b.getX() + b.getWidth() * (float) i / 8.0f), b.getY(), b.getBottom());
            for (int i = 1; i < 4; ++i)
                g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * (float) i / 4.0f), b.getX(), b.getRight());

            const float alphas[3] = { 1.0f, 0.35f, 0.15f };
            for (int h = 2; h >= 0; --h)
            {
                juce::Path trace;
                for (int i = 0; i < points; ++i)
                {
                    const float x = b.getX() + b.getWidth() * (float) i / (float) (points - 1);
                    const float y = b.getCentreY() - std::clamp (history[(size_t) h][(size_t) i] * 1.6f, -1.0f, 1.0f) * b.getHeight() * 0.45f;
                    if (i == 0)
                        trace.startNewSubPath (x, y);
                    else
                        trace.lineTo (x, y);
                }
                if (h == 0)
                {
                    g.setColour (Palette::phosphor.withAlpha (0.18f));
                    g.strokePath (trace, juce::PathStrokeType (4.0f));
                }
                g.setColour (Palette::phosphor.withAlpha (alphas[h]));
                g.strokePath (trace, juce::PathStrokeType (1.3f));
            }

            g.setColour (Palette::border);
            g.drawRect (b, 1.0f);
        }

    private:
        std::array<std::array<float, points>, 3> history {};
    };

    // Scanlines and a vignette drawn over everything. Never takes mouse input.
    class ScanlineOverlay final : public juce::Component
    {
    public:
        ScanlineOverlay()
        {
            setInterceptsMouseClicks (false, false);
            pattern = juce::Image (juce::Image::ARGB, 4, 3, true);
            for (int x = 0; x < 4; ++x)
                pattern.setPixelAt (x, 2, juce::Colour (0x2a000000));
        }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            g.setFillType (juce::FillType (pattern, juce::AffineTransform()));
            g.fillRect (b);

            juce::ColourGradient vignette (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                                           juce::Colour (0x66000000), b.getX(), b.getY(), true);
            g.setGradientFill (vignette);
            g.fillRect (b);
        }

    private:
        juce::Image pattern;
    };
}
