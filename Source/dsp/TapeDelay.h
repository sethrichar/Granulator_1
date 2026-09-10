#pragma once

#include "DspUtils.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace dsp
{
    // Stereo delay with a tape-ish loop: lowpass + highpass + soft saturation in
    // the feedback path, wow & flutter on the read head, and a slow-slewing delay
    // time so time changes bend pitch instead of clicking.
    class TapeDelay
    {
    public:
        struct Params
        {
            float timeSeconds = 0.42f;
            float feedback    = 0.35f;   // 0..1
            float toneHz      = 4500.0f;
            float wow         = 0.2f;    // 0..1
            float drive       = 0.2f;    // 0..1
            float mix         = 0.0f;    // 0..1
        };

        void prepare (double sampleRate, double maxSeconds)
        {
            sr = sampleRate;
            const int len = (int) std::ceil (maxSeconds * sampleRate) + 64;
            bufL.resize (len); bufR.resize (len);
            maxDelay = (double) len - 16.0;
            timeSmoothed.reset (sampleRate, 0.25);        // slow: tape-style glide
            mixSmoothed.reset (sampleRate, 0.02);
            fbSmoothed.reset (sampleRate, 0.05);
            hpL.setCutoff (60.0f, sampleRate); hpR.setCutoff (60.0f, sampleRate);
            reset();
        }

        void reset()
        {
            bufL.clear(); bufR.clear(); writePos = 0;
            lpL.reset(); lpR.reset(); hpL.reset(); hpR.reset();
            wowPhase = 0.0f; flutterPhase = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, const Params& p)
        {
            const int n = buffer.getNumSamples();
            float* L = buffer.getWritePointer (0);
            float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

            timeSmoothed.setTargetValue ((float) std::clamp ((double) p.timeSeconds * sr, 32.0, maxDelay));
            mixSmoothed.setTargetValue (p.mix);
            fbSmoothed.setTargetValue (p.feedback);
            lpL.setCutoff (p.toneHz, sr); lpR.setCutoff (p.toneHz, sr);

            const float wowInc     = 0.45f / (float) sr;
            const float flutterInc = 5.7f  / (float) sr;
            const float wowDepth   = p.wow * 0.0025f * (float) sr;   // up to 2.5 ms
            const float driveGain  = 1.0f + 6.0f * p.drive;
            const float driveComp  = 1.0f / std::sqrt (driveGain);

            for (int i = 0; i < n; ++i)
            {
                const float inL = L[i];
                const float inR = R != nullptr ? R[i] : inL;

                const float mod = wowDepth * (0.7f * std::sin (2.0f * kPi * wowPhase)
                                            + 0.3f * std::sin (2.0f * kPi * flutterPhase));
                wowPhase += wowInc;         if (wowPhase >= 1.0f) wowPhase -= 1.0f;
                flutterPhase += flutterInc; if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;

                const double d = std::clamp ((double) timeSmoothed.getNextValue() + (double) mod, 16.0, maxDelay);
                const double dR = std::clamp (d + (double) (mod * 0.35f), 16.0, maxDelay);   // slightly different wobble on R

                const float rl = bufL.readDelayed (writePos, d);
                const float rr = bufR.readDelayed (writePos, dR);

                // feedback path: tone -> hp -> saturate
                const float fb = fbSmoothed.getNextValue();
                float fl = hpL.highpass (lpL.lowpass (rl));
                float fr = hpR.highpass (lpR.lowpass (rr));
                fl = softClip (fl * driveGain) * driveComp;
                fr = softClip (fr * driveGain) * driveComp;

                bufL[writePos] = inL + fl * fb;
                bufR[writePos] = inR + fr * fb;
                if (++writePos >= bufL.size()) writePos = 0;

                const float mix = mixSmoothed.getNextValue();
                L[i] = inL * (1.0f - mix) + rl * mix;
                if (R != nullptr) R[i] = inR * (1.0f - mix) + rr * mix;
            }
        }

    private:
        double sr = 44100.0, maxDelay = 1000.0;
        CircularBuffer bufL, bufR;
        int writePos = 0;
        OnePole lpL, lpR, hpL, hpR;
        float wowPhase = 0.0f, flutterPhase = 0.0f;
        juce::SmoothedValue<float> timeSmoothed, mixSmoothed, fbSmoothed;
    };
}
