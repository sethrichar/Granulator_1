#pragma once

#include "DspUtils.h"
#include "StepSequencer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace dsp
{
    // Buffer-based grain delay. A long circular buffer is written continuously
    // (unless frozen); grains are spawned at "rate" and read from "time" behind
    // the write head, forward or backward, at a pitch ratio, through an
    // asymmetric raised-cosine window. The wet signal is filtered, soft-clipped
    // and fed back into the buffer.
    class GrainEngine
    {
    public:
        static constexpr int kMaxGrains = 192;

        struct Params
        {
            float timeSeconds  = 0.35f;
            float scatter      = 0.0f;   // 0..1, fraction of time
            float sizeSeconds  = 0.12f;
            float sizeRand     = 0.0f;   // 0..1
            float rateHz       = 12.0f;
            float rateRand     = 0.0f;   // 0..1
            float pitchSemis   = 0.0f;
            float pitchRand    = 0.0f;   // semitones
            float reverseProb  = 0.0f;   // 0..1
            float shape        = 0.5f;   // 0..1
            float feedback     = 0.2f;   // 0..1
            float tone         = 0.0f;   // -1..1
            float spread       = 0.3f;   // 0..1
            float mix          = 0.5f;   // 0..1
            bool  freeze       = false;
        };

        void prepare (double sampleRate, int maxBlockSize, double maxBufferSeconds);
        void reset();

        // Processes in place. `buffer` must have 1 or 2 channels; output is stereo
        // when 2 channels are present.
        void process (juce::AudioBuffer<float>& buffer, const Params& p,
                      const SequencerSettings& seq, StepClock& clock);

        int  activeGrainCount() const { return activeCount.load (std::memory_order_relaxed); }
        int  lastStep() const         { return lastStepIndex.load (std::memory_order_relaxed); }
        double bufferSeconds() const  { return bufferLengthSeconds; }

    private:
        struct Grain
        {
            bool   active = false;
            double readPos = 0.0;       // absolute position in buffer
            double inc = 1.0;           // samples per output sample (signed)
            float  phase = 0.0f;        // 0..1 over the grain
            float  phaseInc = 0.0f;
            float  attack = 0.5f;       // fraction of the grain spent rising
            float  amp = 1.0f;
            float  gainL = 1.0f, gainR = 1.0f;
        };

        void spawn (const Params& p, const StepModifiers& mod);
        float window (float phase, float attack) const;

        double sr = 44100.0;
        double bufferLengthSeconds = 60.0;
        CircularBuffer bufL, bufR;
        int writePos = 0;

        std::array<Grain, kMaxGrains> grains;
        int samplesToNextGrain = 0;

        OnePole toneLpL, toneLpR, toneHpL, toneHpR;
        float fbL = 0.0f, fbR = 0.0f;   // last wet sample (post-filter) for feedback

        juce::SmoothedValue<float> mixSmoothed, feedbackSmoothed;
        FastRandom rng;

        std::atomic<int> activeCount { 0 };
        std::atomic<int> lastStepIndex { 0 };
    };
}
