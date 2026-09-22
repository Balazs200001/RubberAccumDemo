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
