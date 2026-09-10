#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
namespace P = ParamIDs;

// ---------------------------------------------------------------------------
// Cached raw parameter pointers so processBlock never does string lookups.
// ---------------------------------------------------------------------------
struct RuminateAudioProcessor::RawParams
{
    using A = std::atomic<float>*;

    explicit RawParams (AudioProcessorValueTreeState& s)
    {
        auto get = [&s] (const String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };

        output     = get (P::output);
        grTime     = get (P::grTime);     grSync     = get (P::grSync);     grDiv      = get (P::grDiv);
        grScatter  = get (P::grScatter);  grSize     = get (P::grSize);     grSizeRand = get (P::grSizeRand);
        grRate     = get (P::grRate);     grRateRand = get (P::grRateRand); grPitch    = get (P::grPitch);
        grPitchRand= get (P::grPitchRand);grReverse  = get (P::grReverse);  grShape    = get (P::grShape);
        grFeedback = get (P::grFeedback); grTone     = get (P::grTone);     grSpread   = get (P::grSpread);
        grMix      = get (P::grMix);      grFreeze   = get (P::grFreeze);

        seqOn = get (P::seqOn); seqSteps = get (P::seqSteps); seqSync = get (P::seqSync); seqDiv = get (P::seqDiv);
        seqRate = get (P::seqRate); seqMode = get (P::seqMode); seqPosRange = get (P::seqPosRange);

        const char* laneIds[kNumLanes] = { P::lanePitch, P::laneDir, P::lanePos, P::laneSize, P::laneLevel };
        for (int l = 0; l < kNumLanes; ++l)
        {
            laneOn[l] = get (P::laneEnableId (laneIds[l]));
            for (int i = 0; i < P::kMaxSteps; ++i)
                lane[l][i] = get (P::stepId (laneIds[l], i));
        }

        chPlace = get (P::chPlace); chRate = get (P::chRate); chDepth = get (P::chDepth); chMix = get (P::chMix);

        tpPlace = get (P::tpPlace); tpTime = get (P::tpTime); tpSync = get (P::tpSync); tpDiv = get (P::tpDiv);
        tpFeedback = get (P::tpFeedback); tpTone = get (P::tpTone); tpWow = get (P::tpWow); tpDrive = get (P::tpDrive);
        tpMix = get (P::tpMix);

        rvPlace = get (P::rvPlace); rvSize = get (P::rvSize); rvDamp = get (P::rvDamp); rvWidth = get (P::rvWidth);
        rvPredelay = get (P::rvPredelay); rvMix = get (P::rvMix);
    }

    static constexpr int kNumLanes = 5;
    enum { pitchLane = 0, dirLane, posLane, sizeLane, levelLane };

    A output;
    A grTime, grSync, grDiv, grScatter, grSize, grSizeRand, grRate, grRateRand, grPitch, grPitchRand,
      grReverse, grShape, grFeedback, grTone, grSpread, grMix, grFreeze;
    A seqOn, seqSteps, seqSync, seqDiv, seqRate, seqMode, seqPosRange;
    A laneOn[kNumLanes];
    A lane[kNumLanes][P::kMaxSteps];
    A chPlace, chRate, chDepth, chMix;
    A tpPlace, tpTime, tpSync, tpDiv, tpFeedback, tpTone, tpWow, tpDrive, tpMix;
    A rvPlace, rvSize, rvDamp, rvWidth, rvPredelay, rvMix;
};

// ---------------------------------------------------------------------------
RuminateAudioProcessor::RuminateAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RuminateState", createParameterLayout())
{
    raw = std::make_unique<RawParams> (apvts);
}

RuminateAudioProcessor::~RuminateAudioProcessor() = default;

bool RuminateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    if (in  != AudioChannelSet::mono() && in  != AudioChannelSet::stereo()) return false;
    return true;
}

void RuminateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock, 60.0);
    clock.prepare (sampleRate);
    chorus.prepare (sampleRate);
    tape.prepare (sampleRate, 4.0);
    reverb.prepare (sampleRate, samplesPerBlock);
    outputGain.reset (sampleRate, 0.02);
    stereoScratch.setSize (2, samplesPerBlock);
}

RuminateAudioProcessor::Transport RuminateAudioProcessor::readTransport (int /*numSamples*/)
{
    if (testTransport.valid)
        return testTransport;

    Transport t;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            t.valid = true;
            t.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) t.bpm = jmax (20.0, *bpm);
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq;
        }
    }
    return t;
}

void RuminateAudioProcessor::buildSequencerSettings (dsp::SequencerSettings& s) const
{
    const auto& r = *raw;
    s.enabled     = r.seqOn->load() > 0.5f;
    s.numSteps    = jlimit (1, P::kMaxSteps, (int) std::lround (r.seqSteps->load()));
    s.mode        = (int) std::lround (r.seqMode->load());
    s.posRangeOct = r.seqPosRange->load();

    dsp::Lane* lanes[RawParams::kNumLanes] = { &s.pitch, &s.direction, &s.position, &s.size, &s.level };
    for (int l = 0; l < RawParams::kNumLanes; ++l)
    {
        lanes[l]->enabled = r.laneOn[l]->load() > 0.5f;
        for (int i = 0; i < P::kMaxSteps; ++i)
            lanes[l]->values[(size_t) i] = r.lane[l][i]->load();
    }
}

void RuminateAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const auto& r = *raw;
    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    if (numSamples == 0) return;

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    // Work internally in stereo so pans/widths behave even with a mono bus.
    stereoScratch.setSize (2, numSamples, false, false, true);
    stereoScratch.copyFrom (0, 0, buffer, 0, 0, numSamples);
    stereoScratch.copyFrom (1, 0, buffer, numIn > 1 ? 1 : 0, 0, numSamples);

    const auto transport = readTransport (numSamples);
    const double secondsPerBeat = 60.0 / transport.bpm;

    // ---- Effects params ----------------------------------------------------
    dsp::Chorus::Params cp;
    cp.rateHz = r.chRate->load(); cp.depth = r.chDepth->load() * 0.01f; cp.mix = r.chMix->load() * 0.01f;

    dsp::TapeDelay::Params tp;
    tp.timeSeconds = r.tpSync->load() > 0.5f
                       ? (float) (SyncDivisions::beats ((int) r.tpDiv->load()) * secondsPerBeat)
                       : r.tpTime->load() * 0.001f;
    tp.feedback = r.tpFeedback->load() * 0.01f; tp.toneHz = r.tpTone->load();
    tp.wow = r.tpWow->load() * 0.01f; tp.drive = r.tpDrive->load() * 0.01f; tp.mix = r.tpMix->load() * 0.01f;

    dsp::ReverbFx::Params rp;
    rp.size = r.rvSize->load() * 0.01f; rp.damp = r.rvDamp->load() * 0.01f; rp.width = r.rvWidth->load() * 0.01f;
    rp.predelaySeconds = r.rvPredelay->load() * 0.001f; rp.mix = r.rvMix->load() * 0.01f;

    const bool chorusPre = r.chPlace->load() < 0.5f;
    const bool tapePre   = r.tpPlace->load() < 0.5f;
    const bool reverbPre = r.rvPlace->load() < 0.5f;

    // ---- Granulator params -------------------------------------------------
    dsp::GrainEngine::Params gp;
    gp.timeSeconds = r.grSync->load() > 0.5f
                       ? (float) (SyncDivisions::beats ((int) r.grDiv->load()) * secondsPerBeat)
                       : r.grTime->load() * 0.001f;
    gp.scatter     = r.grScatter->load() * 0.01f;
    gp.sizeSeconds = r.grSize->load() * 0.001f;
    gp.sizeRand    = r.grSizeRand->load() * 0.01f;
    gp.rateHz      = r.grRate->load();
    gp.rateRand    = r.grRateRand->load() * 0.01f;
    gp.pitchSemis  = r.grPitch->load();
    gp.pitchRand   = r.grPitchRand->load();
    gp.reverseProb = r.grReverse->load() * 0.01f;
    gp.shape       = r.grShape->load();
    gp.feedback    = r.grFeedback->load() * 0.01f;
    gp.tone        = r.grTone->load();
    gp.spread      = r.grSpread->load() * 0.01f;
    gp.mix         = r.grMix->load() * 0.01f;
    gp.freeze      = r.grFreeze->load() > 0.5f;

    // ---- Sequencer clock ---------------------------------------------------
    buildSequencerSettings (seqSettings);
    const bool seqSync   = r.seqSync->load() > 0.5f;
    const double stepBeats   = SyncDivisions::beats ((int) r.seqDiv->load());
    const double stepSeconds = seqSync ? stepBeats * secondsPerBeat : 1.0 / jmax (0.01f, r.seqRate->load());
    const bool hostLocked = seqSync && transport.valid && transport.playing;
    clock.beginBlock (hostLocked, transport.ppq, transport.bpm, stepBeats, stepSeconds);

    // ---- The chain ---------------------------------------------------------
    if (chorusPre) chorus.process (stereoScratch, cp);
    if (tapePre)   tape.process   (stereoScratch, tp);
    if (reverbPre) reverb.process (stereoScratch, rp);

    engine.process (stereoScratch, gp, seqSettings, clock);

    if (! chorusPre) chorus.process (stereoScratch, cp);
    if (! tapePre)   tape.process   (stereoScratch, tp);
    if (! reverbPre) reverb.process (stereoScratch, rp);

    // ---- Output ------------------------------------------------------------
    outputGain.setTargetValue (Decibels::decibelsToGain (r.output->load()));
    if (numOut >= 2)
    {
        buffer.copyFrom (0, 0, stereoScratch, 0, 0, numSamples);
        buffer.copyFrom (1, 0, stereoScratch, 1, 0, numSamples);
    }
    else if (numOut == 1)
    {
        buffer.copyFrom (0, 0, stereoScratch, 0, 0, numSamples);
        buffer.addFrom  (0, 0, stereoScratch, 1, 0, numSamples);
        buffer.applyGain (0.5f);
    }
    outputGain.applyGain (buffer, numSamples);
}

// ---------------------------------------------------------------------------
void RuminateAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RuminateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* RuminateAudioProcessor::createEditor()
{
    return new RuminateAudioProcessorEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RuminateAudioProcessor();
}
