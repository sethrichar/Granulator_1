#include "GrainEngine.h"

namespace dsp
{
    void GrainEngine::prepare (double sampleRate, int /*maxBlockSize*/, double maxBufferSeconds)
    {
        sr = sampleRate;
        bufferLengthSeconds = maxBufferSeconds;
        const int len = (int) std::ceil (maxBufferSeconds * sampleRate) + 16;
        bufL.resize (len);
        bufR.resize (len);
        mixSmoothed.reset (sampleRate, 0.02);
        feedbackSmoothed.reset (sampleRate, 0.05);
        rng.seed (0xC0FFEE11u);
        reset();
    }

    void GrainEngine::reset()
    {
        bufL.clear();
        bufR.clear();
        writePos = 0;
        for (auto& g : grains) g.active = false;
        samplesToNextGrain = 0;
        toneLpL.reset(); toneLpR.reset(); toneHpL.reset(); toneHpR.reset();
        fbL = fbR = 0.0f;
        activeCount.store (0);
    }

    float GrainEngine::window (float phase, float attack) const
    {
        if (phase < attack)
            return 0.5f - 0.5f * std::cos (kPi * phase / attack);
        return 0.5f + 0.5f * std::cos (kPi * (phase - attack) / (1.0f - attack));
    }

    void GrainEngine::spawn (const Params& p, const StepModifiers& mod)
    {
        // --- per-grain randomisation + step modifiers ------------------------
        float level = mod.levelOn ? mod.level : 1.0f;
        if (level <= 0.0005f)
            return;                                   // rest step: no grain

        double delaySec = p.timeSeconds;
        if (mod.posOn)  delaySec *= std::exp2 ((double) mod.posOctaves);
        if (p.scatter > 0.0f) delaySec += rng.bipolar() * p.scatter * delaySec;

        double sizeSec = p.sizeSeconds;
        if (p.sizeRand > 0.0f) sizeSec *= std::exp2 ((double) (rng.bipolar() * p.sizeRand));
        if (mod.sizeOn) sizeSec *= std::exp2 ((double) mod.sizeOctaves);
        sizeSec = std::clamp (sizeSec, 0.002, bufferLengthSeconds * 0.25);

        float semis = p.pitchSemis;
        if (p.pitchRand > 0.0f) semis += rng.bipolar() * p.pitchRand;
        if (mod.pitchOn) semis += mod.pitchSemis;
        const double ratio = (double) semitonesToRatio (semis);

        const bool reverse = mod.dirOn ? mod.reverse : (rng.unipolar() < p.reverseProb);

        // --- keep the read head inside valid, already-written audio ----------
        const double lengthSamples = std::max (8.0, sizeSec * sr);
        const int    bufLen        = bufL.size();
        double delaySamples        = delaySec * sr;

        double minDelay = 4.0;                        // interpolation guard
        if (! reverse) minDelay += std::max (0.0, ratio - 1.0) * lengthSamples;
        double maxDelay = (double) bufLen - 8.0;
        if (reverse) maxDelay -= ratio * lengthSamples;
        if (maxDelay < minDelay) maxDelay = minDelay;
        delaySamples = std::clamp (delaySamples, minDelay, maxDelay);

        // --- find a voice (steal the most-finished one if full) --------------
        Grain* g = nullptr;
        float bestPhase = -1.0f;
        for (auto& v : grains)
        {
            if (! v.active) { g = &v; break; }
            if (v.phase > bestPhase) { bestPhase = v.phase; g = &v; }
        }
        if (g == nullptr) return;

        double start = (double) writePos - delaySamples;
        while (start < 0.0) start += (double) bufLen;
        while (start >= (double) bufLen) start -= (double) bufLen;

        g->active   = true;
        g->readPos  = start;
        g->inc      = reverse ? -ratio : ratio;
        g->phase    = 0.0f;
        g->phaseInc = (float) (1.0 / lengthSamples);
        g->attack   = std::clamp (0.02f + 0.96f * p.shape, 0.02f, 0.98f);

        // Overlap compensation so dense clouds don't get louder than sparse ones.
        const float overlap = std::max (1.0f, p.rateHz * p.sizeSeconds);
        g->amp = level / std::sqrt (overlap);

        panGains (rng.bipolar() * p.spread, g->gainL, g->gainR);
    }

