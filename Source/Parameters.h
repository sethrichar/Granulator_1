#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// ---------------------------------------------------------------------------
// Parameter IDs. Everything the host can automate lives here.
// ---------------------------------------------------------------------------
namespace ParamIDs
{
    inline constexpr int kMaxSteps = 16;

    // Global
    inline constexpr auto output        = "output";

    // Granulator
    inline constexpr auto grTime        = "gr_time";        // ms
    inline constexpr auto grSync        = "gr_sync";
    inline constexpr auto grDiv         = "gr_div";
    inline constexpr auto grScatter     = "gr_scatter";     // % of time
    inline constexpr auto grSize        = "gr_size";        // ms
    inline constexpr auto grSizeRand    = "gr_sizeRand";    // %
    inline constexpr auto grRate        = "gr_rate";        // grains / sec
    inline constexpr auto grRateRand    = "gr_rateRand";    // %
    inline constexpr auto grPitch       = "gr_pitch";       // semitones
    inline constexpr auto grPitchRand   = "gr_pitchRand";   // semitones
    inline constexpr auto grReverse     = "gr_reverse";     // % probability
    inline constexpr auto grShape       = "gr_shape";       // 0 = percussive, 0.5 = symmetric, 1 = swell
    inline constexpr auto grFeedback    = "gr_feedback";    // %
    inline constexpr auto grTone        = "gr_tone";        // -1 (dark) .. +1 (thin)
    inline constexpr auto grSpread      = "gr_spread";      // % stereo
    inline constexpr auto grMix         = "gr_mix";         // %
    inline constexpr auto grFreeze      = "gr_freeze";

    // Step sequencer ("Modifiers")
    inline constexpr auto seqOn         = "seq_on";
    inline constexpr auto seqSteps      = "seq_steps";
    inline constexpr auto seqSync       = "seq_sync";
    inline constexpr auto seqDiv        = "seq_div";
    inline constexpr auto seqRate       = "seq_rate";       // Hz when not synced
    inline constexpr auto seqMode       = "seq_mode";
    inline constexpr auto seqPosRange   = "seq_posRange";   // octaves

    // Lane prefixes. Step params are "<lane>_<n>" with n = 1..16.
    inline constexpr auto lanePitch     = "seq_pitch";
    inline constexpr auto laneDir       = "seq_dir";
    inline constexpr auto lanePos       = "seq_pos";
    inline constexpr auto laneSize      = "seq_size";
    inline constexpr auto laneLevel     = "seq_level";

    inline juce::String laneEnableId (const char* lane)          { return juce::String (lane) + "_on"; }
    inline juce::String stepId (const char* lane, int stepIndex)  { return juce::String (lane) + "_" + juce::String (stepIndex + 1); }

    // Chorus
    inline constexpr auto chPlace       = "ch_place";
    inline constexpr auto chRate        = "ch_rate";
    inline constexpr auto chDepth       = "ch_depth";
    inline constexpr auto chMix         = "ch_mix";

    // Tape delay
    inline constexpr auto tpPlace       = "tp_place";
    inline constexpr auto tpTime        = "tp_time";
    inline constexpr auto tpSync        = "tp_sync";
    inline constexpr auto tpDiv         = "tp_div";
    inline constexpr auto tpFeedback    = "tp_feedback";
    inline constexpr auto tpTone        = "tp_tone";        // Hz
    inline constexpr auto tpWow         = "tp_wow";
    inline constexpr auto tpDrive       = "tp_drive";
    inline constexpr auto tpMix         = "tp_mix";

    // Reverb
    inline constexpr auto rvPlace       = "rv_place";
    inline constexpr auto rvSize        = "rv_size";
    inline constexpr auto rvDamp        = "rv_damp";
    inline constexpr auto rvWidth       = "rv_width";
    inline constexpr auto rvPredelay    = "rv_predelay";    // ms
    inline constexpr auto rvMix         = "rv_mix";
}

// ---------------------------------------------------------------------------
// Tempo-sync note divisions (shared by grain time, tape time and sequencer).
// ---------------------------------------------------------------------------
namespace SyncDivisions
{
    const juce::StringArray& names();
    double beats (int index);          // length in quarter notes
    int    defaultIndex();             // "1/4"
}

namespace Placement
{
    enum { pre = 0, post = 1 };
    const juce::StringArray& names();
}

namespace SeqModes
{
    enum { forward = 0, reverse, pingPong, random };
    const juce::StringArray& names();
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
