#pragma once

#include "DspUtils.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace dsp
{
    // Pre-delay + JUCE's built-in (Freeverb-style) reverb, with our own wet/dry
    // mix so it behaves like the other effects. Easy to swap for an FDN later.
    class ReverbFx
    {
    public:
        struct Params
        {
            float size = 0.7f; float damp = 0.4f; float width = 1.0f;
            float predelaySeconds = 0.01f; float mix = 0.0f;
        };

        void prepare (double sampleRate, int maxBlockSize)
        {
            sr = sampleRate;
            reverb.setSampleRate (sampleRate);
            const int len = (int) (0.25 * sampleRate) + 16;
            preL.resize (len); preR.resize (len);
            wet.setSize (2, maxBlockSize);
            mixSmoothed.reset (sampleRate, 0.02);
            reset();
        }

        void reset() { reverb.reset(); preL.clear(); preR.clear(); writePos = 0; }

        void process (juce::AudioBuffer<float>& buffer, const Params& p)
        {
            const int n = buffer.getNumSamples();
            float* L = buffer.getWritePointer (0);
            float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

            juce::Reverb::Parameters rp;
            rp.roomSize = p.size; rp.damping = p.damp; rp.width = p.width;
            rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.freezeMode = 0.0f;
            reverb.setParameters (rp);
            mixSmoothed.setTargetValue (p.mix);

            const double pd = std::clamp (p.predelaySeconds * sr, 1.0, (double) preL.size() - 8.0);
            float* wl = wet.getWritePointer (0);
            float* wr = wet.getWritePointer (1);

            for (int i = 0; i < n; ++i)
            {
                const float inL = L[i];
                const float inR = R != nullptr ? R[i] : inL;
                preL[writePos] = inL; preR[writePos] = inR;
                wl[i] = preL.readDelayed (writePos, pd);
                wr[i] = preR.readDelayed (writePos, pd);
                if (++writePos >= preL.size()) writePos = 0;
            }

            reverb.processStereo (wl, wr, n);

            for (int i = 0; i < n; ++i)
            {
                const float mix = mixSmoothed.getNextValue();
                const float inL = L[i];
                const float inR = R != nullptr ? R[i] : inL;
                L[i] = inL * (1.0f - mix) + wl[i] * mix;
                if (R != nullptr) R[i] = inR * (1.0f - mix) + wr[i] * mix;
            }
        }

    private:
        double sr = 44100.0;
        juce::Reverb reverb;
        CircularBuffer preL, preR;
        int writePos = 0;
        juce::AudioBuffer<float> wet;
        juce::SmoothedValue<float> mixSmoothed;
    };
}
