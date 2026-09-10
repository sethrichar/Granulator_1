#pragma once

#include "Controls.h"
#include "../Parameters.h"

namespace ui
{
    // 5 lanes x 16 steps. Continuous lanes are vertical bars, direction is a
    // toggle per step. Steps past the active count are dimmed; the playing step
    // is highlighted by the editor's timer.
    class StepGrid : public juce::Component
    {
    public:
        static constexpr int kLanes = 5;

        explicit StepGrid (juce::AudioProcessorValueTreeState& s) : state (s)
        {
            const char* ids[kLanes]   = { ParamIDs::lanePitch, ParamIDs::laneDir, ParamIDs::lanePos, ParamIDs::laneSize, ParamIDs::laneLevel };
            const char* names[kLanes] = { "Pitch", "Reverse", "Position", "Size", "Level" };

            for (int l = 0; l < kLanes; ++l)
            {
                auto& lane = lanes[l];
                lane.enable = std::make_unique<Toggle> (state, ParamIDs::laneEnableId (ids[l]), names[l]);
                addAndMakeVisible (*lane.enable);

                for (int i = 0; i < ParamIDs::kMaxSteps; ++i)
                {
                    const auto id = ParamIDs::stepId (ids[l], i);
                    if (l == 1)
                    {
                        auto b = std::make_unique<juce::ToggleButton>();
                        b->setClickingTogglesState (true);
                        b->setColour (juce::ToggleButton::tickColourId, colours::accent2);
                        addAndMakeVisible (*b);
                        lane.buttonAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *b);
                        lane.buttons[i] = std::move (b);
                    }
                    else
                    {
                        auto sl = std::make_unique<juce::Slider> (juce::Slider::LinearBarVertical, juce::Slider::NoTextBox);
                        sl->setColour (juce::Slider::trackColourId, colours::accent2.withAlpha (0.85f));
                        sl->setColour (juce::Slider::backgroundColourId, colours::bg);
                        sl->setPopupDisplayEnabled (true, false, this);
                        sl->setDoubleClickReturnValue (true, l == 4 ? 1.0 : 0.0);
                        addAndMakeVisible (*sl);
                        lane.sliderAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, *sl);
                        lane.sliders[i] = std::move (sl);
                    }
                }
            }
        }

        void setActiveSteps (int n)
        {
            n = juce::jlimit (1, ParamIDs::kMaxSteps, n);
            if (n == activeSteps) return;
            activeSteps = n;
            for (auto& lane : lanes)
                for (int i = 0; i < ParamIDs::kMaxSteps; ++i)
                {
                    const float a = i < activeSteps ? 1.0f : 0.25f;
                    if (lane.sliders[i]) lane.sliders[i]->setAlpha (a);
                    if (lane.buttons[i]) lane.buttons[i]->setAlpha (a);
                }
            repaint();
        }

        void setPlayingStep (int step)
        {
            if (step == playingStep) return;
            playingStep = step;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            if (playingStep >= 0 && playingStep < ParamIDs::kMaxSteps)
            {
                g.setColour (colours::accent2.withAlpha (0.12f));
                g.fillRoundedRectangle (columnBounds (playingStep).toFloat(), 3.0f);
            }
            g.setColour (colours::dim);
            g.setFont (juce::FontOptions (10.0f));
            for (int i = 0; i < ParamIDs::kMaxSteps; ++i)
                g.drawText (juce::String (i + 1), columnBounds (i).removeFromTop (12), juce::Justification::centred);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            r.removeFromTop (12);
            const int laneH = r.getHeight() / kLanes;
            for (int l = 0; l < kLanes; ++l)
            {
                auto row = r.removeFromTop (laneH);
                lanes[l].enable->setBounds (row.removeFromLeft (labelWidth).reduced (0, 2));
                const float cellW = (float) row.getWidth() / ParamIDs::kMaxSteps;
                for (int i = 0; i < ParamIDs::kMaxSteps; ++i)
                {
                    auto cell = juce::Rectangle<int> (row.getX() + (int) (i * cellW), row.getY(), (int) cellW, row.getHeight()).reduced (2, 3);
                    if (lanes[l].sliders[i]) lanes[l].sliders[i]->setBounds (cell);
                    if (lanes[l].buttons[i]) lanes[l].buttons[i]->setBounds (cell.withSizeKeepingCentre (22, 22));
                }
            }
        }

    private:
        juce::Rectangle<int> columnBounds (int i) const
        {
            auto r = getLocalBounds();
            r.removeFromLeft (labelWidth);
            const float cellW = (float) r.getWidth() / ParamIDs::kMaxSteps;
            return { r.getX() + (int) (i * cellW), 0, (int) cellW, getHeight() };
        }

        struct Lane
        {
            std::unique_ptr<Toggle> enable;
            std::unique_ptr<juce::Slider> sliders[ParamIDs::kMaxSteps];
            std::unique_ptr<juce::ToggleButton> buttons[ParamIDs::kMaxSteps];
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments[ParamIDs::kMaxSteps];
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachments[ParamIDs::kMaxSteps];
        };

        juce::AudioProcessorValueTreeState& state;
        Lane lanes[kLanes];
        int activeSteps = 0;
        int playingStep = -1;
        const int labelWidth = 92;
    };
}
