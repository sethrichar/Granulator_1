#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/GrainEngine.h"
#include "dsp/Chorus.h"
#include "dsp/TapeDelay.h"
#include "dsp/ReverbFx.h"

class RuminateAudioProcessor : public juce::AudioProcessor
{
public:
    RuminateAudioProcessor();
    ~RuminateAudioProcessor() override;

    // -- AudioProcessor -----------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 60.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // -- for the editor -----------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    int currentStep() const { return engine.lastStep(); }
    int activeGrains() const { return engine.activeGrainCount(); }

    // For tests: run the chain with an explicit transport instead of a host.
    void setTestTransport (bool playing, double bpm, double ppq) { testTransport = { true, playing, bpm, ppq }; }

private:
    struct RawParams;                 // cached atomic pointers
    std::unique_ptr<RawParams> raw;

    struct Transport { bool valid = false; bool playing = false; double bpm = 120.0; double ppq = 0.0; };
    Transport readTransport (int numSamples);
    Transport testTransport;

    void buildSequencerSettings (dsp::SequencerSettings& s) const;

    dsp::GrainEngine engine;
    dsp::StepClock   clock;
    dsp::Chorus      chorus;
    dsp::TapeDelay   tape;
    dsp::ReverbFx    reverb;
    dsp::SequencerSettings seqSettings;

    juce::SmoothedValue<float> outputGain;
    juce::AudioBuffer<float> stereoScratch;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RuminateAudioProcessor)
};
