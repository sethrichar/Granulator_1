#pragma once

#include "DspUtils.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace dsp
{
    // Two modulated delay lines in quadrature: a wide, simple stereo chorus.
    class Chorus
    {
    public:
        struct Params { float rateHz = 0.6f; float depth = 0.4f; float mix = 0.0f; };

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            const int len = (int) (0.05 * sampleRate) + 8;   // 50 ms is plenty
            bufL.resize (len); bufR.resize (len);
            mixSmoothed.reset (sampleRate, 0.02);
            depthSmoothed.reset (sampleRate, 0.05);
            reset();
        }

        void reset() { bufL.clear(); bufR.clear(); writePos = 0; phase = 0.0f; }

        void process (juce::AudioBuffer<float>& buffer, const Params& p)
        {
            const int n = buffer.getNumSamples();
            float* L = buffer.getWritePointer (0);
            float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

            mixSmoothed.setTargetValue (p.mix);
            depthSmoothed.setTargetValue (p.depth);
            const float phaseInc = p.rateHz / (float) sr;
            const double base = 0.008 * sr;              // 8 ms centre delay
            const double maxDepth = 0.006 * sr;          // +-6 ms

            for (int i = 0; i < n; ++i)
            {
                const float inL = L[i];
                const float inR = R != nullptr ? R[i] : inL;

                bufL[writePos] = inL;
                bufR[writePos] = inR;

                const float depth = depthSmoothed.getNextValue();
                const double dl = base + maxDepth * depth * std::sin (2.0f * kPi * phase);
                const double dr = base + maxDepth * depth * std::cos (2.0f * kPi * phase);

                const float wl = bufL.readDelayed (writePos, dl);
                const float wr = bufR.readDelayed (writePos, dr);

                if (++writePos >= bufL.size()) writePos = 0;
                phase += phaseInc; if (phase >= 1.0f) phase -= 1.0f;

                const float mix = mixSmoothed.getNextValue();
                L[i] = inL * (1.0f - mix) + wl * mix;
                if (R != nullptr) R[i] = inR * (1.0f - mix) + wr * mix;
            }
        }

    private:
        double sr = 44100.0;
        CircularBuffer bufL, bufR;
        int writePos = 0;
        float phase = 0.0f;
        juce::SmoothedValue<float> mixSmoothed, depthSmoothed;
    };
}
