// Offline test harness: drives the real processor with synthetic audio and
// checks the DSP behaves (produces output, stays finite, feedback is bounded,
// sequencer steps land where expected). Optionally writes WAVs for listening.
//
//   RuminateTests            run checks
//   RuminateTests <outdir>   run checks and write demo renders to <outdir>

#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include <cstdio>
#include <cmath>

using namespace juce;
namespace P = ParamIDs;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s (%s)\n", msg, #cond); } else std::printf ("ok:   %s\n", msg); } while (0)

struct Harness
{
    RuminateAudioProcessor proc;
    double sr = 48000.0;
    int block = 256;

    Harness()
    {
        proc.setPlayConfigDetails (2, 2, sr, block);
        proc.prepareToPlay (sr, block);
        proc.setTestTransport (true, 120.0, 0.0);
    }

    void set (const String& id, float value)
    {
        auto* p = proc.apvts.getParameter (id);
        jassert (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    // Renders `seconds` of audio, feeding `gen(sampleIndex)` in, returns output.
    template <typename Gen>
    AudioBuffer<float> render (double seconds, Gen gen)
    {
        const int total = (int) (seconds * sr);
        AudioBuffer<float> out (2, total);
        AudioBuffer<float> blk (2, block);
        MidiBuffer midi;
        double ppq = 0.0;
        for (int start = 0; start < total; start += block)
        {
            const int n = jmin (block, total - start);
            blk.setSize (2, n, false, false, true);
            for (int i = 0; i < n; ++i)
            {
                const float v = gen (start + i);
                blk.setSample (0, i, v);
                blk.setSample (1, i, v);
            }
            proc.setTestTransport (true, 120.0, ppq);
            proc.processBlock (blk, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, start, blk, ch, 0, n);
            ppq += n * 120.0 / 60.0 / sr;
        }
        return out;
    }
};

static float rms (const AudioBuffer<float>& b, int startSample = 0, int num = -1)
{
    if (num < 0) num = b.getNumSamples() - startSample;
    double acc = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = startSample; i < startSample + num; ++i)
            acc += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return (float) std::sqrt (acc / jmax (1, num * b.getNumChannels()));
}

static bool allFinite (const AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i))) return false;
    return true;
}

static float peak (const AudioBuffer<float>& b) { return b.getMagnitude (0, b.getNumSamples()); }

