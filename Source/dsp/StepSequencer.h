#pragma once

#include "DspUtils.h"
#include <array>

namespace dsp
{
    inline constexpr int kMaxSteps = 16;

    struct Lane
    {
        bool enabled = false;
        std::array<float, kMaxSteps> values {};
    };

    // Everything the sequencer needs for one block. Filled from parameters
    // by the processor, read by the grain engine at grain-spawn time.
    struct SequencerSettings
    {
        bool  enabled     = false;
        int   numSteps    = 8;
        int   mode        = 0;       // SeqModes
        float posRangeOct = 1.0f;

        Lane pitch, direction, position, size, level;
    };

    // The snapshot a grain takes of the sequencer when it is born.
    struct StepModifiers
    {
        bool  pitchOn = false;  float pitchSemis  = 0.0f;
        bool  dirOn   = false;  bool  reverse     = false;
        bool  posOn   = false;  float posOctaves  = 0.0f;   // already scaled by posRange
        bool  sizeOn  = false;  float sizeOctaves = 0.0f;
        bool  levelOn = false;  float level       = 1.0f;
    };

    // Turns time (either host PPQ or a free-running phase) into a step index.
    class StepClock
    {
    public:
        void prepare (double sr) { sampleRate = sr; freePhase = 0.0; lastTick = -1; }

        // Call once per block.
        //   hostSynced: use ppq; otherwise advance an internal phase at stepSeconds.
        void beginBlock (bool hostSynced, double ppqAtBlockStart, double bpm, double stepBeats, double stepSeconds)
        {
            synced       = hostSynced;
            ppqStart     = ppqAtBlockStart;
            ppqPerSample = bpm / 60.0 / sampleRate;
            beatsPerStep = std::max (1.0e-6, stepBeats);
            phaseInc     = 1.0 / std::max (1.0e-4, stepSeconds * sampleRate);
        }

        // Absolute tick count at a sample offset inside the block.
        long long tickAt (int sampleOffset) const
        {
            if (synced)
                return (long long) std::floor ((ppqStart + ppqPerSample * sampleOffset) / beatsPerStep);
            return (long long) std::floor (freePhase + phaseInc * sampleOffset);
        }

        // Call at the end of each block (only matters in free-run mode).
        void endBlock (int numSamples)
        {
            if (! synced)
            {
                freePhase += phaseInc * numSamples;
                if (freePhase > 1.0e9) freePhase -= 1.0e9;   // keep it bounded
            }
        }

        // Map a tick to a step index for the given mode.
        int stepForTick (long long tick, int numSteps, int mode, FastRandom& rng)
        {
            numSteps = std::max (1, numSteps);
            const auto t = tick < 0 ? -tick : tick;

            switch (mode)
            {
                default:
                case 0: return (int) (t % numSteps);
                case 1: return numSteps - 1 - (int) (t % numSteps);
                case 2:
                {
                    const int period = std::max (1, 2 * numSteps - 2);
                    const int i = (int) (t % period);
                    return i < numSteps ? i : period - i;
                }
                case 3:
                    if (tick != lastTick)
                    {
                        lastTick = tick;
                        randomStep = rng.below (numSteps);
                    }
                    return std::min (randomStep, numSteps - 1);
            }
        }

    private:
        double sampleRate = 44100.0;
        bool   synced = false;
        double ppqStart = 0.0, ppqPerSample = 0.0, beatsPerStep = 1.0;
        double freePhase = 0.0, phaseInc = 0.0;
        long long lastTick = -1;
        int randomStep = 0;
    };

    inline StepModifiers snapshot (const SequencerSettings& s, int step)
    {
        StepModifiers m;
        if (! s.enabled) return m;
        step = std::clamp (step, 0, kMaxSteps - 1);

        m.pitchOn = s.pitch.enabled;         m.pitchSemis  = s.pitch.values[(size_t) step];
        m.dirOn   = s.direction.enabled;     m.reverse     = s.direction.values[(size_t) step] > 0.5f;
        m.posOn   = s.position.enabled;      m.posOctaves  = s.position.values[(size_t) step] * s.posRangeOct;
        m.sizeOn  = s.size.enabled;          m.sizeOctaves = s.size.values[(size_t) step] * 2.0f;   // +-2 octaves
        m.levelOn = s.level.enabled;         m.level       = s.level.values[(size_t) step];
        return m;
    }
}
