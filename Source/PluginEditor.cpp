#include "PluginEditor.h"

using namespace juce;
namespace P = ParamIDs;

RuminateAudioProcessorEditor::RuminateAudioProcessorEditor (RuminateAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      freeze (p.apvts, P::grFreeze, "Freeze"),
      output (p.apvts, P::output, "Output"),
      grainSection ("GRAIN DELAY", ui::colours::accent),
      seqSection ("STEP MODIFIERS", ui::colours::accent2),
      chorusSection ("CHORUS", ui::colours::accent3),
      tapeSection ("TAPE DELAY", ui::colours::accent3),
      reverbSection ("REVERB", ui::colours::accent3),
      stepGrid (p.apvts)
{
    setLookAndFeel (&lnf);

    auto& s = p.apvts;
    auto knob   = [&s] (auto& list, const char* id, const char* name, Colour c) { list.push_back (std::make_unique<ui::Knob> (s, id, name, c)); };
    auto choice = [&s] (auto& list, const char* id, const char* name) { list.push_back (std::make_unique<ui::Choice> (s, id, name)); };
    auto toggle = [&s] (auto& list, const char* id, const char* name) { list.push_back (std::make_unique<ui::Toggle> (s, id, name)); };

    const auto a1 = ui::colours::accent, a2 = ui::colours::accent2, a3 = ui::colours::accent3;

    // ---- grain -------------------------------------------------------------
    knob   (grainControls, P::grTime,      "Time",      a1);
    toggle (grainControls, P::grSync,      "Sync");
    choice (grainControls, P::grDiv,       "Div");
    knob   (grainControls, P::grScatter,   "Scatter",   a1);
    knob   (grainControls, P::grSize,      "Size",      a1);
    knob   (grainControls, P::grSizeRand,  "Size Rnd",  a1);
    knob   (grainControls, P::grRate,      "Rate",      a1);
    knob   (grainControls, P::grRateRand,  "Rate Rnd",  a1);
    knob   (grainControls, P::grPitch,     "Pitch",     a1);
    knob   (grainControls, P::grPitchRand, "Pitch Rnd", a1);
    knob   (grainControls, P::grReverse,   "Reverse",   a1);
    knob   (grainControls, P::grShape,     "Shape",     a1);
    knob   (grainControls, P::grFeedback,  "Feedback",  a1);
    knob   (grainControls, P::grTone,      "Tone",      a1);
    knob   (grainControls, P::grSpread,    "Spread",    a1);
    knob   (grainControls, P::grMix,       "Mix",       a1);

    // ---- sequencer ---------------------------------------------------------
    toggle (seqControls, P::seqOn,       "Seq On");
    knob   (seqControls, P::seqSteps,    "Steps",     a2);
    toggle (seqControls, P::seqSync,     "Sync");
    choice (seqControls, P::seqDiv,      "Div");
    knob   (seqControls, P::seqRate,     "Rate",      a2);
    choice (seqControls, P::seqMode,     "Mode");
    knob   (seqControls, P::seqPosRange, "Pos Range", a2);

    // ---- fx ----------------------------------------------------------------
    choice (chorusControls, P::chPlace, "Place");
    knob   (chorusControls, P::chRate,  "Rate",  a3);
    knob   (chorusControls, P::chDepth, "Depth", a3);
    knob   (chorusControls, P::chMix,   "Mix",   a3);

    choice (tapeControls, P::tpPlace,    "Place");
    knob   (tapeControls, P::tpTime,     "Time",     a3);
    toggle (tapeControls, P::tpSync,     "Sync");
    choice (tapeControls, P::tpDiv,      "Div");
    knob   (tapeControls, P::tpFeedback, "Feedback", a3);
    knob   (tapeControls, P::tpTone,     "Tone",     a3);
    knob   (tapeControls, P::tpWow,      "Wow",      a3);
    knob   (tapeControls, P::tpDrive,    "Drive",    a3);
    knob   (tapeControls, P::tpMix,      "Mix",      a3);

    choice (reverbControls, P::rvPlace,    "Place");
    knob   (reverbControls, P::rvSize,     "Size",     a3);
    knob   (reverbControls, P::rvDamp,     "Damp",     a3);
    knob   (reverbControls, P::rvWidth,    "Width",    a3);
    knob   (reverbControls, P::rvPredelay, "Predelay", a3);
    knob   (reverbControls, P::rvMix,      "Mix",      a3);

    for (auto* sec : { &grainSection, &seqSection, &chorusSection, &tapeSection, &reverbSection })
        addAndMakeVisible (*sec);

    // Controls live inside their sections so section-local layout applies.
    const std::pair<ui::Section*, std::vector<std::unique_ptr<Component>>*> owners[] =
    {
        { &grainSection, &grainControls }, { &seqSection, &seqControls }, { &chorusSection, &chorusControls },
        { &tapeSection, &tapeControls },   { &reverbSection, &reverbControls },
    };
    for (auto& [section, list] : owners)
        for (auto& c : *list) section->addAndMakeVisible (*c);
    seqSection.addAndMakeVisible (stepGrid);

    title.setText ("RUMINATE", dontSendNotification);
    title.setFont (FontOptions (22.0f, Font::bold));
    title.setColour (Label::textColourId, ui::colours::text);
    addAndMakeVisible (title);

    status.setJustificationType (Justification::centredRight);
    status.setFont (FontOptions (12.0f));
    status.setColour (Label::textColourId, ui::colours::dim);
    addAndMakeVisible (status);

    addAndMakeVisible (freeze);
    addAndMakeVisible (output);

    setResizable (true, true);
    setResizeLimits (960, 640, 1920, 1280);
    setSize (1120, 720);
    startTimerHz (30);
}

RuminateAudioProcessorEditor::~RuminateAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void RuminateAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (ui::colours::bg);
}

