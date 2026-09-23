# MEASUREMENTS

Every performance number the project claims, with the conditions it was taken under.

A number without its build configuration, hardware and scene is not evidence. Editor numbers are
recorded only when explicitly labelled as such, because editor frame-rate smoothing hides the
spikes the project's claim is about.

## Format

```
### <date> — <what was measured>
Build: Development | Shipping | Editor
Hardware: <GPU, CPU>
Scene: <map, actor count, what was running>
Method: <stat GPU / CSV profiler / automation test / Insights>
Result: <numbers>
Reading: <what this means for the design>
```

## Standing hardware and method

Unless an entry says otherwise, every number below came from this configuration.

```
Build:    Development, editor target, run headless through UnrealEditor-Cmd.exe
Hardware: NVIDIA GeForce RTX 4070 Laptop GPU, 7948 MB dedicated, driver 592.82
          D3D12, SM6 feature level, 32472 MB system memory
Power:    UNRECORDED for every entry before 2026-09-23 — see the method note under R2
Scene:    Engine OpenWorld template map, no actors, no vehicle systems, nothing else drawing
Method:   C++ automation test, RHI timestamp queries (RQT_AbsoluteTime) bracketing each pass
Command:  UnrealEditor-Cmd.exe RubberAccumDemo.uproject
          -ExecCmds="Automation RunTests RacetrackSurface"
          -TestExit="Automation Test Queue Empty" -unattended -nopause -nosplash -nosound
```

**Every entry must state mains or battery.** This is a laptop, and the R2 measurement on
2026-09-23 spread by up to 4x across three runs for that reason alone. Entries written before that
date do not record it and their absolute figures should be read as indicative; the ratios and
shapes measured *within* a single run are unaffected.

