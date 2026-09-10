#pragma once

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/StepGrid.h"

class RuminateAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit RuminateAudioProcessorEditor (RuminateAudioProcessor&);
    ~RuminateAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    RuminateAudioProcessor& processor;
    ui::LookAndFeel lnf;

    // header
    juce::Label title, status;
    ui::Toggle freeze;
    ui::Knob output;

    // sections
    ui::Section grainSection, seqSection, chorusSection, tapeSection, reverbSection;

    std::vector<std::unique_ptr<juce::Component>> grainControls, seqControls, chorusControls, tapeControls, reverbControls;
    ui::StepGrid stepGrid;

    void layoutRow (juce::Rectangle<int> area, std::vector<std::unique_ptr<juce::Component>>& controls);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RuminateAudioProcessorEditor)
};
