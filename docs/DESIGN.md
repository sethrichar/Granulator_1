# Design notes

## Why "snapshot at spawn"
The sequencer never modulates a running grain. When a grain is created it
copies the current step's values (pitch, direction, position, size, level)
into its own state. Because every grain is windowed, stepping any of these is
inherently click-free. It also means the sequencer clock only needs to be
evaluated at spawn time, which keeps the per-sample loop cheap.

The same idea makes adding new lanes easy: add a `Lane` to
`SequencerSettings`, a field to `StepModifiers`, read it in `snapshot()`, use
it in `GrainEngine::spawn()`, and add the 16 + 1 parameters in
`Parameters.cpp`. The step grid picks up new lanes from the same ID table.

Candidates for future lanes: rate multiplier, scatter, pan, tone, feedback.

## Clocking
* **Synced + host playing**: step = floor(ppq / stepBeats) so the sequence is
  phase-locked to the timeline and restarts consistently on loop.
* **Synced but stopped, or free**: an internal phase accumulator at the step
  period (tempo-derived or Hz).
* Ping-pong uses period 2n−2 so the end steps aren't doubled. Random picks a
  new step only when the tick changes.

## Read-head safety
Grains read a 60 s circular buffer that is being written at the same time. For
forward grains faster than 1× the read head could overtake the write head, so
the minimum delay is `(ratio − 1) × grainLength + guard`. Reverse grains can't
go further back than the buffer holds, so the maximum delay is reduced by
`ratio × grainLength`. Both clamps happen at spawn.

## Feedback
The wet sum goes through the Tone filters (which double as a 20 Hz DC block),
then `tanh()` before being added to the input at the write head. At 100 % the
loop sustains as a slowly evolving cloud rather than clipping. The dry path is
untouched by feedback.

## Overlap compensation
Grain amplitude is scaled by `1 / sqrt(rate × size)` (floored at 1) so a dense
cloud and a sparse pattern sit at similar levels.

## Known characteristic: regular grains + pitch shift
With a fixed Rate, zero Scatter/Rate Rnd, and a pitch ratio ≠ 1, the *sum* of
overlapping grains has a small frequency offset that depends on the rate and
the input frequency (classic granular pitch-shift sidebands). Each grain on its
own is at the correct pitch, and any randomisation spreads the artefact into
texture. The test suite measures pitch on isolated grains for that reason.

If exact ensemble pitch at regular rates ever matters, the fix is a
"phase-coherent" mode: keep a read head running at the pitch ratio and start
each new grain where that head currently is (a multi-tap version of the
classic two-tap pitch shifter). Sketch: `pitchHead -= (ratio − 1)` per sample,
wrap it inside a window of one grain length around `Time`, and use
`writePos − Time − pitchHead` as the grain start.

## Effects
The three effects are intentionally simple, allocation-free classes with the
same `process(buffer, params)` shape, so reordering or adding one is a
one-line change in `PluginProcessor::processBlock`. The reverb wraps JUCE's
Freeverb; swap in an FDN later without touching the rest.

## Threading
The processor caches raw atomic parameter pointers once (`RawParams`), reads
them at the top of every block, and passes plain structs into the DSP. The
editor polls `currentStep()` / `activeGrains()` atomics on a 30 Hz timer.
