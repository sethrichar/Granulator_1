#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
    namespace colours
    {
        const juce::Colour bg        { 0xff1b1c20 };
        const juce::Colour panel     { 0xff25272d };
        const juce::Colour panelLine { 0xff34373f };
        const juce::Colour text      { 0xffd9dbe0 };
        const juce::Colour dim       { 0xff8a8e99 };
        const juce::Colour accent    { 0xffe8a44b };   // grain / warm
        const juce::Colour accent2   { 0xff5fb7c7 };   // sequencer / cool
        const juce::Colour accent3   { 0xffa88bd6 };   // fx
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::ResizableWindow::backgroundColourId, colours::bg);
            setColour (juce::Slider::textBoxTextColourId, colours::text);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
            setColour (juce::Slider::rotarySliderOutlineColourId, colours::panelLine);
            setColour (juce::Slider::trackColourId, colours::accent2);
            setColour (juce::Slider::backgroundColourId, colours::bg);
            setColour (juce::Label::textColourId, colours::text);
            setColour (juce::ComboBox::backgroundColourId, colours::bg);
            setColour (juce::ComboBox::outlineColourId, colours::panelLine);
            setColour (juce::ComboBox::textColourId, colours::text);
            setColour (juce::ComboBox::arrowColourId, colours::dim);
            setColour (juce::PopupMenu::backgroundColourId, colours::panel);
            setColour (juce::PopupMenu::textColourId, colours::text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.5f));
            setColour (juce::ToggleButton::textColourId, colours::text);
            setColour (juce::ToggleButton::tickColourId, colours::accent);
            setColour (juce::ToggleButton::tickDisabledColourId, colours::dim);
        }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                               float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s) override
        {
            if (style != juce::Slider::LinearBarVertical)
            {
                LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
                return;
            }

            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            g.setColour (s.findColour (juce::Slider::backgroundColourId));
            g.fillRoundedRectangle (r, 2.0f);

            const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
            const float zeroPos = bipolar ? (float) s.getPositionOfValue (0.0) : (float) (y + h);
            const float top = juce::jmin (sliderPos, zeroPos), bottom = juce::jmax (sliderPos, zeroPos);

            g.setColour (s.findColour (juce::Slider::trackColourId));
            g.fillRect (juce::Rectangle<float> (r.getX() + 1.0f, top, r.getWidth() - 2.0f, juce::jmax (2.0f, bottom - top)));

            if (bipolar)
            {
                g.setColour (colours::dim.withAlpha (0.5f));
                g.fillRect (juce::Rectangle<float> (r.getX(), zeroPos - 0.5f, r.getWidth(), 1.0f));
            }
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& s) override
        {
            auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
            const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto centre = bounds.getCentre();
            const float angle = startAngle + pos * (endAngle - startAngle);
            const float lineW = juce::jmax (2.0f, radius * 0.14f);
            const float arcR  = radius - lineW * 0.5f;

            juce::Path back;
            back.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
            g.setColour (s.findColour (juce::Slider::rotarySliderOutlineColourId));
            g.strokePath (back, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // bipolar knobs fill from the centre
            const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
            const float zeroPos = bipolar ? (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : 0.0f;
            const float fromAngle = startAngle + zeroPos * (endAngle - startAngle);

            juce::Path fill;
            fill.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
            g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId).withAlpha (s.isEnabled() ? 1.0f : 0.4f));
            g.strokePath (fill, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path pointer;
            pointer.addRoundedRectangle (-lineW * 0.5f, -radius + lineW, lineW, radius * 0.45f, lineW * 0.5f);
            pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
            g.setColour (colours::text);
            g.fillPath (pointer);
        }
    };

    // A rotary knob with a caption and its parameter attachment.
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& caption,
              juce::Colour accent = colours::accent)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 16);
            slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            slider.setPopupDisplayEnabled (false, false, nullptr);
            addAndMakeVisible (slider);
            label.setText (caption, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            label.setColour (juce::Label::textColourId, colours::dim);
            addAndMakeVisible (label);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (16));
            slider.setBounds (r);
        }

        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    class Choice : public juce::Component
    {
    public:
        Choice (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& caption)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (paramId)))
                box.addItemList (p->choices, 1);
            addAndMakeVisible (box);
            label.setText (caption, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            label.setColour (juce::Label::textColourId, colours::dim);
            addAndMakeVisible (label);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, paramId, box);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (16));
            box.setBounds (r.withSizeKeepingCentre (r.getWidth(), 22));
        }

        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    class Toggle : public juce::Component
    {
    public:
        Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& caption)
        {
            button.setButtonText (caption);
            button.setClickingTogglesState (true);
            addAndMakeVisible (button);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, paramId, button);
        }

        void resized() override { button.setBounds (getLocalBounds()); }

        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    // Titled panel.
    class Section : public juce::Component
    {
    public:
        Section (const juce::String& title, juce::Colour accent) : titleText (title), accentColour (accent) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (colours::panel);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (colours::panelLine);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (accentColour);
            g.fillRoundedRectangle (r.removeFromLeft (4.0f).reduced (0.0f, 6.0f), 2.0f);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.setColour (colours::text);
            g.drawText (titleText, getLocalBounds().removeFromTop (22).withTrimmedLeft (12), juce::Justification::centredLeft);
        }

        juce::Rectangle<int> content() const { return getLocalBounds().withTrimmedTop (24).reduced (8, 4); }

    private:
        juce::String titleText;
        juce::Colour accentColour;
    };
}
