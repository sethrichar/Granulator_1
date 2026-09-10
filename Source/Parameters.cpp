#include "Parameters.h"

using namespace juce;

namespace
{
    struct Division { const char* name; double beats; };

    const Division kDivisions[] =
    {
        { "1/64",   0.0625 },
        { "1/32",   0.125  },
        { "1/16T",  1.0 / 6.0 },
        { "1/16",   0.25   },
        { "1/8T",   1.0 / 3.0 },
        { "1/16D",  0.375  },
        { "1/8",    0.5    },
        { "1/4T",   2.0 / 3.0 },
        { "1/8D",   0.75   },
        { "1/4",    1.0    },
        { "1/2T",   4.0 / 3.0 },
        { "1/4D",   1.5    },
        { "1/2",    2.0    },
        { "1/2D",   3.0    },
        { "1 bar",  4.0    },
        { "2 bars", 8.0    },
        { "4 bars", 16.0   },
    };

    NormalisableRange<float> logRange (float lo, float hi, float interval = 0.01f)
    {
        NormalisableRange<float> r (lo, hi, interval);
        r.setSkewForCentre (std::sqrt (lo * hi));
        return r;
    }

    auto percent (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                     NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                                     AudioParameterFloatAttributes().withLabel ("%"));
    }

    auto choice (const String& id, const String& name, const StringArray& items, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def);
    }

    auto toggle (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    }

    auto floatParam (const String& id, const String& name, NormalisableRange<float> range, float def, const String& label = {})
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                     AudioParameterFloatAttributes().withLabel (label));
    }
}

const StringArray& SyncDivisions::names()
{
    static const StringArray n = []
    {
        StringArray a;
        for (auto& d : kDivisions) a.add (d.name);
        return a;
    }();
    return n;
}

double SyncDivisions::beats (int index)
{
    index = jlimit (0, (int) std::size (kDivisions) - 1, index);
    return kDivisions[index].beats;
}

int SyncDivisions::defaultIndex()
{
    return 9; // "1/4"
}

const StringArray& Placement::names()
{
    static const StringArray n { "Pre", "Post" };
    return n;
}