**Narrowed 2026-09-23 by measurement rather than caution.** The confound is not uniform. Re-taking
the whole suite on mains showed the CPU game-thread figure moving by 2 to 3.7x and the
GPU-timestamped paint and decay figures not moving at all. So the pre-2026-09-23 GPU pass numbers
are sound and only the CPU wall-clock ones needed the re-take. Details under
[the mains re-take](#2026-09-23--r2-re-taken-on-mains-and-what-the-battery-confound-actually-touched).
The rule stands for every future entry regardless.

### What the timestamp bracket actually contains

The two timestamps are written on the GPU timeline, but the work between them is submitted from
the game thread: `PaintStrokes` runs `BeginDrawCanvasToRenderTarget`, a `K2_DrawTexture` per
stroke, then `EndDrawCanvasToRenderTarget`. Any interval where the GPU is idle waiting for the
game thread to finish issuing those calls falls inside the bracket and is counted.

So these figures are the cost of the pass *as scheduled*, not the GPU's pure busy time. That is
the number a frame budget needs, and it is deliberately the pessimistic reading, but it means a
figure here cannot be attributed wholly to GPU execution. Where a number is suspiciously large
for the texels touched, submission overhead is the first thing to suspect.

---

## 2026-09-22 — B3, paint and decay passes

First measurement taken in the project. Replaces the proposal's estimates.

```
Region:   20000 cm square (200 m), 2048 texels, 9.77 cm per texel
Target:   RGBA8, 16777216 bytes (16.0 MiB), one render target
          (see the note below: this is no longer the production default)
Stroke:   91.7 cm long (5500 cm/s at 60 Hz), 20 cm wide, so about 9.4 by 2.0 texels
Samples:  64 per figure, median and minimum reported, no batching
```

| Pass | Median | Minimum |
|---|---|---|
| Paint, 1 contact | 0.1150 ms | 0.0980 ms |
| Paint, 4 contacts | 0.3610 ms | 0.2760 ms |
| Paint, 80 contacts | 5.9910 ms | 5.0240 ms |
| Full-buffer decay, 2048 square | 0.1570 ms | 0.1370 ms |

### Note added 2026-09-22: the test now pins its format explicitly

These figures were taken when `USurfaceAccumulationBuffer::Allocate` still defaulted to RGBA8, and
`Performance.PaintAndDecay` inherited that default. The B4 and B6 decisions changed the production
default to RGBA16f, which would have silently changed the conditions of this measurement on the
next run.

The test now passes `RTF_RGBA8` explicitly, so the numbers above stay reproducible and keep meaning
what they say. The figures are therefore still RGBA8 figures and are *not* representative of the
shipping configuration's decay cost — for that, read the 2048 RGBA16f row of the B4 sweep below,
0.2400 ms. Paint cost is unaffected, because the same sweep shows paint is independent of format.

### Gate: PASSED

The gate in [PLAN.md](PLAN.md) is *four contact points at 60 Hz within roughly 0.5 ms GPU on a
4070 without batching*. Four contacts cost **0.3610 ms** median, 0.2760 ms best case. That is
inside the budget with about 28 % headroom, unbatched, as specified.

The test also prints a combined figure of 0.5180 ms for four contacts plus one full-buffer decay,
which is over 0.5 ms. That combination is not the gate, and it is not a per-frame cost either:
PLAN.md E2 already makes decay frequency a cost knob. Decay at 1 Hz rather than 60 Hz amortises
0.1570 ms to 0.0026 ms per frame. The decay pass is not on the critical path and the flush
strategy does not need redesigning.

### Reading: cost is per contact, not per texel

The three paint figures are close to linear in contact count.

| Step | Contacts | Marginal cost per contact |
|---|---|---|
| 1 to 4 | +3 | 0.0820 ms |
| 4 to 80 | +76 | 0.0741 ms |

About **0.075 ms per contact**, flat. A stroke covers roughly 19 texels, and the full-buffer decay
pass covers 4.2 million texels for 0.1570 ms. If paint cost were proportional to texels touched, a
stroke would cost under a nanosecond. It costs a hundred thousand times that, so essentially none
of the paint cost is fill: it is per-`K2_DrawTexture` submission and state overhead, one draw call
per stroke, with the GPU idle between them.

Two consequences, both load-bearing:

1. **Batching is mandatory before phase C, not an optimisation for phase H.** Eighty contacts is
   twenty cars, which is the traffic density phases C3 and E7 need to spread rubber across a line.
   At 5.9910 ms it would consume a 60 Hz frame on its own. The gate deliberately said "without
   batching" and the gate passes, but the 80-contact figure is the number that decides how many
   cars the harness can run, and unbatched it is about six.
2. **Texel density is nearly free on the paint side.** Raising resolution does not make painting
   more expensive, because painting is not fill-bound. It only makes the decay pass and memory
   more expensive. That separates the B4 decision cleanly: resolution trades memory and decay cost
   against precision, and does not trade against contact throughput at all.

### Against the proposal's estimates

| Proposal claim | Status |
|---|---|
| 4096 square RGBA8 is 64 MB | Consistent. 2048 square measured at exactly 16.0 MiB, so 4096 square is 64.0 MiB |
| Roughly 5 cm per texel over a 200 m square | Only at 4096 texels. At 2048 it is 9.77 cm per texel |
| 0.5 ms GPU for four contacts at 60 Hz | Confirmed. 0.3610 ms measured |
| Eighty wheels is 320 bytes of readback | Not yet measured, belongs to phase F |
| Readback arrives two or three frames late | Not yet measured, belongs to phase F |

### Re-run on the same day

The suite was run twice as later tests were added. Second run of the same test:

| Pass | First run median | Second run median |
|---|---|---|
| Paint, 1 contact | 0.1150 ms | 0.1100 ms |
| Paint, 4 contacts | 0.3610 ms | 0.3040 ms |
| Paint, 80 contacts | 5.9910 ms | 4.9920 ms |
| Full-buffer decay | 0.1570 ms | 0.1480 ms |

Run-to-run spread is about 15 % on the paint figures and 17 % on the 80-contact figure. Both runs
clear the gate and both show the same per-contact linearity, so the conclusions hold, but no
figure here should be quoted to better than two significant figures.

### Method note: the pooled query path does not work from the game thread

The first version of this test used `RHICreateRenderQueryPool` and `FRHIPooledRenderQuery`. It
compiled and crashed on its first run:

```
Assertion failed: IsInParallelRenderingThread()
  RHIResources.h:2521  FRHIPooledRenderQuery::~FRHIPooledRenderQuery()
```

Both the constructor and the destructor of `FRHIPooledRenderQuery` assert
`IsInParallelRenderingThread()`, so a pooled query cannot be owned by anything whose lifetime ends
on the game thread. The engine's own `GPUBenchmark.cpp` only ever uses the pool inside a function
that already has an `FRHICommandListImmediate`.

The test now calls `RHICreateRenderQuery(RQT_AbsoluteTime)` directly. That returns a plain
refcounted `FRenderQueryRHIRef` whose `Release` is atomic and defers deletion, so it is safe to
drop from the game thread. `RHIGetRenderQueryResult` also moved inside the render command, where
`DynamicRHI.h` documents it as valid; the previous game-thread call was relying on
`GRHISupportsAsyncGetRenderQueryResult` being true on D3D12 without checking it. The query pair is
now created once per figure and reused across all 64 samples rather than allocated per sample.

---

## 2026-09-22 — B4, region size against texel density, memory and format

Test `RacetrackSurface.Budget.RegionAndFormatSweep`. Every resolution and format combination was
allocated for real, painted with four contacts and decayed, 64 timing samples each. Byte counts
are the real allocation, not arithmetic. The two centimetres-per-texel columns are derived from
the resolution and are labelled as such.

| Resolution | Format | Allocation | cm/texel over 200 m | cm/texel over 1500 m | Paint 4 | Full-buffer decay |
|---|---|---|---|---|---|---|
| 1024 | RGBA8 | 4.0 MiB | 19.53 | 146.48 | 0.3610 ms | 0.1450 ms |
| 1024 | RGBA16f | 8.0 MiB | 19.53 | 146.48 | 0.3550 ms | 0.1590 ms |
| 1024 | RGBA32f | 16.0 MiB | 19.53 | 146.48 | 0.3680 ms | 0.3020 ms |
| 2048 | RGBA8 | 16.0 MiB | 9.77 | 73.24 | 0.3640 ms | 0.1810 ms |
| 2048 | RGBA16f | 32.0 MiB | 9.77 | 73.24 | 0.3530 ms | 0.2400 ms |
| 2048 | RGBA32f | 64.0 MiB | 9.77 | 73.24 | 0.3600 ms | 0.8220 ms |
| 4096 | RGBA8 | 64.0 MiB | 4.88 | 36.62 | 0.3510 ms | 0.3610 ms |
| 4096 | RGBA16f | 128.0 MiB | 4.88 | 36.62 | 0.3510 ms | 0.5530 ms |
| 4096 | RGBA32f | 256.0 MiB | 4.88 | 36.62 | 0.3510 ms | 2.4130 ms |

### Reading

**Paint cost is independent of resolution and of format.** Every paint figure is between 0.3510
and 0.3680 ms, a 4 % spread across a 64-fold range of buffer sizes. This is the same conclusion
B3 reached from the contact-count scaling, arrived at independently: painting is submission-bound.
Resolution is therefore free on the write path, and the resolution decision is purely memory
against precision.

**Decay cost tracks bytes touched, not texels.** At 2048 square, RGBA8 to RGBA16f doubles the
bytes and costs 33 % more; RGBA32f quadruples the bytes and costs 4.5 times as much. RGBA32f at
4096 square reaches 2.4130 ms for a single pass, which is a frame-breaking cost for a pass that
does nothing but multiply by a constant.

**Decay cost is nearly irrelevant anyway**, because PLAN.md E2 makes decay frequency a knob. At
1 Hz rather than 60 Hz, the worst affordable configuration below amortises to under 0.01 ms per
frame. Memory is the binding constraint, not time.

### Decision: 2048 square RGBA16f as the default, 200 m region

- **32.0 MiB**, 9.77 cm per texel, decay 0.2400 ms at 1 Hz, paint unaffected.
- **RGBA32f is disqualified** on both axes: 64 MiB at 2048 square and a decay pass 4.5 times the
  RGBA16f cost, to buy precision that the B6 measurement below shows is not needed.
- **RGBA8 is disqualified by the precision floor**, not by cost. See B6. It is the cheapest option
  and it cannot accumulate the increments this project needs.
- **4096 square stays available as a configuration**, not a default. It buys 4.88 cm per texel,
  which is visibly better for a crisp skid edge, for 128 MiB. That is too much to impose by
  default in a plugin others install, and the resolution is already exposed on `FSurfaceRegion`.

**A 1500 m circuit does not fit at any resolution measured.** Even 4096 square gives 36.62 cm per
texel over 1500 m, which is wider than a tyre. This confirms the concern in the proposal rather
than resolving it: a full circuit needs either a region that follows the action or a tiled set of
regions. Recorded here as a known limit of the current single-region design, to be addressed when
D5 takes on World Partition streaming.

## 2026-09-22 — B5, saturating accumulator

Test `RacetrackSurface.Accumulator.SaturationCurve`. One texel, read back after a set number of
passes of the same stroke at increment 0.1, into RGBA16f. A second channel was pre-loaded to 0.5
before the run to see whether painting one channel disturbs another.

The two candidate hardware blend states, read from
`Engine/Source/Runtime/Engine/Private/BatchedElements.cpp:484` and `:487`:

```
Additive     Dst' = Src + Dst
             TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>
Translucent  Dst' = Src*SrcAlpha + Dst*(1-SrcAlpha)
             TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, ...>
```

| Pass | Additive | Saturating (translucent) |
|---|---|---|
| 1 | 0.09998 | 0.06494 |
| 2 | 0.19995 | 0.12561 |
| 5 | 0.50000 | 0.28516 |
| 10 | 1.00000 | 0.48877 |
| 20 | 1.99609 | 0.73877 |
| 40 | 3.98828 | 0.93213 |
| **Sibling channel, 0.5 before** | **0.50000** | **0.03406** |

### Reading: translucent saturates correctly and destroys the other channels

The saturating column is the intended curve. With `Src = 1.0` in the target channel and
`SrcAlpha` as the increment, translucent blending reduces to `Dst' = Dst + a*(1 - Dst)`, an
exponential approach to the ceiling that cannot overshoot. It reaches 0.93213 after 40 passes and
is still climbing. That is exactly what E1 asks for.

It also multiplies every other channel by `(1 - a)` on every pass, because the blend factor is one
scalar applied to all of R, G and B. The sibling channel fell from 0.50000 to 0.03406 over 40
passes. The measured collapse matches the model: the brush falloff makes the effective alpha at
the sampled texel 0.065 rather than 0.1, as the first pass reading of 0.06494 confirms, and
`0.5 * 0.935^40 = 0.035` against 0.03406 measured. So the mechanism is understood, not merely
observed, and it is fatal for a packed layout.

Additive leaves the sibling channel at exactly 0.50000. It is channel-independent, and in a float
format it is unbounded: 3.98828 after 40 passes, straight past the ceiling.

### Decision: additive write, saturation applied at sample time. Neither of the two options offered.

PLAN.md B5 framed this as hardware blend against double buffer. The measurements rule out both as
stated, and point at a third option that is cheaper than either.

- **Hardware translucent blend, one packed target: rejected.** It saturates, but it destroys the
  other channels, and B6 requires skid and buildup in one target.
- **Hardware translucent blend, one target per channel: rejected.** Correct, and channel
  independence becomes trivial because the channels are separate textures, but it needs one
  canvas pass per channel touched and a texture fetch per channel in the material, to solve a
  problem the option below solves with neither.
- **Double buffer: rejected on cost.** It needs a full-buffer material pass per flush to read the
  previous value and write the saturated one. The nearest thing measured is the full-buffer canvas
  decay pass at 2048 square RGBA16f, **0.2400 ms**, which is a floor rather than an estimate: a
  material pass over the same texels cannot be cheaper than a modulate over them, and this one
  would run every flush rather than at decay frequency. That is 79 % of the entire four-contact
  paint budget, plus a second 32 MiB target.
- **Chosen: additive accumulation of an unbounded exposure value, mapped through
  `1 - exp(-k * E)` where it is sampled.** Additive keeps channel independence, which is measured
  above at exactly 0.50000. The float format has no ceiling to clip against, which the 3.98828
  reading demonstrates. The saturating curve then comes out of the mapping rather than the blend,
  so it costs nothing on the write path: no extra pass, no second target, no per-channel target.
  The existing modulate decay pass still works, decaying exposure, which maps to a decaying
  coverage.

The cost of this choice is that the stored value is not the displayed value, so the material and
the phase F query path must apply the same mapping or they will disagree. That is one shared
constant.

**Not verified.** The mapping itself is not implemented or measured. It is authored material-side
in B2 and E1. The `Saturating` paint mode stays in the buffer because it is what produced the
measurement above, and because it remains the correct choice for any single-channel target.

## 2026-09-22 — B6, channel packing and the precision floor

Test `RacetrackSurface.Accumulator.PrecisionFloor`. 200 additive passes of one stroke at a fixed
increment, then read the centre texel. "First pass that moved the texel" polls after every pass
until the value leaves zero.

| Format | Increment | After 200 passes | Ideal | First pass that moved the texel |
|---|---|---|---|---|
| RGBA8 | 0.0500 | 1.00000 | 1.00000 | 1 |
| RGBA8 | 0.0100 | 1.00000 | 1.00000 | 1 |
| RGBA8 | 0.0040 | 0.78431 | 0.80000 | 1 |
| RGBA8 | 0.0020 | **0.00000** | 0.40000 | **never** |
| RGBA8 | 0.0010 | **0.00000** | 0.20000 | **never** |
| RGBA16f | 0.0500 | 9.95312 | 1.00000 | 1 |
| RGBA16f | 0.0100 | 1.96484 | 1.00000 | 1 |
| RGBA16f | 0.0040 | 0.78857 | 0.80000 | 1 |
| RGBA16f | 0.0020 | 0.39429 | 0.40000 | 1 |
| RGBA16f | 0.0010 | 0.19714 | 0.20000 | 1 |
| RGBA32f | 0.0500 | 10.00002 | 1.00000 | 1 |
| RGBA32f | 0.0100 | 2.00000 | 1.00000 | 1 |
| RGBA32f | 0.0040 | 0.80000 | 0.80000 | 1 |
| RGBA32f | 0.0020 | 0.40000 | 0.40000 | 1 |
| RGBA32f | 0.0010 | 0.20000 | 0.20000 | 1 |

### Reading: RGBA8 has a hard floor at one part in 255, and below it accumulation is dead

An increment of 0.002 into RGBA8 does not accumulate slowly. It does not accumulate **at all**:
zero after 200 passes, and the texel never moved once. The destination is 8-bit fixed point, the
blend rounds `Dst + 0.002` back to `Dst`, and the writes are silently discarded forever. The floor
is one quantisation step, 1/255 = 0.0039, which is why 0.0040 survives and 0.0020 does not.

This is the number that decides the format, and it decides it against RGBA8. The project's premise
is rubber accumulating over dozens of laps. If the buildup channel is to approach saturation over
500 contacts, the per-contact increment is 0.002, which lands exactly in the dead zone. The
alternative is to raise the increment above 0.0039 and accept saturation in under 256 contacts,
which is not "builds up over a session".

RGBA16f tracks 0.0010 to within 1.4 % over 200 passes and never stalls. RGBA32f is exact and costs
twice the memory and 4.5 times the decay time for accuracy that is not needed. The 0.05 and 0.01
rows also show float formats running past 1.0 without clipping, which is the property the B5
decision depends on.

### Decision: one RGBA16f target, R buildup, G skid, B marbles, A unused

| Channel | Contents | Why here |
|---|---|---|
| R | Rubber buildup exposure | Read together with G by E5's decay coupling |
| G | Skid mark exposure | Read together with R by E5's decay coupling |
| B | Marble density exposure | Read alone by G4, but free to pack |
| A | Unused | Not writable through this path |

One target and one texture fetch gives every channel at once, which satisfies B6's constraint that
skid and buildup be sampleable together. E5 makes skid decay a function of local buildup, so those
two are read in the same material and the same query; splitting them across targets would double
the fetches on the path that matters most.

**The alpha channel is not writable through the canvas path.** Both candidate blend states are
declared `CW_RGB` in `BatchedElements.cpp:484` and `:487`, so alpha is masked out of the colour
write regardless of what the draw passes. This is read from the engine source and was not
measured, because nothing currently needs a fourth channel. If a fourth is ever wanted, that is
the constraint to re-test first.

### Confirmed: the canvas write path is linear, and not for the reason the source suggests

Test `RacetrackSurface.Accumulator.LinearGammaOnWrite`.

```
CreateRenderTarget2D default for RTF_RGBA8: display gamma 1.00, IsSRGB false
With bForceLinearGamma set:                 display gamma 1.00, IsSRGB false
One additive pass at intensity 0.500 stored 0.4980
  linear would be 0.5000, a 2.2 encode would be 0.7297
```

This started as a suspected bug. Reading `UTextureRenderTarget2D::GetDisplayGamma` suggests that
an `RTF_RGBA8` target returns the default 2.2 display gamma, because the function only special
cases `PF_FloatRGB` and `PF_FloatRGBA`, and `UTextureRenderTarget2D::InitAutoFormat` does not set
`bForceLinearGamma` — the engine source carries a `// ?? missing ?` comment at exactly that spot.
A 2.2 encode on write while the material samples linear would put the accumulator in the wrong
space and break both the additive sum and the saturating mapping.

Measurement says otherwise. The constructor at `TextureRenderTarget2D.cpp:48` already sets
`bForceLinearGamma = true`, so the default is linear and the stored value is 0.4980 against 0.5000
ideal. Setting the flag explicitly in `Allocate` was therefore a no-op that cost a redundant
`UpdateResource`, and it was removed. The test stays as the guard on the invariant instead, so if
a future engine version changes that constructor default the suite fails rather than the
accumulator quietly going non-linear.

---

## 2026-09-22 — C0, stroke batching: before and after

Test `RacetrackSurface.Performance.PaintAndDecay`, same standing hardware and method as above. The
test still pins `RTF_RGBA8` deliberately, so these figures remain comparable with the B3 numbers
recorded at the top of this file.

**The change.** One line of behaviour in `USurfaceAccumulationBuffer::PaintStrokes`: the canvas
returned by `UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget` has its `Allow_Flush` bit
cleared for the duration of the stroke loop, through a scoped guard, and restored before
`EndDrawCanvasToRenderTarget`. Nothing else about the paint path changed — same brush, same blend
states, same per-stroke geometry.

### Before, two runs of the identical pre-change code

| Pass | Run A | Run B |
|---|---|---|
| Paint, 1 contact | 0.0900 ms | 0.0920 ms |
| Paint, 4 contacts | 0.2620 ms | 0.2820 ms |
| Paint, 80 contacts | 4.2230 ms | 4.5860 ms |
| Full-buffer decay | 0.1290 ms | 0.1210 ms |

### After, three runs

| Pass | Run A | Run B | Run C |
|---|---|---|---|
| Paint, 1 contact | 0.0920 ms | 0.1000 ms | 0.0910 ms |
| Paint, 4 contacts | 0.0930 ms | 0.1050 ms | 0.0910 ms |
| Paint, 80 contacts, uniform heading | 0.0960 ms | 0.0890 ms | 0.0900 ms |
| Paint, 80 contacts, 80 distinct headings | 0.1060 ms | 0.1150 ms | 0.0980 ms |
| Full-buffer decay | 0.1290 ms | 0.1200 ms | 0.1210 ms |

### C0 exit condition: MET

The exit condition in [PLAN.md](PLAN.md) is *eighty contacts within the four-contact budget*. The
four-contact budget before the change was 0.2620 to 0.2820 ms. Eighty contacts now cost **0.0890 to
0.1150 ms**, so eighty contacts are not merely inside the old four-contact budget — they cost less
than four contacts used to. Against the 80-contact baseline it is a **40 to 50 times** reduction.

### Reading: cost is now flat in contact count, which is the proof the batching is real

The important number is not the speedup, it is the shape. One contact, four contacts and eighty
contacts now all cost the same 0.09 to 0.11 ms, where before they cost 0.09, 0.27 and 4.4 ms. A
flat line against contact count is what one submission per call looks like.

The residual 0.09 ms is also self-validating: it is, to two significant figures, exactly the
*old* one-contact figure. That is expected, because the old one-contact case was already one flush.
The per-flush cost has not changed at all; what changed is how many of them a frame pays for. This
is the strongest available evidence that the diagnosis was right — the cost removed was N-1
redundant flushes, not fill, not state, not anything else.

Decay is unchanged at 0.12 to 0.13 ms, as expected: it was always a single draw and was never
affected by the per-item flush.

### The rotation case, measured rather than assumed

Batches are keyed partly on the canvas transform, and `FCanvasTileItem::Draw` pushes a per-stroke
rotation transform whenever the rotation is non-zero, so strokes at different headings land in
separate batches. The existing 80-contact benchmark happens to use strokes that are all exactly
horizontal, so all 80 share one batch — a best case that does not represent a field of cars at
varying headings.

Rather than report that best case as though it were general, a second 80-contact figure was added
with 80 distinct headings. It costs **0.0980 to 0.1150 ms** against 0.0890 to 0.0960 ms for the
uniform case. Batch fragmentation is therefore real but nearly free here, roughly 0.01 to 0.02 ms
across 80 strokes, because the cost that dominated was the per-item flush rather than the draw
call. Production traffic with varying headings gets essentially the full win.

### Correctness: the batched result is identical, not merely similar

Test `RacetrackSurface.Accumulator.BatchedPaintMatchesSequential`. Twelve strokes at twelve
distinct headings, all crossing the centre so they overlap heavily, painted two ways into two
RGBA16f buffers: once as a single batched call, once as twelve separate calls. A 16 by 16 grid of
texels centred on the overlap is read back from each and compared.

```
12 strokes at 12 distinct headings, 256 grid samples, 256 inked, peak 0.59961,
max batched-vs-sequential difference 0.000000
```

All 256 samples are inked and the peak is 0.59961, against 12 x 0.05 = 0.6 for twelve fully
overlapping strokes, so the window genuinely samples the overlap rather than isolated strokes. The
maximum difference between the two buffers is **0.000000** — identical, not close.

This matters because additive blending being order-independent is an argument, not a measurement,
and the first version of this test was wrong in a way worth recording: its sample grid straddled
the centre without ever landing on it, so every sample saw exactly one stroke (peak 0.04999) and
the test passed without exercising overlap at all. The assertion that the peak must exceed one
stroke's worth was added so it cannot silently degrade back into that.

### What is not verified

- **Draw-call count was not read from a counter.** `GNumDrawCallsRHI` exists and could be sampled,
  but it is a global incremented by everything drawing in the process, so a delta around the paint
  would also catch editor viewport work. The claim that submissions collapsed rests on the flat
  cost curve and the uniform-versus-fanned comparison, both of which are measurements, rather than
  on a direct count.
- **Not measured in a real scene.** These are automation-test figures with nothing else drawing.
  The harness in C2 onward is what will show the cost with twenty cars actually running.
  *Partly closed 2026-09-22 at five cars — see the real-scene entry below. Twenty is still open.*

---

## 2026-09-22 — First real-scene measurement, five cars in L_TrackHarness

The first number in this project that did not come from an automation test. Everything above was
measured with nothing else drawing; this is the paint pass inside a live frame, competing with the
scene's own rendering.

```
Build:    Development, editor target, Simulate-In-Editor, driven over MCP
Hardware: NVIDIA GeForce RTX 4070 Laptop GPU, driver 592.82, D3D12, SM6
Scene:    /Game/Maps/L_TrackHarness — Template_Default lighting and sky, a 200 m plane
          rendering M_SurfaceAccumulationDebug, ARacetrackCircuit, 5 ASplineFollowingCar
Region:   20000 cm square, 2048 texels, RGBA16f, 33554432 bytes (the production default,
          NOT the RTF_RGBA8 the B3/C0 tests pin)
Contacts: 5 per frame, one per car, RubberBuildup channel, all at distinct headings
Method:   r.RacetrackSurface.TimePaint, an RHI timestamp bracket (RQT_AbsoluteTime) around the
          whole of PaintStrokes including EndDrawCanvasToRenderTarget, logged per call
Samples:  1346 consecutive paint calls
```

| Statistic | 5 contacts, real scene |
|---|---|
| Minimum | 0.0270 ms |
| **Median** | **0.0460 ms** |
| Mean | 0.0911 ms |
| 90th percentile | 0.1980 ms |
| 99th percentile | 0.4450 ms |
| Maximum | 1.0070 ms |
| Share under 0.12 ms | 79.3 % |

### Reading: C0 survives contact with a real scene, and the tail is the new information

Against the C0 headless figures — 4 contacts at 0.0910 to 0.1050 ms, 80 contacts at 0.0890 to
0.1150 ms — the real-scene **median is lower, at 0.0460 ms**, and the median is the honest
comparison because the headless figures are themselves medians of 64 samples.

That the median did not get *worse* under real rendering load is the result that matters: the C0
claim was taken with nothing else on the GPU, and the obvious way for it to have been misleading
was for contention to eat the win. It did not.

**The distribution is what the synthetic test could not show.** Mean 0.0911 ms sits at double the
median, p99 is 0.4450 ms and the worst single frame is 1.0070 ms. A 64-sample synthetic benchmark
reports a median and a minimum and cannot see a tail like this. The tail is contention, not paint
work — the same five quads are being drawn every frame, so a twenty-fold spread cannot be the
drawing. Budgeting off the median alone would be wrong; the number to carry forward for frame
budgeting is **p99 at 0.45 ms**, not the 0.046 ms median.

### Two reasons these figures are not directly interchangeable with the C0 table

- **Different format.** This ran RGBA16f, the production default; B3 and C0 pin `RTF_RGBA8`
  deliberately. The B4 sweep measured paint as independent of format to within 4 % across a
  64-fold range of buffer sizes, so this should not matter, but it is a difference and it is not
  re-verified here.
- **Different sampling regime.** The C0 test times 64 iterations of a loop it controls. This times
  one real frame's paint, 1346 times, inside a live game loop. The instrument synchronises the
  game thread with the render thread on every paint, so it perturbs frame pacing; the *bracket*
  still measures the GPU timestamp delta, but the surrounding frame is not a normal frame.

Treat the two as agreeing to about a factor of two, not better.

### What is not verified

- **Twenty cars in a real scene is still unmeasured.** Five contacts is what the harness runs
  today. Eighty contacts is the density C3 and E7 need, and the only 80-contact figures in this
  file are synthetic. Taking that number is the first thing C3 should do, not the last.
  *Closed 2026-09-23 by [R6](#2026-09-23--r6-the-real-scene-re-taken-on-the-ribbon-and-twenty-cars-at-last).
  Twenty cars cost 0.0220 ms median against 0.0210 ms for five — flat, as C0 predicted.*
- **The stored stroke width was not read back.** `WidthInTexels` computes to 2.05 texels for a
  20 cm contact patch at 9.77 cm per texel, and the paint code uses it directly as the quad's
  height, so the geometry says the mark is ~20 cm wide. The captured frames show visibly wider
  bands, which is most likely bloom on an unlit emissive material at coverage near 1.0, but no
  texel was read back to confirm that, and no bloom-off capture was taken because setting
  `r.BloomQuality` needs console access that MCP does not have. **If the bands are genuinely
  wider than 2 texels, D1's "narrow" requirement is already violated and every later intensity
  figure is affected** — worth settling with a readback before D1, not after.
  *Closed 2026-09-23 by [R4](#2026-09-23--r4-the-painted-stroke-width-read-back-at-last). The
  buffer stores exactly 2 texels, 19.53 cm, for a 20 cm patch. D1's requirement is not violated
  and the later intensity figures are safe.*
- **Frame rate was not recorded.** `stat unit` is unreachable over MCP, so the cost of the paint
  pass *as a share of the frame* is not known, only its absolute time.

---

## 2026-09-22 — R2, world to track-space conversion cost

The gate on [phase R](PLAN.md). Track-space storage introduces a cost the square world-XY region
did not have: every world-space contact has to be located on the centreline spline before it can
be painted. `USplineComponent::FindInputKeyClosestToWorldLocation` is a search, and eighty of them
per frame is the density C3 and E7 need.

Test `RacetrackSurface.Performance.TrackSpaceConversion`. This is a **CPU game-thread**
measurement, not a GPU one — no rendering is involved — so it uses wall-clock sampling
(`FPlatformTime::Seconds`, 64 samples, median reported) rather than the RHI timestamp queries the
GPU passes use.

Standing hardware and method, plus: no scene at all. A closed circular `USplineComponent` built in
the test, 150 m radius, 12 m track width, contacts spread evenly around the lap at five lateral
offsets. 64 samples per figure, median reported.

Three runs were taken: two headless under the standing method, and one earlier in the running
editor. **The headless runs are the numbers of record.** All three are given because the spread
between them is itself a result.

### Headless run A, the number to carry forward

| Spline points | Contacts | Full search | per contact | Known segment | per contact | Speedup |
|---|---|---|---|---|---|---|
| 18 | 4 | 0.0256 ms | 0.0064 ms | 0.0020 ms | 0.0005 ms | 12.8x |
| **18** | **80** | **0.5053 ms** | 0.0063 ms | 0.0335 ms | 0.0004 ms | 15.1x |
| 64 | 4 | 0.0586 ms | 0.0147 ms | 0.0012 ms | 0.0003 ms | 48.9x |
| 64 | 80 | 1.1915 ms | 0.0149 ms | 0.0277 ms | 0.0003 ms | 43.0x |
| 256 | 4 | 0.1983 ms | 0.0496 ms | 0.0014 ms | 0.0004 ms | 141.6x |
| 256 | 80 | 4.0437 ms | 0.0505 ms | 0.0316 ms | 0.0004 ms | 128.0x |
| 1024 | 4 | 0.7644 ms | 0.1911 ms | 0.0018 ms | 0.0004 ms | 424.8x |
| 1024 | 80 | 15.5313 ms | 0.1941 ms | 0.0339 ms | 0.0004 ms | 458.1x |

### Headless run B, same binary, same command, minutes later

| Spline points | Contacts | Full search | per contact | Known segment | per contact | Speedup |
|---|---|---|---|---|---|---|
| 18 | 4 | 0.0177 ms | 0.0044 ms | 0.0014 ms | 0.0004 ms | 12.6x |
| **18** | **80** | **0.2830 ms** | 0.0035 ms | 0.0138 ms | 0.0002 ms | 20.5x |
| 64 | 4 | 0.0235 ms | 0.0059 ms | 0.0006 ms | 0.0001 ms | 39.2x |
| 64 | 80 | 0.5334 ms | 0.0067 ms | 0.0113 ms | 0.0001 ms | 47.2x |
| 256 | 4 | 0.0882 ms | 0.0220 ms | 0.0005 ms | 0.0001 ms | 176.7x |
| 256 | 80 | 1.7317 ms | 0.0216 ms | 0.0128 ms | 0.0002 ms | 135.3x |
| 1024 | 4 | 0.3281 ms | 0.0820 ms | 0.0009 ms | 0.0002 ms | 363.9x |
| 1024 | 80 | 6.4214 ms | 0.0803 ms | 0.0129 ms | 0.0002 ms | 497.9x |

18 spline points is the harness circuit's actual complexity, read from `ARacetrackCircuit`.

### The spread is a result, and it is large

| Spline points, 80 contacts | Editor | Headless A | Headless B | A / B |
|---|---|---|---|---|
| 18 | 0.1332 ms | 0.5053 ms | 0.2830 ms | 1.79 |
| 64 | 0.3025 ms | 1.1915 ms | 0.5334 ms | 2.23 |
| 256 | 1.0773 ms | 4.0437 ms | 1.7317 ms | 2.33 |
| 1024 | 4.1797 ms | 15.5313 ms | 6.4214 ms | 2.42 |

Run A is uniformly about twice run B across every configuration, and the editor run is faster again.
A uniform whole-run scale factor is not per-configuration noise; it is the machine running at a
different clock.

**Cause identified after the fact: the laptop was on battery, not mains.** So the spread is a power
state, and it is controllable — it is not thermal drift, not scheduling, and not anything the test
can be rewritten to avoid. All three runs above were taken unplugged, progressively later into a
discharge.

**What that does and does not invalidate:**

- **The shape is identical in all three runs** and is the thing every conclusion below rests on:
  cost linear in spline points, flat in contact count, single-segment lookup flat at O(1). Three
  independent runs agreeing on shape is stronger evidence than one agreeing on a constant.
- **No absolute figure in this entry is a mains-power figure**, so none of them is comparable with
  the rest of this file, which is the real problem. Every other number recorded in this project was
  taken under whatever power state happened to apply and none of them says which.
- **The conclusion this entry draws about when the segment cache becomes necessary depends on the
  constant**, and the constant is exactly what is unreliable here. That conclusion is therefore
  provisional — see below.

**OUTSTANDING: re-take on mains power.** Until then this table establishes the shape and a
pessimistic bound, not a figure to compare against the paint-pass numbers elsewhere in this file.

*Closed 2026-09-23 by [the mains re-take](#2026-09-23--r2-re-taken-on-mains-and-what-the-battery-confound-actually-touched).
Three mains runs agree to within 5 % where these spread by 2.42x, the 1 ms crossover is measured at
234 spline points, and the reading below about the editor run is corrected there.*

### Gate: PASSED, no bail-out

Eighty contacts at harness spline complexity cost **0.5053 ms** on the worst run, which is
**3.03 % of a 60 Hz frame**. The gate asked whether this cost makes the restructure a bad trade,
and 3 % of a frame for the whole conversion does not. The phase continues.

For scale, the paint pass this sits beside costs 0.0460 ms median in the five-car scene, so
conversion is now the larger of the two by about ten times. That is a change in where the cost
lives, and it is worth stating plainly: **track space moved cost from the GPU to the game thread.**

### Reading: the cost is linear in spline points, and that is the number that matters

Per-contact cost against spline point count, run A: 0.0063, 0.0149, 0.0505, 0.1941 ms for 18, 64,
256 and 1024 points. From 64 points upward that is very nearly proportional — 4x the points for
3.4x and 3.8x the cost. Run B gives 0.0035, 0.0067, 0.0216, 0.0803, the same proportionality at a
different constant.

This is the behaviour predicted by reading the engine source rather than discovered by surprise.
`FindInputKeyClosestToWorldLocation` (`SplineComponent.cpp:2859`) delegates to
`FInterpCurve<FVector>::FindNearest` (`InterpCurve.h:544`), which loops over **every segment** and
solves an exact closest-point quintic per segment. There is no spatial index and no early out, so
the cost is O(segments) by construction.

**Contact count is free by comparison.** Per-contact cost is flat across 4 and 80 contacts at every
spline complexity, so twenty cars cost twenty times one car and nothing worse.

### The fix exists, is measured, and is deliberately not taken yet

`USplineComponent::FindInputKeyOnSegmentClosestToWorldLocation(WorldLocation, Index)`
(`SplineComponent.h:837`) searches one named segment and skips the scan entirely. Measured at
**0.0001 ms per contact, flat at every spline complexity** — genuinely O(1), as the source says.

That is a 15x saving at harness complexity and a **458x** saving at 1024 points, and unlike the
search itself the single-segment figure is essentially the same in both headless runs. It is not
taken now because the gate passes without it and the plan's standing rule is to measure before
optimising. What the table does is convert that from a judgement into a threshold.

**At 80 contacts the full search passes 1 ms somewhere between 64 and 256 spline points on battery**
— run A crosses at 64 points (1.1915 ms), run B at about 150, and the editor run at about 256. A
7 km circuit has 70 points at one every 100 m and 256 at one every 27 m, so the crossover and the
target sit on top of each other and **which side they fall on depends on the power state the
measurement was taken in.**

That is the honest position and it is weaker than the one first written here. What can be said
without the constant: the per-contact segment cache is needed at *some* track complexity between
this harness and a real circuit, it is not needed for this harness, and a mains-power re-take is
what turns that into a number.

Two things make the cache cheap when it is needed: the contacts that matter most come from
`ASplineFollowingCar`, which already uses the track-space overload and never searches at all; and a
car's segment index changes by at most one or two per frame, so the previous index plus its
neighbours is almost always the answer.

### The editor run, kept for the comparison rather than as evidence

An earlier run was taken in the running editor over MCP, before the editor could be closed for the
phase R editor-target rebuild. Its 18-point, 80-contact figure was 0.1332 ms, 0.80 % of a frame —
**optimistic by about four times against the worst headless run**, and it is what the first draft
of this entry recorded.

It is kept here for the comparison, and because the reason it differs turned out not to be the one
first assumed. The initial reading of the discrepancy was that the editor number was optimistic and
the headless ones were the truth. With the battery cause known, the more likely reading is that the
editor run was simply taken at a higher clock, and **none of the three is a mains-power figure**.

*Corrected 2026-09-23. The second reading was right and the first was backwards. The editor run
matches the mains runs to within 5 % at every spline complexity, so it was a full-clock run and the
two headless runs were the throttled ones. Calling the headless runs "the numbers of record" above
was a method judgement standing in for a measurement. See
[the mains re-take](#2026-09-23--r2-re-taken-on-mains-and-what-the-battery-confound-actually-touched).*

### Method note for every future measurement in this project

**Record the power state.** No entry in this file before this one does, which means the paint-pass
and decay figures recorded under phases B, C0 and the five-car scene are all of unknown power
state too. They remain internally comparable where they were taken in the same run — the B4 sweep
compares nine configurations against each other in one process, and C0's before/after were separate
runs and are the weakest of those comparisons for exactly this reason — but cross-entry comparisons
in this file carry an unquantified factor that nobody was tracking.

This does not invalidate any conclusion drawn so far, because every one of them rests on a ratio or
a shape measured within a single run: cost per contact being flat, cost being independent of
resolution and format, RGBA8's accumulation dead zone, the batched-equals-sequential difference of
exactly zero. It does mean the absolute millisecond figures quoted anywhere in this file should be
read as indicative.

---

## 2026-09-23 — R4, the painted stroke width, read back at last

Test `RacetrackSurface.Accumulator.PaintedStrokeWidth`. This closes the "stored stroke width was
not read back" item that has been carried since the first real-scene measurement, and which was
flagged as potentially invalidating D1's "narrow" requirement before D1 had begun.

One stroke, 20 cm contact width, intensity 1.0, additive, painted horizontally through the centre
of a 512 by 512 RGBA16f buffer. A 25-texel column is read back through the middle of the mark with
`ReadRenderTargetRawPixelArea(..., bNormalize = false)`, which is the only readback path that
preserves float precision on `PF_FloatRGBA`. Run twice, headless, identical both times.

| cm/texel | Quad height, texels | Peak | Inked texels | **Inked width** | Half-max texels |
|---|---|---|---|---|---|
| 9.765625 | 2.048 | 1.0000 | 2 | **19.53 cm** | 2 |
| 2.441406 | 8.192 | 1.0000 | 8 | **19.53 cm** | 8 |

Raw profiles, exactly as logged:

```
9.765625 cm/texel:  [255]=1.0000 [256]=1.0000
2.441406 cm/texel:  [252]=1.0000 [253]=1.0000 [254]=1.0000 [255]=1.0000
                    [256]=1.0000 [257]=1.0000 [258]=1.0000 [259]=1.0000
```

### D1's "narrow" requirement is not broken, and the arithmetic was right

A 20 cm contact patch paints **19.53 cm**, which is 2 texels at the production density and 8 texels
at four times that density. The physical width is the same to the last decimal at both, so the
stroke width is genuinely set by the world-space contact width through the texel mapping, not by
anything resolution-dependent. The earlier arithmetic — 2.05 texels for a 20 cm patch — was
correct, and the visibly wider bands in the captured frames are therefore something in the *display*
path, not the buffer. Bloom on an unlit emissive material remains the likely explanation and is now
the only remaining candidate, since the buffer has been eliminated.

**Every later intensity figure is safe.** That was the thing at risk: if the buffer had been laying
down marks several times wider than asked, coverage per pass would have been wrong and every
saturation number with it.

### The finding nobody was looking for: the brush falloff does nothing

Every inked texel reads **exactly 1.0000**, and the inked count is exactly the quad height. There is
no falloff at all — not a soft edge, not a gradient, not even a partially-covered edge texel.

At 2 texels that could be dismissed as too few samples to show a gradient. At 8 texels it cannot:
`BuildRadialBandTexels` generates a smoothstep falloff from the centre row to the edges, so eight
texels should read something like 0.1, 0.5, 0.9, 1.0, 1.0, 0.9, 0.5, 0.1. They read 1.0 eight times.

The reason is in the blend state. Additive is
`TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>` (`BatchedElements.cpp:484`, already read for
B5). Source factor One, destination factor One, **and `CW_RGB` masks alpha out of the colour
write**. The brush texture is white with a varying alpha, so with alpha taking no part in either
the blend factors or the written channels, the brush contributes its RGB — solid white — and
nothing else. The falloff is discarded.

So the production paint path draws **hard-edged rectangles**, and has done since the brush was
written in phase B. Three consequences:

1. **`BuildRadialBandTexels` is dead weight in the additive path.** A 1 by 1 white texture would
   produce identical output. It still matters for `ESurfacePaintMode::Saturating`, where alpha *is*
   the blend factor rather than a masked-out channel. The B5 table already contained this contrast
   without it being read that way: additive's first pass stored 0.09998 for an increment of 0.1,
   the full value with no falloff applied, while the translucent column's first pass stored 0.06494
   for the same increment. The additive figure is exactly what "alpha is ignored" predicts. The
   translucent figure is lower because alpha is *not* ignored there — the precise 0.065 has not
   been re-derived here and B5's explanation of it is not re-checked by this test.
2. **Any expectation of soft-edged marks is wrong.** D2 and E4 are where mark edges become a visual
   quality question, and they start from a hard edge, not a soft one.
3. **It is not a defect to fix now.** A crisp edge is what D1 wants, and the measured width is
   correct. Recorded so that the first person to want a soft edge knows why they do not have one
   and where it went.

### What is not verified

- **Only the additive path was measured.** The saturating path uses the same brush with alpha as
  the blend factor, so it should retain the falloff, but no readback confirms that.
- **Bloom is still not ruled out by measurement**, only by elimination. Setting `r.BloomQuality`
  needs console access MCP does not have, so a bloom-off capture has still never been taken.
- **The mark's shape along its length was not examined**, only its width. The end caps add half the
  width to the quad's length and nothing has checked what that looks like.

---

## 2026-09-23 — R2 re-taken on mains, and what the battery confound actually touched

The outstanding item from the R2 entry above. Six headless runs, editor closed.

```
Build:    Development, editor target, headless through UnrealEditor-Cmd.exe
Hardware: NVIDIA GeForce RTX 4070 Laptop GPU, driver 592.82, D3D12, SM6
Power:    MAINS. Win32_Battery BatteryStatus = 2 (AC connected) sampled before and after every
          run. Charge rose from 50 % to 51 % across the set, so the adapter was carrying the load
          rather than the machine merely being plugged into a dead socket. Scheme: Balanced.
Command:  -ExecCmds="Automation RunTests RacetrackSurface+RubberAccumDemo"
          -TestExit="Automation Test Queue Empty" -unattended -nopause -nosplash -nosound
Result:   36 of 36 tests pass in all six runs.
```

Two sets, for two different jobs:

- **M1, M2, M3** run the test *unchanged*. Same binary, same command, same candidates as headless
  runs A and B above. Only the power state differs, so this is the controlled comparison.
- **F1, F2, F3** run it after adding spline-point candidates 128, 192 and 384, and a printed
  crossover figure. These answer the design question the unchanged test could only bracket.

### The spread collapses from 2.4x to 5 %

Full search, 80 contacts, the figure every conclusion turns on:

| Spline points | Battery A | Battery B | Battery, third run | **Mains M1 / M2 / M3** | Mains spread |
|---|---|---|---|---|---|
| 18 | 0.5053 ms | 0.2830 ms | 0.3540 ms | **0.1383 / 0.1377 / 0.1451 ms** | 1.05x |
| 64 | 1.1915 ms | 0.5334 ms | — | **0.3148 / 0.3105 / 0.3208 ms** | 1.03x |
| 256 | 4.0437 ms | 1.7317 ms | — | **1.0915 / 1.0985 / 1.1166 ms** | 1.02x |
| 1024 | 15.5313 ms | 6.4214 ms | — | **4.0965 / 4.1411 / 4.1687 ms** | 1.02x |

Three battery runs of the same binary spread by up to 2.42x against each other. Three mains runs
spread by 1.05x at worst and 1.02x at the three larger sizes. **The battery diagnosis is confirmed
by removing the cause and watching the spread go away**, which is a stronger result than the
after-the-fact attribution the earlier entry could make.

Mains is 3.65x faster than battery run A at harness complexity, and 2.05x faster than run B.

### The editor run was not optimistic. It was the only full-clock run of the three.

The earlier entry recorded the editor run as *"optimistic by about four times against the worst
headless run"*, kept it *"for the comparison rather than as evidence"*, and stated that **"the
headless runs are the numbers of record"**. The mains figures reverse that.

| Spline points, 80 contacts | Editor run, 2026-09-22 | Mains mean, M1 to M3 | Editor / mains |
|---|---|---|---|
| 18 | 0.1332 ms | 0.1404 ms | 0.95 |
| 64 | 0.3025 ms | 0.3154 ms | 0.96 |
| 256 | 1.0773 ms | 1.1022 ms | 0.98 |
| 1024 | 4.1797 ms | 4.1354 ms | 1.01 |

The editor run agrees with the mains runs to within 5 % at every complexity. It was taken at full
clock; the two headless runs were the throttled ones. **The conclusion recorded on 2026-09-22 —
that the headless runs were the truth and the editor run flattered the result — was backwards**,
and it was backwards for a reason worth keeping: headless was assumed to be the more rigorous
method, so its numbers were trusted over the editor's on method grounds when the actual difference
between them was power state and nothing else. Rigour in the harness does not make a number right
if an uncontrolled variable is moving underneath it.

### The 1 ms crossover, measured rather than interpolated

The open question was where the full search passes 1 ms at 80 contacts, because a real 7 km circuit
carries 70 to 256 spline points depending on authoring density and the battery runs put the
crossover somewhere inside that same range. The test now samples 128, 192 and 384 as well, and
prints the crossover it finds.

| Spline points | F1 | F2 | F3 |
|---|---|---|---|
| 18 | 0.1404 ms | 0.1369 ms | 0.1375 ms |
| 64 | 0.3079 ms | 0.3054 ms | 0.3291 ms |
| 128 | 0.5695 ms | 0.5692 ms | 0.5788 ms |
| 192 | 0.8215 ms | 0.8288 ms | 0.8521 ms |
| 256 | 1.0677 ms | 1.0941 ms | 1.1014 ms |
| 384 | 1.5546 ms | 1.6207 ms | 1.6648 ms |
| 1024 | 4.1405 ms | 4.2828 ms | 4.3038 ms |
| **Crossover, printed by the test** | **238 points** | **233 points** | **230 points** |

**Roughly 234 spline points at 80 contacts.** The single-segment lookup stays flat at 0.0001 ms per
contact across the whole range, as it did on battery, so the speedup column now reaches 450x at
1024 points.

### Decision: the segment cache is still not implemented, and the reason is now a number

Against the authoring densities PLAN.md names for a 7 km circuit:

| Authoring density | Spline points | Cost at 80 contacts | Share of a 60 Hz frame |
|---|---|---|---|
| Harness circuit as built | 18 | 0.14 ms | 0.8 % |
| One point every 100 m | 70 | 0.33 ms | 2.0 % |
| One point every 27 m | 256 | 1.09 ms | 6.5 % |
| Crossover | **234** | 1.00 ms | 6.0 % |

So the sparse end of the range is clear by a factor of three and the dense end crosses. The ranges
are separated in the sense that matters — the answer now depends on a property of the circuit
someone authors, not on which run you happened to read.

**Not implemented, on three grounds:**

1. **No caller pays this cost today.** Searching the whole repo for the world-space
   `RegisterSurfaceContact` overload finds the Blueprint library wrapper and three automation
   tests. `ASplineFollowingCar` uses `RegisterSurfaceContactInTrackSpace` and performs no search at
   all, so the live five-car scene runs **zero** spline searches per frame. Every 80-contact search
   figure in this file is synthetic, and optimising a path with no users is the thing the plan's
   measure-before-optimising rule exists to prevent.
2. **The harness is 18 points**, 0.14 ms, 0.8 % of a frame.
3. **The fix stays cheap to add later.** `FindInputKeyOnSegmentClosestToWorldLocation`
   (`SplineComponent.h:837`) is measured flat at O(1), the change is localised to the conversion
   call site, and nothing about deferring it makes it harder.

**The condition to implement it, stated so it is not a judgement call next time: when a circuit
with more than about 230 centreline spline points exists, or when any world-space caller reaches
80 contacts per frame in a real scene.** Neither is true now.

One caveat against the table above, which is speculation and is labelled as such: 256 points for a
7 km circuit is one point every 27 m, and a hand-authored Spa would plausibly need several times
that through the tighter corners. At 1024 points the cost is 4.2 ms and the cache is not optional.
That is not a measurement of any circuit that exists, so it does not change the decision, but it is
the direction the risk points.

### What the battery confound touched: the game thread, not the GPU

The R2 entry above closes with a method note warning that *every* absolute figure in this file
predates the power-state rule and should be read as indicative. The full suite ran in all six runs,
so the paint and decay passes were re-taken on mains at no extra cost. They did not move.

Against the C0 "after" figures, which are the correct comparison because they are post-batching:

| Pass | Battery, C0 after, 3 runs | **Mains, F1 to F3** | Moved? |
|---|---|---|---|
| Paint, 1 contact | 0.0920 / 0.1000 / 0.0910 ms | **0.0930 / 0.0930 / 0.1100 ms** | no |
| Paint, 4 contacts | 0.0930 / 0.1050 / 0.0910 ms | **0.0920 / 0.0940 / 0.0910 ms** | no |
| Paint, 80, uniform heading | 0.0960 / 0.0890 / 0.0900 ms | **0.0920 / 0.1040 / 0.0900 ms** | no |
| Paint, 80, 80 distinct headings | 0.1060 / 0.1150 / 0.0980 ms | **0.1080 / 0.1070 / 0.0960 ms** | no |
| Full-buffer decay | 0.1290 / 0.1200 / 0.1210 ms | **0.1380 / 0.1410 / 0.1190 ms** | no |

**Battery cost the game-thread spline search a factor of 2 to 3.7 and cost the GPU-timestamped
paint and decay passes nothing measurable.** Decay is if anything marginally slower on mains, which
is run-to-run noise rather than a signal.

That result is worth more than the re-take itself, because it says which of this file's older
figures actually needed one:

- **The B3, C0 and B4 GPU pass figures did not need re-taking**, and that is now demonstrated
  rather than assumed. They are timed with RHI timestamp queries on the GPU timeline, and the
  GPU's clock was not what the power state was throttling.
- **R2 did need it**, because it is the only CPU wall-clock measurement in the file.
- **The five-car real-scene entry is the one still genuinely open.** It is a GPU timestamp, so the
  reasoning above suggests it is safe, but it also ran inside a live editor frame where the game
  thread sets the pace, and its tail — p99 0.4450 ms against a 0.0460 ms median — is contention.
  Contention with a throttled game thread is not the same measurement as contention with a fast
  one. R6 re-takes it on mains.

The B4 sweep also re-ran on mains. Its paint column now reads 0.089 to 0.116 ms across all nine
configurations against the 0.351 to 0.368 ms recorded in the original sweep. **That is C0's
batching, not the power state**, and the two must not be confused: the original sweep predates the
flush-deferral guard. The sweep's own conclusion is unchanged and is now better supported — paint
is flat to within 3 % across a 64-fold range of buffer sizes, and decay still tracks bytes touched,
2048 RGBA16f at 0.199 to 0.215 ms and 4096 RGBA32f at 1.895 to 2.323 ms.

### Unrelated finding: the B5 saturating figures have changed since phase R3

Not something this measurement went looking for. The `Accumulator.SaturationCurve` numbers printed
by all six runs differ from the B5 table recorded on 2026-09-22, identically in every run:

| Pass | B5 as recorded, pre-R3 | Current, all six runs |
|---|---|---|
| 1 | 0.06494 | **0.09888** |
| 10 | 0.48877 | **0.64795** |
| 40 | 0.93213 | **0.98682** |
| Sibling channel after 40 | 0.03406 | **0.00785** |

The additive column is unchanged to the last digit, and so is the additive sibling channel at
exactly 0.50000. Only the saturating path moved.

The cause is the R3 restructure: the stroke's texel footprint is built in track space now, so the
sampled texel sits differently under the brush than it did against the world-XY region. The
effective alpha at that texel has gone from 0.065 to 0.0989 — from 65 % of the nominal 0.1
increment to 99 % of it.

The B5 *model* still holds exactly, which is the reassuring part. B5 explained the sibling collapse
as `0.5 * (1 - a)^40` with `a` the effective alpha, and got 0.035 against 0.03406 measured. The same
model at the new alpha gives `0.5 * (1 - 0.09888)^40 = 0.00777` against **0.00785** measured. The
mechanism is unchanged; only the sampled point under the brush moved.

**This bears on a carried unknown and does not close it.** The open item is whether the saturating
path retains the brush falloff that R4 proved the additive path discards. These numbers say the
falloff is doing almost nothing *at the sampled texel* — 99 % of nominal is not a 65 % attenuation
— but one texel is not a profile, and R4's method was a profile. Settling it properly means the
`PaintedStrokeWidth` readback run against `ESurfacePaintMode::Saturating`, which is a small
addition to an existing test and is not done here.

**The B5 table above is stale against the current code and should be read as the pre-R3 figures it
is.** It is left in place rather than edited, because the reasoning built on it is sound and the
entry is the record of what was measured on the day.

---

## 2026-09-23 — R6, the real scene re-taken on the ribbon, and twenty cars at last

Closes two things at once: R6's re-take against the five-car baseline, and the "twenty cars in a
real scene is unmeasured" gap carried since C2.

```
Build:    Development, editor target binary run as a STANDALONE GAME, not Simulate-In-Editor
Hardware: NVIDIA GeForce RTX 4070 Laptop GPU, driver 592.82, D3D12, SM6
Power:    MAINS, BatteryStatus = 2 verified before and after
Scene:    /Game/Maps/L_TrackHarness, Template_Default lighting and sky, ARacetrackCircuit with its
          procedural road, BP_SurfaceRegionDisplay's 200 m plane, and the car field
Buffer:   Track-space ribbon, 3167 x 123 texels, RGBA16f, 3116328 bytes (2.97 MiB)
Cars:     Post-C3 ASplineFollowingCar, so speed, slip and line all vary with position
Method:   r.RacetrackSurface.TimePaint 1, an RHI timestamp bracket around PaintStrokes
Command:  UnrealEditor.exe RubberAccumDemo.uproject /Game/Maps/L_TrackHarness -game
          -ExecCmds="r.RacetrackSurface.TimePaint 1" -windowed -resx=1280 -resy=720
          -nosplash -nosound -abslog=...
```

| Configuration | Samples | Min | **Median** | Mean | p90 | p95 | **p99** | p99.9 | Max |
|---|---|---|---|---|---|---|---|---|---|
| 5 cars, run A | 6811 | 0.0190 | **0.0210** | 0.0237 | 0.0240 | 0.0260 | **0.0460** | 0.4320 | 0.8280 |
| 5 cars, run B | 7139 | 0.0190 | **0.0220** | 0.0257 | 0.0250 | 0.0270 | **0.1660** | 0.4360 | 0.8300 |
| 20 cars, run A | 6634 | 0.0200 | **0.0220** | 0.0246 | 0.0250 | 0.0260 | **0.0480** | 0.4390 | 0.8580 |
| 20 cars, run B | 6597 | 0.0190 | **0.0220** | 0.0247 | 0.0250 | 0.0260 | **0.0470** | 0.3810 | 0.9050 |

### Twenty cars cost the same as five, in a real scene

**0.0220 ms median at twenty cars against 0.0210 ms at five.** Every percentile up to p95 is
identical between the two configurations to within a single 0.001 ms quantisation step. This is the
headline result and it is the one the project needed: C0's batching was demonstrated headlessly on
a synthetic bench, and every 80-contact figure in this file before today was synthetic. The claim
that paint cost is flat in stroke count now holds with twenty cars actually driving.

It also retires the B3 finding that drove C0 into being a phase C prerequisite. B3 measured about
0.075 ms *per contact*; twenty cars would have been 1.5 ms on that curve. They cost 0.022 ms total.

### Against the five-car baseline, and how much of the gain is which change

| | Baseline, 2026-09-22 | **This entry, 5 cars** | Change |
|---|---|---|---|
| Samples | 1346 | 6811 / 7139 | 5x more |
| Median | 0.0460 ms | **0.0210 / 0.0220 ms** | 2.1x better |
| Mean | 0.0911 ms | **0.0237 / 0.0257 ms** | 3.7x better |
| p90 | 0.1980 ms | **0.0240 / 0.0250 ms** | 8x better |
| p99 | 0.4450 ms | **0.0460 / 0.1660 ms** | 2.7x to 9.7x better |
| Max | 1.0070 ms | 0.8280 / 0.8300 ms | 1.2x better |
| Share under 0.12 ms | 79.3 % | **98.8 to 99.4 %** | — |

**Three things changed at once and the gain must not be attributed to one of them.** In decreasing
order of how confident the attribution is:

1. **The run is standalone `-game`, not Simulate-In-Editor.** This is the biggest factor in the
   tail and the least interesting one. The baseline's mean sat at double its median and only 79.3 %
   of frames came in under 0.12 ms, which that entry already identified as contention rather than
   paint work. Take the editor out and the contention goes with it: 99.4 % of frames now land under
   0.12 ms. **This also makes the measurement compliant with this file's own standing rule**, which
   says Development or Shipping and never editor, because editor frame-rate smoothing hides spikes.
   The baseline broke that rule out of necessity, since the CVar could not be set over MCP. It can
   be set on a command line.
2. **The buffer is 11x smaller.** 3167 x 123 is 389 k texels against the square region's 4.19 M.
   B3 and B4 both measured paint as independent of buffer size, so this should account for none of
   the gain, and the flat 5-versus-20 result here agrees. Listed because it changed, not because it
   is believed to matter.
3. **The cars are post-C3**, so speed and slip vary with position. Slip sets `Stroke.Intensity`,
   not stroke count or size, so this should not move cost either.

**The honest summary is that the paint pass did not get faster; the frame around it got quieter.**
The median improvement of 2.1x is real but modest, and the p90 and p99 improvements are mostly the
editor leaving.

### The tail did not disappear, it got rarer

p99.9 is still 0.38 to 0.44 ms and the maximum is still 0.83 to 0.91 ms, which is the same order as
the baseline's 1.0070 ms. Between 34 and 66 samples out of roughly 6800 exceed 0.2 ms in every run.
So the spike is still there and still has to be budgeted for; there are simply far fewer of them.
**The number to carry for frame budgeting is p99.9 at 0.44 ms**, not the 0.022 ms median, and that
is the same conclusion the baseline reached about its own p99.

Run B of the five-car pair is the one anomaly: p99 0.1660 ms against 0.0460 ms for the other three
runs, with 66 samples over 0.2 ms against 34 to 37. Its median and p95 match the others exactly, so
this is a heavier tail in one run and not a different central cost. Not explained.

### The lap-seam split, seen outside a test for the first time

The stroke counts logged per frame are not constant:

| Configuration | 5 strokes | 6 strokes | 20 strokes | 21 strokes | 22 strokes |
|---|---|---|---|---|---|
| 5 cars | 6793 | 18 | — | — | — |
| 20 cars | — | — | 6579 | 37 | 18 |

One car crossing the lap seam turns one contact into two strokes, which is exactly what R3 built
and what until now was only asserted by `SurfaceStrokeTests`. The 22-stroke frames at twenty cars
are two cars crossing in the same frame. At five cars the field is spread evenly around the lap so
crossings never coincide, and only the 6-stroke case appears.

**This also exposed a mislabelled diagnostic.** `FScopedPaintTimer` counts `Strokes.Num()` and the
log line said "contacts". Fixed in commit `845fac0`. The baseline entry's "Contacts: 5 per frame"
was therefore reporting strokes; at five cars with no seam crossing in the sampled window the two
happen to be equal, so **no baseline number is affected**, only the word.

### Method change: this measurement no longer needs the editor or Live Coding

`EDITOR_SETUP.md` records the baseline as having been taken by temporarily editing `bEnabled` in
`FScopedPaintTimer`'s constructor to ignore the CVar, triggering a Live Coding compile, running the
session, then reverting and recompiling. That was necessary because MCP cannot set a console
variable in a running editor.

It is not necessary from a command line. `r.RacetrackSurface.TimePaint` is declared `ECVF_Default`
and read with `GetValueOnGameThread`, so `-ExecCmds` sets it at startup. The whole measurement is
now one command with no source edit to remember to revert, which also removes the risk that the
edit is left in. Recorded in `EDITOR_SETUP.md` against the old recipe.

### What is not verified

- **Frame rate is still not recorded.** `stat unit` needs console output this method does not
  capture, so the paint pass as a *share* of the frame is still unknown, only its absolute time.
  Roughly 6600 to 7100 paint calls landed in a 70 second window in every configuration, which
  implies about 95 to 100 frames per second, but the instrument synchronises the game thread with
  the render thread on every paint, so that is a perturbed frame rate and not a clean reading.
- **Nothing was seen on screen.** The material function still samples by world position against a
  track-space ribbon, so the surface renders nothing meaningful. This measures the write path only.
  R5 is what changes that.
- **The twenty-car field was 15 spawned plus the 5 still placed in the map**, reached by a
  temporary `CarCount` default that was reverted after the runs. The five map-placed cars lost
  their hand-set offsets when `LateralOffsetAlpha` was renamed to `BaselineLateralOffsetAlpha`, so
  they all drove the centreline. That affects where rubber lands, not how much work the paint pass
  does, so it does not affect these figures. It does mean **the twenty cars were not twenty
  distinct lines**, and the run should be repeated once the map is cleaned up if the spatial
  distribution ever matters to a measurement.
- **Draw-call count still has never been read from a counter.**

---

## 2026-09-23 — Deposition was two to three times the derived rate, and frame-rate dependent

Found by looking at the harness rather than by a test: rubber saturated the racing line inside a
lap when the premise of the project is buildup over a session. Two separate faults, one on top of
the other.

### Fault one: stroke intensity was the raw slip amount

`AppendStroke` set `Stroke.Intensity = Clamp(Contact.SlipAmount)`. With the harness car's cornering
slip of 0.45 and `1 - exp(-3 E)`, one contact reached **74 % coverage** and three reached 98 %.

That contradicts this file's own B6 derivation, quoted from the entry above: *"if the buildup
channel is to approach saturation over 500 contacts, the per-contact increment is 0.002"*. The code
was depositing 0.45 where the design said 0.002 — **225 times too much**. Slip is now scaled through
`RacetrackSurfaceMapping::SlipToExposure`, so a full-slip contact deposits `ExposurePerUnitSlip`,
0.002, and 500 of them reach 0.9502. Asserted in
`RacetrackSurface.Stroke.ConvertsWidthAndIntensity`.

### Fault two: a texel collected two to three deposits per pass, depending on frame rate

Fixing the rate was not enough, which is what prompted measuring instead of tuning again.

`PaintStrokes` sizes a stroke quad as `Length = Delta.Size() + Width`. The extra contact width is
there so consecutive strokes leave no wedge gap when the heading changes between frames. It also
makes neighbouring quads **overlap**, and the overlap fraction is `Width / Travel` — which depends
on how far the car moved that frame, so on frame rate and on speed.

Test `RacetrackSurface.Accumulator.DepositPerPassIsIndependentOfFrameRate`. One pass swept across a
512-texel buffer, split into 16, 64 and 256 strokes, nominal intensity 0.01 per stroke.

| Strokes over the sweep | Travel per stroke | Deposit before | Deposit after |
|---|---|---|---|
| 16 | 19.20 texels | 0.01999 | **0.01009** |
| 64 | 4.80 texels | 0.01999 | **0.00993** |
| 256 | 1.20 texels | **0.02998** | **0.00985** |
| **Spread** | | **1.50x** | **1.024x** |

Before, a pass deposited two to three times its nominal intensity and varied by half again across a
sixteen-fold frame rate range. After, it deposits the nominal rate to within 2.4 %.

The before figures land on exact multiples of 0.01 because a texel receives a **whole number** of
overlapping strokes — that quantisation is the signature of the fault and is what identified it.

**The fix is one line of reasoning: a stroke deposits for the ground it newly sweeps, not for the
length it is drawn.** Intensity is scaled by `Travel / (Travel + Width)`. Summed over the
`(Travel + Width) / Travel` strokes that cover a texel, the total is exactly one nominal deposit,
independent of both frame rate and speed.

### Why this mattered more than the visible symptom

The symptom was cosmetic — the track went red too quickly. The defect was not.

**Rubber laid per lap depended on frame rate.** Every future intensity comparison this project
intends to make would have been affected: E5 couples skid decay to local buildup, and E7 is
specified as *"two identical marks, traffic over one only, compare intensity"*. Two runs of E7 at
different frame rates would have produced different buildup for no physical reason, and the
difference would have looked like a result. The same applies to any measurement taken while
something else was loading the machine — the same class of confound as the battery one recorded
above, arriving through a different door.

**No figure already in this file is affected.** The paint and decay timings measure how long a pass
takes, not what it deposits, and the readback entries (B5, B6, R4) paint single strokes where there
is no neighbour to overlap with. The B5 and B6 tables are unchanged by the fix, which is the check
that it did not disturb the accumulator's arithmetic.

### A test was improved rather than merely repaired

`Accumulator.LinearGammaOnWrite` compared the stored value against a literal 0.5 and so broke on the
scaling. Rather than adjust the constant, it now paints two strokes at 0.5 and 0.25 and asserts the
**ratio**:

```
Additive passes at intensity 0.500 and 0.250 stored 0.3569 and 0.1804, a ratio of 1.978.
Linear encoding gives 2.000, a 2.2 encoding would give 1.370
```

That tests the property the entry is actually about — linear versus gamma encoding — without
depending on the absolute deposit at all, so it survives any future change to the deposition model.
The first version would have needed editing again every time the rate moved.

### Saturation timescale, and why it is reported at runtime rather than derived here

With the rate correct, how long the line takes to appear is arithmetic over the field size, and it
changes whenever the field or the slip values change. The circuit now prints it at startup:

```
One cornering contact deposits 0.001000 exposure. With 20 cars over one apex texel that is
0.0200 per lap, so the apex reaches 63 % coverage in about 17 laps and 95 % in about 50
```

**Fifty laps to the apex saturating is the chosen target, not a hundred and not twenty.** The
saturating curve is front-loaded — 63 % of the way at a third of the exposure — so the visible
change is mostly done by lap 20 either way, and a twenty-lap target would compress all of it into
the first six. It is also not the end of the evolution: once the apex saturates the less-committed
drivers keep depositing beside it, so the line **widens**, which is the better evidence of an
emergent racing line than the apex darkening.

A 24 hour race is about 615 laps per car and is not what the rate should be tuned to. Duration is
time compression's job, which is what the `.` and `,` controls are for.

**Not verified.** The 17 and 50 lap figures assume every car crosses the same apex texel once per
lap. That is close to true at a corner apex where the field converges and false on a straight where
it is fanned out, so the straight-line figure is not predicted at all, only asserted to be far
slower. Nothing has counted actual passes over a texel against the prediction.

---

## 2026-09-23 — Twenty cars on Spa, and the first frame rate this project has recorded

Closes the gap carried since R6: every real-scene figure before this was the oval harness, and the
Spa configuration — the one the demo actually uses and the one the presentation will show — had
never been profiled. Also closes, partly, the "frame rate is still not recorded" item that has been
open in this file since the C2 entry.

```
Build:    Development, editor target binary run as a STANDALONE GAME, not Simulate-In-Editor
Hardware: NVIDIA GeForce RTX 4070 Laptop GPU, D3D12, SM6; Intel Core i7-14700HX
Power:    MAINS, BatteryStatus = 2 at 100 %, verified in the same session
Scene:    /Game/Maps/L_SpaHarness, Spa-shaped 1500 m circuit, 20 ASplineFollowingCar
Buffer:   Track-space ribbon, 15360 x 123 texels, RGBA16f, 15114240 bytes, logged at allocation
Method:   r.RacetrackSurface.TimePaint 1, an RHI timestamp bracket around PaintStrokes
Command:  UnrealEditor.exe RubberAccumDemo.uproject /Game/Maps/L_SpaHarness -game
          -ExecCmds="r.RacetrackSurface.TimePaint 1" -windowed -resx=1280 -resy=720
          -nosplash -nosound -abslog=C:/RSBuild/spa20-A.log
```

### Paint pass, twenty cars, two runs

| Configuration | Samples | Min | **Median** | Mean | p90 | p95 | **p99** | p99.9 | Max |
|---|---|---|---|---|---|---|---|---|---|
| Spa, 20 cars, run A | 19713 | 0.0200 | **0.0300** | 0.0969 | 0.3190 | 0.3810 | **0.5480** | 0.9990 | 7.3920 |
| Spa, 20 cars, run B | 18530 | 0.0210 | **0.0360** | 0.1140 | 0.3390 | 0.4020 | **0.4970** | 0.8650 | 6.9650 |

Each run is a 201 second window, which at one paint per frame implies 98.1 and 92.2 frames per
second *with the instrument on*. That is not the scene's frame rate; see below.

### Frame time, measured separately and without the instrument

`r.RacetrackSurface.TimePaint` synchronises the game and render threads on every paint, so it
cannot be on while frame rate is being read. A second run captured 12000 frames through the CSV
profiler with the CVar left at its default of 0, and the first 1000 frames were discarded as
load and warm-up.

```
Command:  UnrealEditor.exe RubberAccumDemo.uproject /Game/Maps/L_SpaHarness -game
          -windowed -resx=1280 -resy=720 -nosplash -nosound -csvCaptureFrames=12000
```

| Counter | Median | Mean | p90 | p99 | Max |
|---|---|---|---|---|---|
| **FrameTime** | **4.484 ms** | 4.615 | 5.408 | 5.996 | 13.821 |
| RenderThreadTime | 4.470 | 4.607 | 5.408 | 5.988 | 7.674 |
| GameThreadTime | 2.369 | 2.468 | 2.955 | 3.447 | 5.308 |
| GPUTime | 2.351 | 2.398 | 2.728 | 2.885 | 3.083 |

**223 frames per second at the median, and the scene is render-thread bound.** RenderThreadTime
tracks FrameTime almost exactly (4.470 against 4.484) while the GPU sits at about half that. The
paint pass is not what limits this scene, and neither is the GPU.

### The paint pass as a share of a frame, and why that number is a cross-run comparison

0.030 ms of paint against a 2.351 ms GPU frame is **1.3 % of GPU time**, and against a 4.484 ms
frame it is **0.7 % of the frame**. Both figures combine two different runs in two different
instrument states, because no single run can produce both: the paint number requires the timestamp
bracket, and the bracket destroys the frame rate it would be compared against. The paint work
itself does not change between the two — same twenty cars, same twenty strokes per frame, same
buffer — so the comparison is fair, but it is a comparison and not a measurement.

**The number to carry for frame budgeting is p99.9 at about 1.0 ms**, which is roughly 22 % of a
4.5 ms frame. The median is not the interesting figure.

### The tail is bimodal, and that is new

| Threshold | Samples over it, run A | Share |
|---|---|---|
| 0.1 ms | 4896 of 19713 | 24.8 % |
| 0.2 ms | 2979 | 15.1 % |
| 0.3 ms | 2609 | 13.2 % |
| 0.5 ms | 277 | 1.4 % |
| 1.0 ms | 20 | 0.1 % |
| 2.0 ms | 2 | 0.0 % |

13.2 % of frames exceed 0.3 ms but only 1.4 % exceed 0.5 ms. That is not a long thin tail, it is a
**second cluster around 0.3 to 0.4 ms affecting roughly one frame in seven**. On the oval, 0.5 to
1 % of frames exceeded 0.2 ms; here it is 15 %.

**The cause is not established and three things changed at once.** The buffer is about five times
larger in texels (1.89 M against 389 k); the scene is far bigger (1500 m of road mesh against
309 m); and unlike the R6 run, the road is now actually rendering the debug material, so the GPU is
doing real work alongside the paint. B3 and B4 both measured the paint work itself as independent
of buffer size, which argues against the first explanation for the *work* — but neither measured
render-pass setup on a target 15360 texels wide, which is a different cost and is not covered by
that evidence. Attributing this to any one of the three would be a guess.

### The lap seam, again

| Configuration | 20 strokes | 21 strokes |
|---|---|---|
| Spa, 20 cars, run A | 19653 frames | 60 frames |

One car crossing the lap seam turns one contact into two strokes, exactly as on the oval. No
22-stroke frames appeared in this window, so no two cars crossed in the same frame.

### A correction to the recorded buffer size

`EDITOR_SETUP.md` records the 1500 m ribbon as 15361 x 123 texels at 15115224 bytes. This run
logged **15360 x 123 at 15114240 bytes**, one texel shorter along the track. Both are real logged
values from real runs; the rescaled spline length is only reported to one decimal place, so a lap
length a fraction under 150000.0 cm in one run and a fraction over in the other is enough to move
the `ceil`. The figure to quote is the one from the run being discussed.

**Not verified.** The decay pass is still unmeasured at this buffer size, or at any size above the
oval's — it is the pass that scales with texel count rather than contact count, and it is still
never called from the running game, so there is nothing to instrument in a real scene. The bimodal
tail is unexplained. The 223 fps figure is one scene on one machine at 1280 x 720 windowed with
the template's lighting, and nothing has been varied to see what moves it. No draw-call count has
been read from a counter, which remains true since C0.

---

## 2026-09-23 — C6: counting passes over a texel, and finding the prediction wrong in both directions

Closes the gap this file has carried since phase C: *"Nothing has counted actual passes over a
texel against the prediction."* Something has now, and the prediction did not survive it.

```
Build:    Development editor target, Live Coding patch 9, automation run in-editor
Hardware: NVIDIA GeForce RTX 4070 Laptop GPU; Intel Core i7-14700HX
Scene:    Default Stadium layout, 30921 cm lap, TrackWidth 1200 cm, spawned in an editor world
Field:    20 cars, DriverLateralSpread 0.075, baselines and commitments from
          ARacetrackCircuit::GetDriverBaselineLateralOffsetAlpha / GetDriverLineCommitment,
          seeds from GetDriverSeedForGridIndex, FieldRandomSeed 20260923
Method:   RubberAccumDemo.Track.CountedPassesOverATexelAgainstThePrediction. Ticks the field at
          1/60 s for 12000 ticks, detects each car crossing a chosen along-track distance by the
          change in floor((Distance - Chosen) / Length), and tests that car's LateralOffsetAlpha
          against each of the 123 lateral texels for contact-patch coverage. No GPU, no readback:
          this counts the contact geometry the deposition rate is derived from, not the painted
          buffer.
Samples:  the midpoint of the longest run below 0.02 cornering, and of the longest run above 0.98
```

### What was predicted, and what was counted

| | texels inked | passes per texel per lap | exposure per lap | laps to 63 % |
|---|---|---|---|---|
| Busiest straight texel | 14 | **8.29** | 0.001658 | ~201 |
| Busiest apex texel | 29 | **2.45** | 0.002447 | ~136 |
| Straight, predicted from stripe spacing | — | 4.22 | — | — |
| Apex, assumed by the one-texel figure | — | 20 | — | — |

**Measured contrast: 1.5 to 1.** `PLAN.md` predicted 5:1 after C6, against 100:1 before it.

Three separate errors, and they partly cancel, which is why the plan's conclusion reads as
roughly right while both of its rows are wrong.

**The straight row was too optimistic by five.** It took `20 * 0.0002` per straight texel, i.e.
the whole field crossing one texel. But `AdjacentSpacing <= ContactWidth / 4` was chosen to give
*four*-fold overlap precisely so the stripes merge without the field collapsing onto one line, so
the arithmetic that sizes the spread and the arithmetic that predicts the exposure contradict each
other. At 4.737 cm spacing against a 20 cm mark the static prediction is 4.22 cars per texel.

**The straight count is nevertheless twice that static prediction, at 8.29.** Cars are not at
their baselines in the middle of a straight. `LineChangeResponsiveness` is 2.0, a half-second time
constant, and the oval's straight is about two seconds long, so the field is still lerping back
from the previous corner when it crosses the sample point and is bunched tighter than the grid
spacing puts it. The static formula describes a steady state this layout never reaches.

**The apex row was too optimistic by eight.** The one-texel assumption was already flagged in this
file as approximate because `DriverLineCommitmentAtCentre/AtEdge` are 0.2 and 0.8 rather than 1.0.
C6 makes it much worse on purpose: `MaxCornerDeviationAlpha` of 0.20 spreads the arcs over 29
texels, 283 cm, against the straight band's 14 texels and 137 cm. That 2:1 width ratio is exactly
what the plan asked for and is the visible fan the feature exists to produce — but spreading the
same twenty passes over 2.5 times the area drops the busiest apex texel to 2.45 passes, *below*
the straight's 8.29.

### The consequence the plan did not work out

The apex wins on exposure only because a cornering contact carries five times the slip of a
straight one. Geometry now works against it. Net, a corner texel darkens 1.5 times faster than a
straight one, where before C6 it was 100 times faster.

So **`MaxCornerDeviationAlpha` is a direct trade between how wide the fan is and how much darker
the line is**, and the plan treats it as a free parameter tuned by eye for the fan alone. At 0.20
the fan is clearly visible and the line is barely darker. Lowering it concentrates the apex and
narrows the fan. Which end of that trade looks right is a judgement to make on a capture, not a
number to derive, and it has not been made yet.

This is also a second, independent argument for phase E. With no decay, a 1.5:1 rate ratio means
the straight is at 45 % coverage when the apex reaches 63 %, and the two are visually close from
the start rather than separating first and converging later.

### Not verified

These are oval figures. The demo runs Spa, whose corners are graded rather than on-or-off and
whose straights are shorter relative to the lateral settling time, so both the residual-convergence
inflation and the apex spread will differ and neither has been counted there. Nothing here touches
the painted buffer: this counts contact-patch coverage, which is the input to the deposition
arithmetic, not its output, so a bug between `RegisterSurfaceContactInTrackSpace` and the render
target would not show up in it. The 12.67 laps are enough for the straight figure, which is nearly
deterministic, and thin for the apex figure, which depends on a fresh triangular draw per car per
corner. No screenshot has been taken of the resulting buffer, so the claim that the fan is visible
as separable arcs is still arithmetic and not an observation.

---

## 2026-09-23 — C8: four tyre contacts per car, and the circuit rescaled to resolve them

Closes two things at once, because they turn out to be one thing. Every figure below is from a
single headless run with the editor closed.

```
Build:    Development editor target, run headless through UnrealEditor-Cmd.exe
Hardware: NVIDIA GeForce RTX 4070 Laptop GPU, D3D12, SM6; Intel Core i7-14700HX
Power:    MAINS, BatteryStatus = 2 at 100 %, read from Win32_Battery in the same session
Scene:    Editor world, actors spawned by the tests. Stadium 30921 cm and Spa 48000 cm layouts
Method:   RubberAccumDemo.* and RacetrackSurface.* automation tests
Command:  UnrealEditor-Cmd.exe RubberAccumDemo.uproject
          -ExecCmds="Automation RunTests RacetrackSurface+RubberAccumDemo"
          -TestExit="Automation Test Queue Empty" -unattended -nopause -nosplash -nosound
          -abslog=C:/RSBuild/tests-c8-04.log
Result:   55 of 55 pass
```

### The ribbon, both layouts, against the budget

`RubberAccumDemo.Track.BothLayoutsFitTheCapAndTheMemoryBudget`. Byte counts are arithmetic over
the texel extents the ribbon reports, not a real allocation; the 1500 m predecessor's figure was
confirmed against a real allocation in the R6 entry and the format has not changed.

| Layout | Lap | Along | Across | Bytes | MiB | 20 cm patch |
|---|---|---|---|---|---|---|
| Stadium | 30921 cm | 9895 | 256 | 20264960 | 19.33 | 6.40 texels |
| **Spa** | **48000 cm** | **15360** | **256** | **31457280** | **30.00** | **6.40 texels** |
| *Spa as it was* | *150000 cm* | *15360* | *123* | *15114240* | *14.41* | *2.05 texels* |

Both axes divide exactly: `48000 / 3.125 = 15360` and `800 / 3.125 = 256`, so neither carries
`ceil` slack. The along-track axis keeps **1024 texels of margin** under the 16384 cap, which is
the same margin the 1500 m layout had and is the reason 400 m at 2.44140625 was rejected — that
pairing lands on 16384.0 exactly, and this project has already seen the same rescale log 15360 in
one run and 15361 in another.

### The stored mark is three times finer, read back rather than derived

`RacetrackSurface.Accumulator.PaintedStrokeWidth`, now sweeping three densities.

| cm/texel | quad texels | peak | inked texels | inked cm |
|---|---|---|---|---|
| 9.765625 | 2.048 | 0.9800 | 2 | 19.53 |
| **3.125** | **6.400** | **0.9409** | **6** | **18.75** |
| 2.441406 | 8.192 | 0.9258 | 8 | 19.53 |

A 20 cm contact patch stores as 6 texels spanning 18.75 cm. The stored width tracks the asked-for
patch to within one texel at all three densities, so the rescale buys resolution without changing
what a mark measures.

### The bicycle model, validated against a formula the code does not contain

`RubberAccumDemo.Car.LaysFourTracksInACornerAndTwoOnAStraight`. The car's tracked point is the rear
axle and the front axle sits one wheelbase along the body heading, so offtracking is a consequence
of the pose rather than a term anywhere in the source. Measured on the Stadium's constant 3000 cm
radius with a driver at zero line commitment, so nothing but the wheelbase separates the axles:

| | measured | `Wheelbase² / (2R)` |
|---|---|---|
| Rear inside front, steady 3000 cm corner | **3.156 cm** | 3.267 cm |
| Same car on a straight | **0.063 cm** | 0 |
| Left to right separation | 79.92 cm | 80 cm wheel track |

**3.4 % off the closed form.** On a straight the axles agree to 0.6 mm, so a car running straight
lays two tracks and not four — which is what a car running straight does.

### Four tracks are resolvable only at the rescaled size

Same test, one driver over a settled lap of the 48000 cm Spa, 4587 sampled ticks:

| | value |
|---|---|
| Widest front-to-rear separation | **56.76 cm** |
| As mark widths | **2.84** |
| Share of the lap exceeding one mark width | **5.1 %** |
| Widest separation where the track reads as straight | 16.81 cm |

At the old 1500 m scale the same geometry gives about 7.8 cm of steady-state offtracking, which is
**0.8 texels** at 9.7656 cm and below the resolution of the buffer. The rescale is what makes four
wheels four marks; without it two of the four would be redundant.

**Two effects separate the axles and they oppose each other, which the plan missed.** Steady-state
offtracking puts the rear inside the front. During turn-in the body is yawed into the corner, so
the front axle is displaced toward the inside instead, and at the lateral rates this model reaches
that term is the larger. The first version of this test asserted "rear inside front" at the
tightest point on the lap and measured the front 19.43 cm inside the rear. Both are correct
vehicle behaviour. The test now checks the sign where there is a genuine steady state to check it
in, and checks only resolvability on Spa.

### The lockup

`RubberAccumDemo.Car.LocksOneFrontWheelUnderBraking`, 30000 ticks on seed 77003, two cars.

| | value |
|---|---|
| Braking events drawn | 127 |
| Ticks holding a locked wheel | 662 |
| Of those, ticks while braking | **662 of 662** |
| Front left / front right | 359 / 303 |
| Rear wheels locked | **0** |
| SkidMark contacts while locked | 662 of 662 |
| RubberBuildup contacts while locked | 1986, exactly three per tick |
| Ticks where the two seeded cars disagreed | **0** |

Exactly one of the four wheels deposits on `SkidMark` while a wheel is locked and the other three
keep laying rubber, which is the behaviour the change exists for. The lockup draws from a second
`FRandomStream` offset from `DriverSeed`, so the C6 corner-deviation sequence is untouched and its
measured distribution stays reproducible.

### Nothing falls off the ribbon

`RubberAccumDemo.Track.NoWheelFallsOffTheRibbon`, twenty cars for 6000 ticks on the 800 cm track.
The widest wheel reached alpha **0.7864**, leaving 0.2136 of margin, and **zero** contacts were
discarded outside the ribbon. This is the check that made 800 cm the choice over 600: a contact at
`|alpha| > 1` is dropped into a counter nothing reads, so it is silent, and the wheels now sit
50 cm off the car's own line.

### Saturation, re-counted per wheel

`RubberAccumDemo.Track.CountedPassesOverATexelAgainstThePrediction`, the same 12.67 laps of a
twenty-car field, now counting each wheel through `WorldToTrack` rather than each car's own alpha.

| | texels inked | span | passes per texel per lap | exposure per lap | laps to 63 % |
|---|---|---|---|---|---|
| Busiest straight texel | 57 | 178 cm | **21.39** | 0.004278 | ~78 |
| Busiest apex texel | 90 | 281 cm | **12.08** | 0.012077 | ~28 |
| Straight, predicted from stripe spacing x 2 coincident wheels | — | — | 12.67 | — | — |

**Measured contrast 2.8 to 1, against 1.5 before C8.** Both rates roughly doubled because halving
the track halves `AdjacentSpacing` and packs the field into half the width. The contrast improved
on top of that because a car's four wheels straddle the corner band while on a straight the front
and rear coincide, so the apex gains a pair the straight already had. The straight count is again
about 1.7x the static prediction, the same residual convergence the C6 entry found at 1.96x, and
that ratio is now an assertion rather than a note.

**The apex band is 90 texels against 29 and 281 cm against 283.** The feature is the same size on
the track and resolved three times more finely, which is the rescale doing its job.

### The price of going through the world-space API

`RacetrackSurface.Performance.TrackSpaceConversion`, with 77 added to the spline-point candidates
because that is what `RacetrackLayouts::SpaCentreline` holds.

| Spline points | Contacts | Full search | per contact | Known segment | Speedup |
|---|---|---|---|---|---|
| 18 | 80 | 0.1378 ms | 0.0017 | 0.0086 ms | 16x |
| 64 | 80 | 0.3036 ms | 0.0038 | 0.0071 ms | 43x |
| **77** | **80** | **0.3658 ms** | **0.0046** | **0.0087 ms** | **42x** |
| 128 | 80 | 0.5566 ms | 0.0070 | 0.0097 ms | 57x |
| 256 | 80 | 1.0574 ms | 0.0132 | 0.0082 ms | 129x |

**0.3658 ms for the twenty-car field's eighty wheel contacts**, 2.2 % of a 60 Hz frame and about
8 % of the 4.484 ms frame this scene actually runs at. The 1 ms crossover moved to 236 spline
points, consistent with the 234 measured on the mains re-take.

**This trips R2's own written trigger**, which said to implement the per-contact segment cache
"when a circuit exceeds about 230 centreline spline points, **or when any world-space caller
reaches 80 contacts per frame in a real scene**". Twenty cars times four wheels is exactly 80. The
cache is measured at **0.0087 ms, 42 times cheaper and flat in spline points**, and is
deliberately still not implemented: it is plugin optimisation that was not asked for, and the
point of writing the trigger down was to make the decision visible rather than automatic. The cost
of not taking it is now a number.

### A defect this found, which predates it

`ARacetrackCircuit::PostInitializeComponents` was the only caller of `ConfigureTrackRibbon`, and
`AActor::PostActorConstruction` only runs `PostInitializeComponents` when
`World->AreActorsInitialized()` (`Actor.cpp:4436`), which is false in the editor world the
automation tests spawn into. **No host test had ever configured the surface ribbon.** It cost
nothing while the car used the track-space overload, which needs no centreline. The moment the
wheels went through the world-space API, every contact was discarded for want of one, and the
counting test was silently measuring against the ribbon struct's own defaults — 1200 cm wide at
9.7656 cm per texel — rather than against the circuit in front of it. The first run of this suite
reported `82 lateral texels at 9.7656 cm` on an 800 cm track and looked plausible.

The fix calls `PublishTrackRibbon` from `OnConstruction` and `SetLayout` as well, which also means
editing `TrackWidth` or the layout in the editor now resizes the ribbon instead of waiting for the
next play session.

### The road mesh folds, it always has, and this makes it about twice as deep

Measured as the tightest three-point radius over one `RoadSegmentLength` ring spacing, which is the
scale at which an offset curve folds — adjacent rings' inside vertices cross when the radius falls
below half the track width.

| | tightest radius | half width | ratio |
|---|---|---|---|
| Spa at 1500 m, 1200 wide, 400 cm rings | 277 cm | 600 | 0.46 |
| **Spa at 480 m, 800 wide, 128 cm rings** | **89 cm** | **400** | **0.22** |
| Stadium, 128 cm rings | 352 cm | 400 | 0.88 |

Ratios below 1 mean the inside edge of the generated road passes through its own centre of
curvature. **All three are below 1, including the layout as it shipped.** The cause is that the 77
centreline points were read off a plan view by eye, which `EDITOR_SETUP.md` already records as not
survey-accurate, so the data carries local kinks far tighter than the corners they describe.

Holding the measuring chord at a fixed fraction of the lap makes the ratio proportional to
`LapLength / TrackWidth`, which was 125 and is now 60. **The 2.08x worsening is arithmetic, not a
bad choice**: the cars are a fixed physical size, so a shorter lap must carry a proportionally
wider track. Holding the old ratio at 480 m would need a 384 cm track, which fails the wheel-alpha
margin above.

The Stadium's 0.88 is a different and smaller thing — a curvature spike at the straight-to-corner
knot, where a Catmull-Rom through generated points has no curvature continuity.

#### Corrected the same day, on looking at it: this is a hard constraint, not a degradation

The entry above treats the fold as a pre-existing defect made worse. Seen in a close-up of a slow
corner, the road visibly **passes through itself**, and the ratio is not a quality metric — below
1.0 the offset curve is undefined and the mesh self-intersects.

The lap length required for a valid road, derived from the authored shape's own radii:

| `TrackWidth` | never folds above | comfortable above | authored kinks clean above |
|---|---|---|---|
| 800 cm | 477 m | 954 m | 1726 m |
| 1000 cm | 596 m | 1192 m | 2158 m |
| 1200 cm | 715 m | **1430 m** | 2589 m |

**480 m at 800 cm clears the bare threshold by 3 cm** — 403 cm of radius on the fastest corner
against a 400 cm half width — and fails everywhere tighter. **The pre-C8 1500 m at 1200 cm sat
just above the comfortable line at 1430 m.** That configuration was not chosen against this
constraint and nobody had written it down; it happened to land on the right side of it.

So C8's texel decision holds and **C8's lap decision does not**. No setting keeps the along-track
axis under 16384 at 3.125 cm per texel and reaches even the 954 m minimum, which needs 30528
texels. This is now the primary argument for folded rows: not resolution, but that a valid road
geometry and a resolvable tyre mark are otherwise mutually exclusive.

#### And the elevation cannot be raised to compensate

Tried at `ElevationExaggeration` 7.8125, which restores the pre-C8 absolute rise, and reverted.
The authored Z values are integers and **35 of the 76 points step by exactly one unit**, so the
profile has a quantisation floor:

| Lap | `ElevationExaggeration` | One integer Z step | Gradient step |
|---|---|---|---|
| 480 m | 2.5 | 25 cm | 4.0 % |
| 480 m | 7.8125 | 77 cm | **12.4 %** |
| 7004 m | 2.5 | 362 cm | 4.0 % |

At 12.4 % the circuit visibly terraces. The gradient step depends only on the exaggeration, not on
the lap, so a longer lap restores the absolute drama at the same 4 % step. Another route to the
same conclusion as the fold.

### Not verified

**Nothing here was seen on screen.** Every figure is from a headless automation run. The demo map
still carries `AuthoredLayoutLapLength` 150000 and `RoadSegmentLength` 400 as per-instance
overrides on `RacetrackCircuit_0`, so `L_SpaHarness` continues to run the old 1500 m circuit until
those are changed; the recipe is in `EDITOR_SETUP.md`. That means **no real-scene paint timing,
frame time or buffer capture exists at the new scale**, and the 0.3658 ms conversion figure has not
been confirmed against a running game — it is a bench measurement on a synthetic 77-point circle,
not on the Spa spline itself.

Whether the road-mesh fold is visible is a judgement for a capture and has not been made. Whether
six texels per mark actually reads as a tyre mark rather than a block, which is the whole purpose
of the rescale, is the same kind of judgement and is also unmade. The decay pass remains unmeasured
at any buffer size above the oval's. The `LockupProbability` of 0.15 is a by-eye demo value and
nothing has judged whether 127 braking events producing 662 locked ticks per 30000 looks right.

---

## 2026-09-23 — C9: folded ribbon rows, and what the fold actually does

Build: `RubberAccumDemoEditor Win64 Development`. All 63 automation tests pass headlessly with the
editor closed, and both build paths are clean — `BuildPlugin` adaptive non-unity and the host unity
target, which C8 established are not interchangeable checks.

Test command:

```
UnrealEditor-Cmd.exe RubberAccumDemo.uproject
  -ExecCmds="Automation RunTests RacetrackSurface+RubberAccumDemo"
  -TestExit="Automation Test Queue Empty" -unattended -nopause -nosplash -nosound
  -abslog="C:/RSBuild/tests-c9-04.log"
```

### The ribbon, both layouts, folded

| Layout | Lap cm | Along | Across | Rows | Atlas | Bytes | MiB | Mark texels | Tightest radius vs half width |
|---|---|---|---|---|---|---|---|---|---|
| Stadium | 30921 | 9895 | 256 | **1** | 9895 x 256 | 20264960 | 19.33 | 6.40 | 352 vs 400, ratio 0.88 |
| **Spa** | 180000 | 57601 | 256 | **4** | **14401 x 1024** | **117972992** | **112.51** | **6.40** | 191 vs 400, ratio 0.48 |

```
Spa layout: 180000.0 cm of lap at 800.0 cm wide, 3.125000 cm per texel,
            57601 x 256 texels in 4 rows of 14401, allocating 14401 x 1024
            for 117972992 bytes at RGBA16f, and a 20.0 cm contact patch stores as 6.40 texels
```

**The Stadium stays unfolded at one row, and that matters more than it looks.** It means the
`RowCount == 1` identity path is exercised by a real layout on every test run rather than only by a
synthetic case, so the unfolded path cannot rot while the folded one is developed.

**The plan said 14400 x 1024 and 117964800 bytes. The measurement says 14401 and 117972992.** The
Spa spline is rescaled about its centroid to hit `AuthoredLayoutLapLength` and then measured, and
it lands a hair over 180000 cm, so `ceil` gives **57601** along-track texels rather than 57600 and
each of the four rows carries one spare texel. The cost is 8192 bytes, 0.007 %. This is exactly the
effect C8 rejected the 400 m / 2.44140625 pairing for — a design that is correct only when a
floating-point rescale rounds the right way — and it is the reason the byte assertion in
`SpaLayoutFoldsIntoFourRows` is a one-percent tolerance rather than an equality. The exact figure
is still asserted, against an exactly-constructed ribbon, in
`RacetrackSurface.Ribbon.FoldsIntoRowsBeyondTheUnfoldedLimit`.

Real Spa at full size, which nothing runs but which the model now admits:

```
700400 cm by 1500 cm at 3.1250 cm/texel: 224128 x 480 texels in 14 rows of 16010,
allocating 16010 x 6720, 860697600 bytes (860.7 MB) at RGBA16f
```

Both atlas dimensions stay under the 16384 cap at fourteen rows, so the full circuit is now a
memory decision rather than an impossibility. 860.7 MB against this laptop's 7948 MB of dedicated
VRAM is 10.8 %.

### The fold is correct, measured three independent ways

**The write side puts ink in the row the mapping predicts.** A contact three quarters of the way
around a two-row ribbon, read back as a single column of the atlas:

```
Column 4999 of a 10000 x 64 atlas: first row peaks at 0.000000,
second row peaks at 0.001802, the ink centres on texel 48.00
```

Predicted centre was texel 48. The peak value is the deposit predicted too: one contact at slip 1.0
is `0.002` of exposure, and the swept share over a 61.3 px travel with a 6.4 px brush is 0.9055, so
`0.002 x 0.9055 = 0.001811` against 0.001802 measured, the difference being float16 storage.

**The track edge does not bleed into the next row.** A 60 cm brush centred exactly on `alpha = 1` of
row 0, which overhangs the row band by half its width:

```
row 0 peaks at 0.094299, row 1 peaks at 0.000000
```

Without the per-stroke scissor that overhang lands in row 1's first across-texels, which is rubber
from one part of the lap appearing at the far edge of a different part. **This is a defect folding
introduces and which nothing else in the design catches**, because the stroke content is otherwise
entirely correct.

**The read side returns the mark where the write side put it.** This is the end-to-end guard: a
stroke painted through the public C++ API, the real `M_SurfaceAccumulationDebug` drawn into a small
render target through `DrawMaterialToRenderTarget`, and that target read back.

```
Painted ribbon 0.700..0.800 across 0.500 into row 1 of 2 on a 10000 x 128 atlas;
sampled back centred on 0.750, 0.500, peaking at 0.6948,
with 0.0000 at the same column of the first row
```

**0.750 and 0.500 against 0.750 and 0.500.** The C++ fold and the HLSL fold agree through a real
compiled shader, and the 0.0000 is the discriminator: it is the place a read that ignored the row
count would have put the mark. This also confirms the collection parameter resolves, which could
not be verified directly — `ParameterId` is not readable through the MCP tool — because an
unresolved row count reads 0, the shader falls back to one row, and the mark lands elsewhere.

### Four tracks at 1800 m, and a test that had stopped measuring

`RubberAccumDemo.Car.LaysFourTracksInACornerAndTwoOnAStraight` failed twice on the way here, for two
unrelated reasons, and both are worth recording.

**First, it had stopped sampling a lap.** `TicksAcrossTwoLaps = 6000` was sized against a 480 m lap.
At 1800 m the first lap alone takes about 5227 ticks, so the test sampled **773 ticks** of the
second lap and drew conclusions from them. The loop now runs until the car has actually completed
two laps, which is what its name always claimed, and samples **5240**.

The numbers changed substantially once it measured a whole lap:

| | C8, 480 m lap | **C9, 1800 m lap** |
|---|---|---|
| Widest front-to-rear separation | 56.76 cm | **58.73 cm** |
| As mark widths | 2.84 | **2.94** |
| Share of lap over one mark width | 5.1 % | **7.7 %** |

**The longer lap made four-track resolvability better, not worse.** C8 predicted the opposite —
"a longer lap makes this worse, not better, and that trade has not been faced" — on the reasoning
that offtracking is `Wheelbase^2 / (2R)` and a longer lap has gentler corners. That reasoning is
sound for the steady-state term and wrong about which term dominates. C8 itself found the transient
is larger: during turn-in the body is yawed into the corner and the front axle is displaced toward
the inside. That term scales with lateral rate, not with radius, and the longer lap does not reduce
it.

**Second, and more interesting: the separation no longer falls away on straights.** Measured across
three bands of the cornering profile, on the settled lap:

| Below this share of sustained cornering | Share of lap | Widest separation | Share of the corner maximum |
|---|---|---|---|
| 0.05 | 8.3 % | 30.09 cm | 51 % |
| 0.02 | 3.0 % | 28.93 cm | 49 % |
| **0.01** | **1.5 %** | **28.93 cm** | **49 %** |

Tightening the band from 8.3 % of the lap to 1.5 % moves the separation by 1.16 cm. **Even on the
straightest track the layout has, the axles are 28.93 cm apart, which is 1.45 mark widths.** At 480 m
the same test asserted the straight separation was under half the corner maximum and it held; here
it lands at 49 %, and a test that passes at 49 % against a 50 % bar is passing by luck rather than
by property.

The mechanism is that `GetCorneringAmountAtDistance` is normalised against a percentile of the
**layout's own** curvature distribution, so "below 0.05" is a relative measure that moves with the
lap, and more importantly it describes the *centreline*, not the car. A car on a racing line is
crossing the track between corners, so its heading differs from the centreline even where the
centreline is straight, and at 1800 m the transitions between corners are long enough that the car
is mid-transition over most of what the profile calls straight.

**The assertion was replaced rather than loosened.** It now requires a corner to add more than a
whole contact patch of separation on top of whatever the straightest track shows — measured,
`58.73 - 28.93 = 29.80 cm`, which is 1.49 mark widths. That is a physical margin rather than a
tuned ratio. The "two tracks on a straight" claim is still tested, on the Stadium, where there is
genuinely straight track: C8 measured 0.063 cm of separation there and that assertion is unchanged
and still passes.

### What this does not measure

**Nothing here has been looked at on screen.** The fold is verified numerically and through a
compiled shader, but no capture has been taken at the 1800 m configuration, and `L_SpaHarness` still
carries the interim 960 m overrides at the time of writing, so the demo map does not yet run this
configuration at all.

**The cost of the read path is unmeasured.** The shader went from 9 texture fetches per road pixel
to 18, because every tap of the 3x3 tent now resolves two neighbouring texels and blends them by
hand rather than letting the sampler do it. No frame time or GPU time has been captured at the new
configuration, so the 4.484 ms frame and 2.351 ms GPU from R6 remain the last real figures and they
were taken at a different lap, a different density and a different shader.

**The cost of the per-stroke scissor is unmeasured.** It forces a canvas flush whenever consecutive
strokes belong to different rows, where the existing code deliberately defers every flush to batch
the whole frame's strokes into one. With twenty cars laying four contacts each the row changes
should be few, but "should be" is not a measurement, and the 0.09 ms paint figure for 80 contacts
predates this.

**The road-mesh fold ratio is not settled.** The tightest three-point radius over one road segment
measures 191 cm against a 400 cm half width, a ratio of 0.48, against 0.22 at C8's 480 m and 0.46 at
the pre-C8 1500 m layout that looked right. That measure is a discrete curvature estimate that
spikes at spline knots rather than a true local radius — C8 reached the same conclusion about the
Stadium's 0.88 — so whether a slow corner still passes through itself is a judgement for a capture
and that capture has not been taken.