static void writeWav (const AudioBuffer<float>& b, const File& f, double sr)
{
    f.deleteFile();
    WavAudioFormat fmt;
    std::unique_ptr<OutputStream> stream = std::make_unique<FileOutputStream> (f);
    auto w = fmt.createWriterFor (stream, AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (w != nullptr) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    File outDir = argc > 1 ? File (argv[1]) : File();
    if (outDir != File()) outDir.createDirectory();

    auto sine = [] (double sr, double hz) { return [=] (int i) { return 0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * hz * i / sr); }; };
    auto burst = [] (double sr) { return [=] (int i) { return i < (int) (0.05 * sr) ? 0.8f * (float) std::sin (2.0 * MathConstants<double>::pi * 330.0 * i / sr) : 0.0f; }; };

    // 1. Bypass-ish: mix 0, no fx -> output equals input.
    {
        Harness h;
        h.set (P::grMix, 0.0f); h.set (P::grFeedback, 0.0f);
        auto out = h.render (1.0, sine (h.sr, 220.0));
        AudioBuffer<float> ref (2, out.getNumSamples());
        for (int i = 0; i < ref.getNumSamples(); ++i) { const float v = sine (h.sr, 220.0) (i); ref.setSample (0, i, v); ref.setSample (1, i, v); }
        float maxDiff = 0.0f;
        for (int i = 2000; i < out.getNumSamples(); ++i)
            maxDiff = jmax (maxDiff, std::abs (out.getSample (0, i) - ref.getSample (0, i)));
        CHECK (maxDiff < 1.0e-3f, "dry path is transparent at grain mix 0");
    }

    // 2. Grains produce output after the delay time and stay finite.
    {
        Harness h;
        h.set (P::grMix, 100.0f); h.set (P::grTime, 300.0f); h.set (P::grFeedback, 0.0f);
        h.set (P::grRate, 20.0f); h.set (P::grSize, 100.0f);
        auto out = h.render (1.5, burst (h.sr));
        CHECK (allFinite (out), "wet output is finite");
        const float before = rms (out, (int) (0.10 * h.sr), (int) (0.15 * h.sr));
        const float after  = rms (out, (int) (0.32 * h.sr), (int) (0.15 * h.sr));
        CHECK (before < 1.0e-4f, "nothing before the delay time (input burst is 50 ms)");
        CHECK (after > 0.01f, "grains audible after delay time");
        if (outDir != File()) writeWav (out, outDir.getChildFile ("02_burst_grains.wav"), h.sr);
    }

    // 3. Full feedback + long render must not blow up (soft clipper bounds it).
    {
        Harness h;
        h.set (P::grMix, 100.0f); h.set (P::grFeedback, 100.0f); h.set (P::grTime, 120.0f);
        h.set (P::grRate, 40.0f); h.set (P::grSize, 200.0f); h.set (P::grPitch, 7.0f); h.set (P::grScatter, 30.0f);
        auto out = h.render (8.0, sine (h.sr, 110.0));
        CHECK (allFinite (out), "100% feedback stays finite");
        CHECK (peak (out) < 4.0f, "100% feedback stays bounded");
        if (outDir != File()) writeWav (out, outDir.getChildFile ("03_feedback_cloud.wav"), h.sr);
    }

    // 4. Pitch shift: +12 st on a sine should put energy near 2x frequency.
    {
        Harness h;
        h.set (P::grMix, 100.0f); h.set (P::grFeedback, 0.0f); h.set (P::grTime, 200.0f);
        // Low rate + short grains so grains don't overlap: this measures the pitch of
        // individual grains. (Overlapping regular grains add the usual granular
        // sideband offset, which is by design.)
        h.set (P::grRate, 2.0f); h.set (P::grSize, 150.0f); h.set (P::grPitch, 12.0f); h.set (P::grSpread, 0.0f);
        const double f0 = 440.0;
        auto out = h.render (2.0, sine (h.sr, f0));
        // crude Goertzel at f0 and 2*f0 over the last second
        // Goertzel energy summed over a +-6 Hz band around `centre`, last second only.
        auto goertzel = [&] (double centre)
        {
            const int N = (int) h.sr; const int start = out.getNumSamples() - N;
            double total = 0.0;
            for (double hz = centre - 6.0; hz <= centre + 6.0; hz += 1.0)
            {
                const double w = 2.0 * MathConstants<double>::pi * hz / h.sr;
                double s0 = 0, s1 = 0, s2 = 0;
                for (int i = 0; i < N; ++i) { s0 = out.getSample (0, start + i) + 2.0 * std::cos (w) * s1 - s2; s2 = s1; s1 = s0; }
                total += std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - 2.0 * std::cos (w) * s1 * s2));
            }
            return total;
        };
        if (outDir != File()) writeWav (out, outDir.getChildFile ("04_pitch_octave.wav"), h.sr);
        const double atF0 = goertzel (f0), at2F0 = goertzel (2.0 * f0);
        std::printf ("      energy @f0 = %.1f  @2f0 = %.1f\n", atF0, at2F0);
        CHECK (at2F0 > 4.0 * atF0, "+12 st grains land an octave up");
    }

    // 5. Sequencer level lane: rests on even steps should leave gaps.
    {
        Harness h;
        h.set (P::grMix, 100.0f); h.set (P::grFeedback, 0.0f); h.set (P::grTime, 50.0f);
        h.set (P::grRate, 60.0f); h.set (P::grSize, 20.0f); h.set (P::grSpread, 0.0f);
        h.set (P::seqOn, 1.0f); h.set (P::seqSteps, 4.0f); h.set (P::seqSync, 1.0f); h.set (P::seqDiv, 9.0f); // 1/4 @120 = 0.5 s
        h.set (P::laneEnableId (P::laneLevel), 1.0f);
        for (int i = 0; i < 4; ++i) h.set (P::stepId (P::laneLevel, i), (i % 2 == 0) ? 1.0f : 0.0f);
        auto out = h.render (2.0, sine (h.sr, 220.0));
        const float onStep  = rms (out, (int) (0.15 * h.sr), (int) (0.25 * h.sr));   // step 0 (0.0-0.5 s)
        const float offStep = rms (out, (int) (0.65 * h.sr), (int) (0.25 * h.sr));   // step 1 (0.5-1.0 s)
        std::printf ("      on-step rms = %.4f  rest-step rms = %.4f\n", onStep, offStep);
        CHECK (onStep > 0.02f, "level lane: active step sounds");
        CHECK (offStep < onStep * 0.05f, "level lane: rest step is (nearly) silent");
        if (outDir != File()) writeWav (out, outDir.getChildFile ("05_seq_level_gate.wav"), h.sr);
    }

    // 6. Pre/post routing + all effects on: finite, non-silent, and different.
    {
        auto run = [&] (bool pre)
        {
            Harness h;
            h.set (P::grMix, 60.0f); h.set (P::grFeedback, 40.0f);
            h.set (P::chMix, 50.0f); h.set (P::tpMix, 50.0f); h.set (P::rvMix, 50.0f);
            h.set (P::chPlace, pre ? 0.0f : 1.0f); h.set (P::tpPlace, pre ? 0.0f : 1.0f); h.set (P::rvPlace, pre ? 0.0f : 1.0f);
            return h.render (3.0, burst (h.sr));
        };
        auto a = run (true), b = run (false);
        CHECK (allFinite (a) && allFinite (b), "fx chain finite in both placements");
        CHECK (rms (a) > 1.0e-3f && rms (b) > 1.0e-3f, "fx chain produces output");
        float diff = 0.0f;
        for (int i = 0; i < a.getNumSamples(); ++i) diff = jmax (diff, std::abs (a.getSample (0, i) - b.getSample (0, i)));
        CHECK (diff > 1.0e-3f, "pre and post placement actually differ");
        if (outDir != File()) { writeWav (a, outDir.getChildFile ("06_fx_pre.wav"), 48000.0); writeWav (b, outDir.getChildFile ("06_fx_post.wav"), 48000.0); }
    }

    // 7. Tape delay self-oscillation stays bounded.
    {
        Harness h;
        h.set (P::grMix, 0.0f); h.set (P::tpMix, 100.0f); h.set (P::tpFeedback, 100.0f); h.set (P::tpDrive, 100.0f);
        auto out = h.render (6.0, burst (h.sr));
        CHECK (allFinite (out) && peak (out) < 4.0f, "tape delay at 100% feedback stays bounded");
    }

    // 8. State round-trip.
    {
        Harness h;
        h.set (P::grPitch, -5.0f); h.set (P::stepId (P::lanePitch, 3), 7.0f);
        MemoryBlock mb; h.proc.getStateInformation (mb);
        Harness h2; h2.proc.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::abs (h2.proc.apvts.getRawParameterValue (P::grPitch)->load() + 5.0f) < 1e-4f, "state restores knob");
        CHECK (std::abs (h2.proc.apvts.getRawParameterValue (P::stepId (P::lanePitch, 3))->load() - 7.0f) < 1e-4f, "state restores step");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