const StringArray& SeqModes::names()
{
    static const StringArray n { "Forward", "Reverse", "Ping-Pong", "Random" };
    return n;
}

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamIDs;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- Global -----------------------------------------------------------
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("global", "Global", "|");
        g->addChild (floatParam (output, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, "dB"));
        layout.add (std::move (g));
    }

    // ---- Granulator -------------------------------------------------------
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("grain", "Grain", "|");
        g->addChild (floatParam (grTime,      "Time",         logRange (1.0f, 60000.0f, 0.1f), 350.0f, "ms"));
        g->addChild (toggle     (grSync,      "Time Sync",    false));
        g->addChild (choice     (grDiv,       "Time Div",     SyncDivisions::names(), SyncDivisions::defaultIndex()));
        g->addChild (percent    (grScatter,   "Scatter",      0.0f));
        g->addChild (floatParam (grSize,      "Size",         logRange (5.0f, 2000.0f, 0.1f), 120.0f, "ms"));
        g->addChild (percent    (grSizeRand,  "Size Rand",    0.0f));
        g->addChild (floatParam (grRate,      "Rate",         logRange (0.2f, 100.0f), 12.0f, "Hz"));
        g->addChild (percent    (grRateRand,  "Rate Rand",    0.0f));
        g->addChild (floatParam (grPitch,     "Pitch",        NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, "st"));
        g->addChild (floatParam (grPitchRand, "Pitch Rand",   NormalisableRange<float> (0.0f, 12.0f, 0.01f), 0.0f, "st"));
        g->addChild (percent    (grReverse,   "Reverse",      0.0f));
        g->addChild (floatParam (grShape,     "Shape",        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
        g->addChild (percent    (grFeedback,  "Feedback",     20.0f));
        g->addChild (floatParam (grTone,      "Tone",         NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
        g->addChild (percent    (grSpread,    "Spread",       30.0f));
        g->addChild (percent    (grMix,       "Grain Mix",    50.0f));
        g->addChild (toggle     (grFreeze,    "Freeze",       false));
        layout.add (std::move (g));
    }

    // ---- Sequencer --------------------------------------------------------
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("seq", "Sequencer", "|");
        g->addChild (toggle (seqOn, "Seq On", false));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { seqSteps, 1 }, "Seq Steps", 1, kMaxSteps, 8));
        g->addChild (toggle (seqSync, "Seq Sync", true));
        g->addChild (choice (seqDiv, "Seq Div", SyncDivisions::names(), 6 /* 1/8 */));
        g->addChild (floatParam (seqRate, "Seq Rate", logRange (0.1f, 20.0f), 4.0f, "Hz"));
        g->addChild (choice (seqMode, "Seq Mode", SeqModes::names(), SeqModes::forward));
        g->addChild (floatParam (seqPosRange, "Pos Range", NormalisableRange<float> (0.0f, 4.0f, 0.01f), 1.0f, "oct"));

        struct Lane { const char* id; const char* name; NormalisableRange<float> range; float def; String label; bool isBool; };
        const Lane lanes[] =
        {
            { lanePitch, "Pitch",     NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, "st", false },
            { laneDir,   "Direction", {},                                             0.0f, {},   true  },
            { lanePos,   "Position",  NormalisableRange<float> (-1.0f, 1.0f, 0.01f),  0.0f, {},   false },
            { laneSize,  "Size",      NormalisableRange<float> (-1.0f, 1.0f, 0.01f),  0.0f, {},   false },
            { laneLevel, "Level",     NormalisableRange<float> (0.0f, 1.0f, 0.01f),   1.0f, {},   false },
        };

        for (auto& lane : lanes)
        {
            auto lg = std::make_unique<AudioProcessorParameterGroup> (lane.id, String ("Seq ") + lane.name, "|");
            lg->addChild (toggle (laneEnableId (lane.id), String ("Seq ") + lane.name + " On", false));

            for (int i = 0; i < kMaxSteps; ++i)
            {
                const auto name = String ("Seq ") + lane.name + " " + String (i + 1);
                if (lane.isBool)
                    lg->addChild (toggle (stepId (lane.id, i), name, false));
                else
                    lg->addChild (floatParam (stepId (lane.id, i), name, lane.range, lane.def, lane.label));
            }
            g->addChild (std::move (lg));
        }
        layout.add (std::move (g));
    }

    // ---- Chorus -----------------------------------------------------------
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("chorus", "Chorus", "|");
        g->addChild (choice     (chPlace, "Chorus Place", Placement::names(), Placement::post));
        g->addChild (floatParam (chRate,  "Chorus Rate",  logRange (0.05f, 10.0f), 0.6f, "Hz"));
        g->addChild (percent    (chDepth, "Chorus Depth", 40.0f));
        g->addChild (percent    (chMix,   "Chorus Mix",   0.0f));
        layout.add (std::move (g));
    }

    // ---- Tape delay -------------------------------------------------------
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("tape", "Tape", "|");
        g->addChild (choice     (tpPlace,    "Tape Place",    Placement::names(), Placement::post));
        g->addChild (floatParam (tpTime,     "Tape Time",     logRange (10.0f, 2000.0f, 0.1f), 420.0f, "ms"));
        g->addChild (toggle     (tpSync,     "Tape Sync",     false));
        g->addChild (choice     (tpDiv,      "Tape Div",      SyncDivisions::names(), 8 /* 1/8D */));
        g->addChild (percent    (tpFeedback, "Tape Feedback", 35.0f));
        g->addChild (floatParam (tpTone,     "Tape Tone",     logRange (500.0f, 16000.0f, 1.0f), 4500.0f, "Hz"));
        g->addChild (percent    (tpWow,      "Tape Wow",      20.0f));
        g->addChild (percent    (tpDrive,    "Tape Drive",    20.0f));
        g->addChild (percent    (tpMix,      "Tape Mix",      0.0f));
        layout.add (std::move (g));
    }

    // ---- Reverb -----------------------------------------------------------
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("reverb", "Reverb", "|");
        g->addChild (choice     (rvPlace,    "Reverb Place",    Placement::names(), Placement::post));
        g->addChild (percent    (rvSize,     "Reverb Size",     70.0f));
        g->addChild (percent    (rvDamp,     "Reverb Damp",     40.0f));
        g->addChild (percent    (rvWidth,    "Reverb Width",    100.0f));
        g->addChild (floatParam (rvPredelay, "Reverb Predelay", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 10.0f, "ms"));
        g->addChild (percent    (rvMix,      "Reverb Mix",      0.0f));
        layout.add (std::move (g));
    }

    return layout;
}