    void GrainEngine::process (juce::AudioBuffer<float>& buffer, const Params& p,
                               const SequencerSettings& seq, StepClock& clock)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = buffer.getNumChannels();
        if (numSamples == 0 || numCh == 0) return;

        float* outL = buffer.getWritePointer (0);
        float* outR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

        // Tone: negative = lowpass sweeps down, positive = highpass sweeps up.
        const float lpHz = p.tone < 0.0f ? 20000.0f * std::pow (200.0f / 20000.0f, -p.tone) : 20000.0f;
        const float hpHz = p.tone > 0.0f ? 20.0f * std::pow (4000.0f / 20.0f, p.tone) : 20.0f;
        toneLpL.setCutoff (lpHz, sr); toneLpR.setCutoff (lpHz, sr);
        toneHpL.setCutoff (hpHz, sr); toneHpR.setCutoff (hpHz, sr);

        mixSmoothed.setTargetValue (p.mix);
        feedbackSmoothed.setTargetValue (p.feedback);

        const int bufLen = bufL.size();
        const double dBufLen = (double) bufLen;
        int active = 0;
        int stepShown = lastStepIndex.load (std::memory_order_relaxed);

        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = outL[n];
            const float inR = outR != nullptr ? outR[n] : inL;

            // ---- spawn scheduling ------------------------------------------
            if (--samplesToNextGrain <= 0)
            {
                const int step = seq.enabled ? clock.stepForTick (clock.tickAt (n), seq.numSteps, seq.mode, rng) : 0;
                stepShown = step;
                spawn (p, snapshot (seq, step));

                double interval = sr / std::max (0.05f, p.rateHz);
                if (p.rateRand > 0.0f) interval *= 1.0 + 0.9 * rng.bipolar() * p.rateRand;
                samplesToNextGrain = std::max (1, (int) interval);
            }

            // ---- render grains ---------------------------------------------
            float wetL = 0.0f, wetR = 0.0f;
            active = 0;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                ++active;

                const float w = window (g.phase, g.attack) * g.amp;
                wetL += bufL.readCubic (g.readPos) * w * g.gainL;
                wetR += bufR.readCubic (g.readPos) * w * g.gainR;

                g.readPos += g.inc;
                if (g.readPos >= dBufLen) g.readPos -= dBufLen;
                else if (g.readPos < 0.0) g.readPos += dBufLen;

                g.phase += g.phaseInc;
                if (g.phase >= 1.0f) g.active = false;
            }

            // ---- tone + feedback -------------------------------------------
            wetL = toneLpL.lowpass (toneHpL.highpass (wetL));
            wetR = toneLpR.lowpass (toneHpR.highpass (wetR));

            const float fbAmt = feedbackSmoothed.getNextValue();
            fbL = softClip (wetL * fbAmt);
            fbR = softClip (wetR * fbAmt);

            // ---- write into memory -----------------------------------------
            if (! p.freeze)
            {
                bufL[writePos] = inL + fbL;
                bufR[writePos] = inR + fbR;
            }
            if (++writePos >= bufLen) writePos = 0;

            // ---- mix -------------------------------------------------------
            const float mix = mixSmoothed.getNextValue();
            const float dry = 1.0f - mix;
            outL[n] = inL * dry + wetL * mix;
            if (outR != nullptr) outR[n] = inR * dry + wetR * mix;
        }

        clock.endBlock (numSamples);
        activeCount.store (active, std::memory_order_relaxed);
        lastStepIndex.store (stepShown, std::memory_order_relaxed);
    }
}