void RuminateAudioProcessorEditor::layoutRow (Rectangle<int> area, std::vector<std::unique_ptr<Component>>& controls)
{
    if (controls.empty()) return;

    // Dropdowns need more width than knobs; toggles need less.
    auto weight = [] (Component* c) -> float
    {
        if (dynamic_cast<ui::Choice*> (c) != nullptr) return 1.35f;
        if (dynamic_cast<ui::Toggle*> (c) != nullptr) return 1.0f;
        return 1.0f;
    };
    float total = 0.0f;
    for (auto& c : controls) total += weight (c.get());

    float xPos = (float) area.getX();
    const float unit = (float) area.getWidth() / total;
    for (auto& c : controls)
    {
        const float w = unit * weight (c.get());
        auto cell = Rectangle<int> ((int) xPos, area.getY(), (int) w, area.getHeight()).reduced (2, 0);
        xPos += w;
        if (dynamic_cast<ui::Knob*> (c.get()) != nullptr)
            c->setBounds (cell);
        else
            c->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 44));
    }
}

void RuminateAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);

    // header
    auto header = r.removeFromTop (48);
    title.setBounds (header.removeFromLeft (160));
    output.setBounds (header.removeFromRight (80).withTrimmedTop (-8).withHeight (64));
    freeze.setBounds (header.removeFromRight (90).withSizeKeepingCentre (90, 28));
    status.setBounds (header.reduced (8, 0));
    r.removeFromTop (18);

    const int gap = 8;
    const int fxH = 150;
    auto fxRow = r.removeFromBottom (fxH);
    r.removeFromBottom (gap);

    const int grainH = 130;
    grainSection.setBounds (r.removeFromTop (grainH));
    r.removeFromTop (gap);
    seqSection.setBounds (r);

    layoutRow (grainSection.content(), grainControls);

    // sequencer: controls column on the left, grid on the right
    auto seq = seqSection.content();
    auto ctl = seq.removeFromLeft (210);
    seq.removeFromLeft (8);
    {
        // two columns: [Seq On, Steps] [Sync, Div] [Rate, Mode] [Pos Range]
        const int rows = ((int) seqControls.size() + 1) / 2;
        const int rowH = jmax (60, ctl.getHeight() / rows);
        const int colW = ctl.getWidth() / 2;
        for (size_t i = 0; i < seqControls.size(); ++i)
        {
            auto& c = seqControls[i];
            auto cell = Rectangle<int> (ctl.getX() + (int) (i % 2) * colW, ctl.getY() + (int) (i / 2) * rowH, colW, rowH).reduced (3, 2);
            if (dynamic_cast<ui::Knob*> (c.get()) != nullptr) c->setBounds (cell);
            else c->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 44));
        }
    }
    stepGrid.setBounds (seq);

    // fx
    const int fxW = (fxRow.getWidth() - 2 * gap);
    chorusSection.setBounds (fxRow.removeFromLeft (fxW * 19 / 100));
    fxRow.removeFromLeft (gap);
    reverbSection.setBounds (fxRow.removeFromRight (fxW * 28 / 100));
    fxRow.removeFromRight (gap);
    tapeSection.setBounds (fxRow);

    layoutRow (chorusSection.content(), chorusControls);
    layoutRow (tapeSection.content(), tapeControls);
    layoutRow (reverbSection.content(), reverbControls);
}

void RuminateAudioProcessorEditor::timerCallback()
{
    const bool seqOn = processor.apvts.getRawParameterValue (P::seqOn)->load() > 0.5f;
    const int steps  = (int) std::lround (processor.apvts.getRawParameterValue (P::seqSteps)->load());
    stepGrid.setActiveSteps (steps);
    stepGrid.setPlayingStep (seqOn ? processor.currentStep() : -1);
    status.setText (String (processor.activeGrains()) + " grains", dontSendNotification);
}
