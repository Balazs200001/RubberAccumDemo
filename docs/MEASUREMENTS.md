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

---

## 2026-09-23 — C9 follow-on: the racing line was diluted by a unit, not by a design decision

Not part of the fold, but found while looking at the folded surface on screen and fixed in the same
phase. The complaint that started it: *"the cars still put way too much rubber down on straights"*,
and separately *"the deposition was perfect before the track shrank"*.

The second remark turned out to be the diagnosis.

### `DriverLateralSpread` is in alpha, and C8 moved the track out from under it

`ARacetrackCircuit::DriverLateralSpread` is a fraction of the **track width**, not a distance.
C8 (`527fa2b`) narrowed `TrackWidth` from 1200 cm to 800 cm and left the spread at 0.075, so the
field's physical spacing changed without anyone editing it:

| | Half width | Field span | Adjacent baselines |
|---|---|---|---|
| Pre-C8, 1200 cm track | 600 cm | 90 cm | **4.74 cm** |
| Post-C8, 800 cm track | 400 cm | 60 cm | **3.16 cm** |

**The field packed 1.5 times tighter than it had, silently.** More cars crossing each straight
texel is more rubber on straights, which is exactly what was reported and exactly when it started.

`0.075 x 1200 / 800 = 0.1125` restores the pre-C8 physical spread, and the measurement confirms it
lands where it should: **4.737 cm against the pre-C8 4.7368 cm.**

### Three changes, and what each was for

| Property | Was | Now | For |
|---|---|---|---|
| `StraightSlipAmount` | 0.10 | **0.05** | The authored slip ratio against cornering becomes exactly 10:1 |
| `MaxCornerDeviationAlpha` | 0.20 | **0.30** | More visible variation between drivers through a corner |
| `ApexLateralOffsetAlpha` | 0.70 | **0.60** | Keeps the widened deviation clamping at the same 0.90 outer limit |
| `DriverLateralSpread` | 0.075 | **0.1125** | Restores the pre-C8 physical spread |

`CorneringSlipAmount` was deliberately **not** raised. `LockedWheelSlipAmount` is 1.0, and moving
cornering slip toward it would erase the margin that makes a locked wheel read as one dark mark
among four, which is C8's exit condition. Halving the straight buys the contrast without spending
that margin; the locked-to-cornering ratio stays at 2:1.

The apex widening is symmetric on purpose. `ApexTarget = clamp(ApexLateralOffsetAlpha + deviation,
+/-0.90)`, and at 0.70 with a +/-0.20 deviation the clamp already bit exactly at 0.90. Widening the
deviation alone would have piled draws against that edge and pushed wheels toward the ribbon
boundary, where C8 measured the widest wheel at alpha 0.7864 with zero contacts discarded. Moving
the nominal apex in to 0.60 as the range widened to +/-0.30 keeps the outer limit at 0.90 and the
range symmetric about the nominal line.

### Measured, three configurations

Counted by `RubberAccumDemo.Track.CountedPassesOverATexelAgainstThePrediction`, twenty cars over
12.67 laps, sampled at the middle of the longest straight and the longest apex.

| | Post-C8 baseline | **Chosen** | Rejected |
|---|---|---|---|
| `DriverLateralSpread` | 0.075 | **0.1125** | 0.25 |
| Adjacent baselines | 3.16 cm | **4.737 cm** | 10.53 cm |
| Busiest straight texel | 21.39 passes | **15.16** | 9.47 |
| Busiest apex texel | 12.08 passes | **12.47** | 12.79 |
| Straight exposure a lap | 0.004278 | **0.001516** | 0.000947 |
| Apex exposure a lap | 0.012077 | **0.012471** | 0.012787 |
| **Contrast** | **2.8 to 1** | **8.2 to 1** | 13.5 to 1 |
| Straight to 63 % | 78 laps | **220 laps** | 352 laps |
| Apex to 63 % | 28 laps | **27 laps** | 26 laps |
| Stripes merge | yes | **yes** | **no** |

**The apex is essentially untouched across all three.** Every bit of the contrast comes from
diluting the straight, which is the right place for it to come from — the racing line should be
where the field converges, not where it is drawn brighter.

**0.25 was tried and rejected on a test, not on taste.**
`RubberAccumDemo.Track.SpawnedStripesMergeOnTheStraight` requires adjacent baselines within a
quarter of the 20 cm contact patch, so each wheel band is crossed at least fourfold and the field
reads as one merged band rather than separate stripes. At 0.25 the baselines are 10.53 cm apart and
that test fails. **The 13.5:1 contrast at 0.25 is therefore partly a return to the pre-C6 world**,
where separate stripes gave 100:1 and the racing line was drawn rather than emergent. The merge
limit is `800 * spread / 19 <= 5`, so **spread <= 0.11875**, and 0.1125 sits under it with margin
while also being the figure the C8 track narrowing implies.

That constraint is the reason the sweep stops where it does. An offline model produced during this
work recommended 0.15 on contrast grounds alone; at 0.15 the baselines are 6.32 cm apart and the
merge test fails. The model had not been told about the constraint, and the measurement was right.

### What this does not fix

**The 2.8:1 was never only a slip problem and the remaining 8.2:1 is not either.** The circuit's own
startup log still predicts 15.8:1 from the stripe arithmetic, and it is still wrong for the reason
it states about itself: it assumes the whole field crosses one apex texel. Measured, the field fans
across **294 cm at the apex** against **203 cm on the straight**, so the apex is still the more
diluted of the two. Converging the field in corners rather than spreading it would raise the
contrast further, and that is a C6 decision about per-corner variation, not a storage one.

**Nothing here was judged on screen.** All of it is counted texels.

---

## 2026-09-23 — C9: what the folded read path costs, measured

The read path went from **9 texture fetches per road pixel to 18**, because every tap of the 3x3
tent now resolves two neighbouring texels and blends them by hand rather than letting the sampler
do it across a row boundary. That was the largest unmeasured claim in the phase.

### The field has to be released from the command line, and a console variable cannot do it

The first capture attempt measured a **frozen grid**. C7 made the field start paused and bound its
release to the `P` key, so a `-game` run with nobody at the keyboard sits on the start line: the road
still renders, so the read path is exercised, but no car moves and nothing paints.

**A console variable was tried first and does not work.** `-ExecCmds` executes at frame 1, after
`BeginPlay` has already read the variable and paused the field, so the log still said
`starting frozen`. The capture that produced the table below ran only because a human pressed `P`
ten seconds in — which is precisely the kind of procedure this file exists to eliminate, and it went
unnoticed until the field was observed never starting on a later run.

**Two command-line switches replace it**, read through `FParse::Param` and therefore independent of
any execution order:

| Switch | Effect |
|---|---|
| `-ReleaseField` | The field starts running, whatever `bStartPaused` says |
| `-FreezeField` | The field starts frozen, whatever `bStartPaused` says |

Neither is needed for ordinary play, where `bStartPaused` and the `P` key still decide. They exist
so a capture is **reproducible from one command line**, which is the standard this file holds itself
to, and the log line now reads `starting running` when the switch has taken.

```
Command:  UnrealEditor.exe RubberAccumDemo.uproject /Game/Maps/L_SpaHarness -game
          -windowed -resx=1280 -resy=720 -nosplash -nosound -ReleaseField
          -csvCaptureFrames=12000
```

12000 frames captured, the first 2500 discarded as load and warm-up.

### Frame time, twenty cars, 1800 m folded into four rows

11000 frames after warm-up:

| Counter | Median | Mean | p90 | p99 | Max |
|---|---|---|---|---|---|
| **FrameTime** | **4.725 ms** | 4.935 | 5.955 | 7.342 | 35.907 |
| RenderThreadTime | 4.675 | 4.922 | 5.958 | 7.293 | 39.367 |
| GameThreadTime | 3.533 | 3.761 | 4.708 | 5.733 | 16.148 |
| **GPUTime** | **2.365** | 2.418 | 2.750 | 2.912 | 3.093 |

The earlier keypress-released capture of the same build gave FrameTime 4.697 and GPUTime 2.393, so
the two agree to within 1 % and the figure is not a fluke of one run.

Against the R6 baseline, which is the last comparable capture this project holds:

| Counter | R6, 1500 m, 9.7656 cm/texel, 9 fetches | **C9, 1800 m, 3.125, folded, 18 fetches** | Change |
|---|---|---|---|
| FrameTime | 4.484 | **4.725** | +5.4 % |
| RenderThreadTime | 4.470 | **4.675** | +4.6 % |
| GameThreadTime | 2.369 | **3.533** | +49.1 % |
| GPUTime | 2.351 | **2.365** | **+0.6 %** |

**Doubling the texture fetches per road pixel cost 0.6 % of GPU time, inside the run to run
spread.** The reason is in the same table: RenderThreadTime tracks FrameTime almost exactly, 4.675
against 4.725, while the GPU sits at about half of it. **The scene is render-thread bound and was
render-thread bound before**, so the road's pixel cost is not what limits it, and there was room to
spend. The manual bilinear was affordable because this scene had headroom, not because it is cheap.

### The paint pass got seven times slower at the median and no slower in the tail

Measured separately, because `r.RacetrackSurface.TimePaint` synchronises the game and render
threads on every paint and destroys the frame rate it would be compared against. 5821 samples after
warm-up, 80 strokes per paint, which is twenty cars laying four wheels each.

```
Command:  UnrealEditor.exe RubberAccumDemo.uproject /Game/Maps/L_SpaHarness -game
          -windowed -resx=1280 -resy=720 -nosplash -nosound -ReleaseField
          -ExecCmds="r.RacetrackSurface.TimePaint 1"
```

| | R6, 20 cars, unfolded | **C9, folded into four rows** |
|---|---|---|
| Min | 0.0200 | 0.0250 |
| **Median** | **0.0300 ms** | **0.2100 ms** |
| Mean | 0.0969 | 0.2251 |
| p90 | 0.3190 | 0.5190 |
| p99 | 0.5480 | 0.6610 |
| **p99.9** | **0.9990** | **0.9780** |
| Max | 7.3920 | 11.1200 |

**The median is seven times worse and the p99.9 is unchanged**, which is the opposite shape to what
a per-stroke cost would produce. R6's own advice was that *"the number to carry for frame budgeting
is p99.9 at about 1.0 ms"*, and by that measure nothing has changed: 0.978 ms against 0.999 ms,
still about a fifth of a frame. Against the 4.725 ms frame above, the median paint is **4.4 %**.

**Two candidate causes and this capture cannot separate them.** The scissor forces a canvas flush
whenever consecutive strokes belong to different rows, where `FScopedCanvasFlushDeferral`
deliberately batches the whole frame into one; and the render target is now 14400 x 1024 against
R6's 15361 x 123, which is **7.5 times the bytes** and close enough to the 7x median change to be
suspicious. Isolating them needs a capture at four rows with the scissor disabled, which does not
exist. **The 7x should not be attributed to the scissor without that run.**

**The +50.6 % on the game thread is not C9's**, and this capture cannot prove whose it is. C8 added
four wheel contacts per car in place of one and moved them through the world-space API, which it
measured at **0.3658 ms for 80 contacts**, and C8 never took a frame capture. So the game-thread
rise spans both phases and cannot be attributed from the two captures that exist. What can be said
is that four contacts per car against one, plus a world-to-track conversion per contact that R2's
segment cache would make 42 times cheaper, is more than enough to account for it, and that the
cache is still not implemented.

### What this does not measure

**No before-and-after exists for the fetch count alone.** Isolating it would need the pre-C9
material, and the material is an asset whose previous version is only recoverable from git. The
1.8 % figure is therefore the combined effect of the fold, the manual bilinear, the finer texel and
the longer lap, not the fetch count on its own.

**The capture is render-thread bound, so it is the wrong instrument for the question it was asked.**
A scene that was GPU bound would show the doubled fetch cost plainly. This one cannot, and a reader
should not take 1.8 % as evidence that manual bilinear is cheap in general — only that it is free
in this scene.

---

## 2026-09-24 — C10: the decay pass at the production atlas, and the half-life calibration

```
Build:    Development, editor target, headless, editor closed
Power:    MAINS. Win32_Battery BatteryStatus = 2
Tests:    74 of 74 pass. Both build paths clean (editor target and RunUAT BuildPlugin)
Command:  UnrealEditor-Cmd.exe RubberAccumDemo.uproject
          -ExecCmds="Automation RunTests RacetrackSurface+RubberAccumDemo"
          -TestExit="Automation Test Queue Empty" -unattended -nopause -nosplash -nosound
```

### The decay pass at 14400 x 1024, which PLAN.md had extrapolated 2.5 times too high

`RacetrackSurface.Performance.DecayAtTheProductionAtlas`, 64 samples, a default-constructed
`FSurfaceTrackRibbon`, which is the demo's own configuration: 180000 cm of lap at 800 cm wide,
3.125 cm per texel, 57600 x 256 texels in 4 rows of 14400, allocating **14400 x 1024 for
117964800 bytes**.

| | Extrapolated in PLAN.md | **Measured** |
|---|---|---|
| Full-buffer decay, median | 0.845 ms | **0.3340 ms** |
| Full-buffer decay, minimum | — | **0.2870 ms** |
| Share of the 2.365 ms GPU frame | 36 % | **14 %** |

**The extrapolation was linear in bytes touched and the pass is not.** B4's stated reading — *"decay
cost tracks bytes touched"* — was taken across a 64-fold range of *square* buffers and does not hold
from 33554432 bytes to 117964800. The real ratio is **3.52 times the bytes for 1.39 times the
time**, so the pass is substantially sub-linear at this size. Recorded as a correction to B4's
reading rather than a new one, because two points do not establish a curve: what is established is
that the linear model overstates the cost at this size by 2.5x.

**This weakens the cost half of the argument for not decaying every frame, and that should be said
plainly.** 0.34 ms against a 4.725 ms frame is 7 %, which is expensive but not disqualifying. The
argument that survives intact is the precision one below, which is not a matter of degree.

### Why the pass runs on banked decay rather than every frame, in numbers

`RacetrackSurface.Mapping.DecayStepSurvivesHalfFloatRounding`.

The buffer is RGBA16f. A half float carries an 11-bit significand, so half an ulp is between `2^-12`
and `2^-11` of the value: a multiply that moves a value by less than **4.883e-4** relative is not
guaranteed to change any bit, and one below 2.441e-4 is guaranteed to change none.

| | Relative change per application | Against the 4.883e-4 threshold |
|---|---|---|
| Decaying every frame at 60 Hz, 1x | 1.281e-5 | **38 times too small** |
| One step at 256 steps per half-life | 2.704e-3 | **5.5 times the threshold** |

So a per-frame decay pass at this half-life **costs 0.34 ms and changes nothing at all**, silently.
Banking progress until one step has elapsed makes the retention a constant 0.997296 whatever the
rate and whatever the frame rate, which is both representable and predictable in cost.

`RacetrackSurface.Mapping.DecayRetentionIsIndependentOfFrameRate` measures the frame-rate
independence directly: over 10 s at 1x, one banked step and 600 compounded frames give retentions
differing by **1.115e-5, which is 44 times finer than the buffer can hold**.

Cost that follows, at the measured 0.3340 ms and the 900 s fallback half-life:
`Rate * 256 / HalfLifeSeconds` is 0.2844 passes a second at 1x, so 0.0950 ms a second and
**0.0016 ms a frame at 60 Hz**, rising linearly with the rate. At 20x it is 1.9001 ms a second.
All four figures are printed by the test rather than worked out here.

### Lap time, measured against the speed profile the half-life is derived from

`RubberAccumDemo.Track.LapSecondsMatchATickedLap`, one car ticked at 1/60 s until it completes a lap.

| Layout | Lap | Predicted by integrating the speed profile | Ticked | Error |
|---|---|---|---|---|
| Stadium | 30921 cm | 14.68 s | **15.83 s** | 7.3 % |
| Spa | 180000 cm | 80.44 s | **87.05 s** | 7.6 % |

The integration assumes a car is at its target speed everywhere and ignores the bounded braking and
acceleration rates, so it runs consistently fast by about 7.5 % on both layouts. That is small
enough for a calibration and the bias is stated rather than corrected.

**This is what forced the half-life to be held in laps rather than in seconds.** At 56 laps the
half-life is 822 s on the Stadium and 4505 s on Spa — **5.5 times apart** — so any single constant
in seconds is wrong on one of the two layouts by that factor.

### The half-life, derived from the measured deposit rather than chosen

`RubberAccumDemo.Track.CountedPassesOverATexelAgainstThePrediction`, which already counted the
busiest apex and straight texel, now also reports the equilibrium the configured half-life implies.

A texel under a steady deposit approaches `E = D / lambda`. Writing the half-life in laps makes lap
time cancel out of that entirely:

```
E = ApexExposurePerLap * HalfLifeInLaps / ln2
```

Asking the apex to settle at an exposure of 1.0, which is the 0.95 coverage `k = 3.0` was itself
derived against, gives `HalfLifeInLaps = ln2 / 0.012471 = 55.6`, taken as **56 laps**.

| | Exposure a lap | Equilibrium exposure | Equilibrium coverage |
|---|---|---|---|
| Busiest apex texel | 0.012471 | **1.0076** | **0.9513** |
| Busiest straight texel | 0.001516 | 0.1224 | **0.3074** |
| **Contrast** | 8.2 to 1 | 8.2 to 1 | **3.09 to 1** |

**This is the first settled racing line this project has been able to state.** Every earlier figure
described a transient that only ever got darker, which is why MEASUREMENTS.md had to record the line
as *"most legible somewhere between"* two lap counts. With decay there is an equilibrium, and the
apex-to-straight contrast becomes a property of the traffic rather than of how long the capture ran.

**Provenance, which was not previously recorded and is a defect in this file.** The 0.012471,
0.001516 and 8.2:1 figures in the C6 table above were measured on the **Stadium**, not on Spa.
`CountedPassesOverATexelAgainstThePrediction` spawns a default `ARacetrackCircuit`, whose `Layout`
defaults to `ETrackLayout::Stadium`. **17 of the 23 host tests that spawn a circuit never call
`SetLayout`**, so they run the oval; only 6 do, and the host module defines 26 tests in total. The
"twenty cars over 12.67 laps" in that table is a 30921 cm lap, not the 180000 cm demo. The
equilibrium arithmetic above is layout-independent by construction, so it is unaffected, but **the
contrast figure itself has not been re-measured on the demo layout** and should not be quoted as a
Spa number.

### Not verified

- **Nothing here has been seen on screen.** The editor was closed for the whole session and Unreal
  MCP was unreachable, so no capture exists of the on-screen messages, of any single-channel view,
  or of the surface fading. The `DebugChannelMask` material parameter does not exist yet either, so
  the channel cycle currently logs and announces correctly and changes nothing visually.
- **The 0.3340 ms decay figure is the pass in isolation**, timed the same way every other pass in
  this file is, not the pass inside a live twenty-car frame.
- **The equilibrium is arithmetic from a measured deposit rate, not an observed steady state.** No
  run has been left going long enough to reach it.
- **The half-life is calibrated against this demo's field.** A different car count or lateral spread
  moves the apex deposit and therefore the equilibrium; the test asserts the coverage lands between
  0.90 and 0.99 so that this fails loudly rather than drifting.

---

## 2026-09-24 — C10.5: the capture, and two defects it found that tests did not

```
Build:    Development, editor target run as UnrealEditor.exe -game, windowed 1600x900
Power:    MAINS
Scene:    /Game/Maps/L_SpaHarness, twenty cars, Spa layout at 180000 cm
Driven:   entirely from -ExecCmds, no keyboard. One process per still, killed after the grab
Capture:  desktop grab of the client rect, no editor UI in frame
```

Six stills in [`assets/media/`](../../2627-Y3A-PR-243663/assets/media/): `c10-view-all-channels`,
`c10-view-rubber-buildup`, `c10-view-skid-marks`, `c10-view-marbles-empty`, `c10-decay-off`,
`c10-decay-on-20x`.

**Every still was taken at `r.RacetrackSurface.DepositionScale 32` and `TimeDilation 20`, and that
has to be read off the image, not into it.** At 32x the derived rate the apex reaches 63 % coverage
in about a quarter of a lap instead of about eight, which is the only way a 70-second run has a
surface worth photographing. The stills therefore show *what the channels look like*, not *how fast
rubber arrives*. The rate figures of record are the ones in the C6 table above, taken at scale 1.

| Still | What it shows |
|---|---|
| `c10-view-all-channels` | Red rubber along the line with yellow where skid overlaps it, which is what `Coverage` being a Vector3 has always meant |
| `c10-view-rubber-buildup` | The same line in red alone |
| `c10-view-skid-marks` | Discrete green lockup marks clustered in the braking zones, with the rubber gone. The first image in this project that shows the two channels are genuinely separate |
| `c10-view-marbles-empty` | Black, with the caption naming the channel and saying nothing writes it before phase G |
| `c10-decay-off` against `c10-decay-on-20x` | The same run with the rate at 0 and at 20. Decay narrows the band: the off-line wash fades and the apex holds |

### Defect found by capturing: `-ExecCmds` splits on commas, not semicolons

The first capture set applied only its first command. `ParseExecCommands::ParseExecCmds`
(`ParseExecCommands.cpp:29`) breaks the string on `,` and treats `;` as ordinary text, so
`-ExecCmds="A; B; C"` is one command named `A` with arguments `; B; C`. Every earlier use in this
project passed a single command, so it had never surfaced. **Every `-ExecCmds` list in this file and
in `EDITOR_SETUP.md` must use commas.**

### Defect found by capturing: on-screen messages expire on the dilated clock

The second capture set applied its commands and still showed no text. `DrawOnscreenDebugMessages`
advances a message's age by `World->GetDeltaSeconds()` (`UnrealEngine.cpp:13543` and `:13569`), and
`UWorld::Tick` writes the **dilated** delta into that (`LevelTick.cpp:1596`, `:1603`).

So a message's lifetime is spent in track time, not in the operator's time:

| Dilation | A six-second message lasts |
|---|---|
| 1x | 6.0 s |
| 20x | **0.3 s** |

**That is C10.1 failing at exactly the setting the demo is built around.** The phase exists so a
viewer can tell what changed, and at the compression the demo runs at, every announcement flashed
and vanished before it could be read. No test caught it because no test renders a viewport, and the
first four captures did not catch it either — the first set failed for the comma reason and looked
like the same symptom.

Fixed by multiplying the requested duration by the current time dilation at post time, in both
modules, so the figure means seconds the operator waits. The host reads dilation from its own world;
the plugin's console-variable callbacks have no world context, so they take the effective dilation
from the first Game or PIE world context. Verified on screen: at 20x the captions are present after
70 real seconds, which is 1400 seconds of track time.

**Not verified.** The dilation is read once, when the message is posted, so a message posted at 1x
and then followed by a jump to 20x still expires twenty times too fast. That is the case where the
operator is changing dilation, which re-posts its own message anyway, so it is recorded rather than
fixed.

---

## 2026-09-24 — G: where the marble band lands, measured at the deposit

```
Build:    Development, editor target, UnrealEditor-Cmd.exe headless with the editor closed
Power:    MAINS
Scene:    default ARacetrackCircuit, Stadium layout, 30850 cm lap, 800 cm wide, one car
Driven:   RubberAccumDemo.Car.MarblesGatherOutsideTheLineAndThinTowardTheEdge, 4000 ticks at 1/60 s
Deposit:  r.RacetrackSurface.DepositionScale at its default of 1.0
```

The phase-G requirement was *where the marble band actually lands across the track width, measured
rather than looked at*. It is measured **at the deposit**, in centimetres and in lateral offset
alpha, not at the texel. Why that is the right place, and what it does not cover, is under *Not
verified* below.

### The throw distribution against the prediction

`PLAN.md` predicted, before any of this ran, that drawing the throw as
`MarbleThrowCentimetres * min(u1, u2)` gives a density of `2(1 - x)` — a linear fall from the
racing line outward — with a mean at a third of the maximum.

| Quantity | Predicted | Measured |
|---|---|---|
| Mean throw | 66.67 cm | **66.40 cm** |
| Widest throw | ≤ 200 cm | **198.40 cm** |
| Deposits landing on the apex side | 0 | **0** |

Binned into fifths of the throw, nearest the racing line first, over 2966 deposits:

| Fifth | 0-40 cm | 40-80 cm | 80-120 cm | 120-160 cm | 160-200 cm |
|---|---|---|---|---|---|
| Predicted | 1068 | 830 | 593 | 356 | 119 |
| **Measured** | **1029** | **916** | **570** | **342** | **109** |

Monotonically falling, which is the band reading as *a gradient away from the line* rather than as
a stripe at the kerb. The second bin is about 10 % over prediction and the first about 4 % under;
the samples are not independent, because consecutive ticks share a car whose own line and speed are
correlated, so the bin counts are not binomial and no significance should be read into that.

### Where that puts the band across the track

Signed so that positive is away from the apex, over the same 2966 deposits:

| | Mean lateral offset alpha |
|---|---|
| The car's own line | -0.3263 |
| The marble deposit | **-0.1603** |

A separation of **0.166 alpha, which is 66.4 cm on this 800 cm track, or 21.2 texels** at 3.125 cm
per texel. The band sits outside the line the car drives, by construction and now by measurement.

### Deposit rate

2966 deposits over 4000 ticks is **74.2 % of ticks**, against the Stadium's **61.10 %** *distance*
share of cornering. Those disagree and should: the car corners at `CorneringSpeedFraction` 0.55, so
it spends 11.4 s of a 15.4 s lap in the corners and only 4.0 s on the straights, which is 74.0 %.
The deposit rate follows time, not distance, and the two figures agree to 0.2 %.

### The channel is genuinely isolated

`RacetrackSurface.Accumulator.MarbleStrokeWritesOnlyTheMarbleChannel`, one marbles stroke and one
rubber stroke painted into the same 512-square RGBA16f buffer and read back with
`ReadRenderTargetRawPixelArea(..., bNormalize = false)`:

| Stroke | R | G | B |
|---|---|---|---|
| Marbles | 0.0000 | 0.0000 | **0.9692** |
| Rubber buildup | **0.9692** | 0.0000 | 0.0000 |

This is the first non-zero value the B channel has ever carried in this project.

### The gate changed during implementation, and the reason is a measurement

`PLAN.md` predicted that multiplying the deposit's slip by the cornering amount would be enough to
stop a straight depositing anything, because `FMath::Sign(0)` returns `0`
(`GenericPlatformMath.h:937-940`) and a zero throw direction would put marbles on the racing line.

**That was wrong, and the first run said so:** 34 of the first sample's deposits landed on the apex
side, and a straight registered 5 contacts rather than 4. The cause is that the cornering profile is
*sampled from the spline and normalised* (the C3 amendment), so on a straight it is small but not
exactly zero — and the *sign* of a value that is numerically noise is arbitrary. The multiply made
those deposits harmless in exposure terms while leaving them on a random side of the car.

Fixed by gating on `CorneringHere >= CornerExitCorneringAmount`, which is **the threshold the car
already owns** for deciding it is no longer in a corner. No new tunable, and behaviour does not
depend on the deposition scale — which a gate derived from the half-float floor would have done.

### Not verified

- **The band was measured at the deposit, not at the texel.** No test reads the marble band back out
  of the accumulation buffer across the track width. The composition that would need checking —
  lateral offset to ribbon to folded atlas — is already covered by `SurfaceFoldedRibbonTests`, and
  the channel isolation is covered above, but *this band* has not been read back from *this buffer*.
  The planned `MarbleBandProfileAcrossTheTrack` test was not built: it needs the full field ticked
  through the subsystem for several laps against a live render target, and it would assert the shape
  that the deposit measurement above already establishes.
- **Nothing has been seen on screen.** The editor was closed for the whole session and Unreal MCP
  was unreachable. Marbles should render as flat blue through the existing debug material with no
  asset change, because `Coverage` is a Vector3 and B is part of it, but that is an inference from
  the graph recorded in `EDITOR_SETUP.md`, not an observation.
- **The Stadium, not the demo layout.** This entry spawns a default circuit, which is the oval, for
  the same reason 17 of the 23 host tests do. The throw distribution is layout-independent by
  construction; the 74.2 % deposit rate is not, because Spa's cornering is graded rather than
  binary, and it has not been measured there.
- **No equilibrium.** Decay defaults to 0 and was off throughout. `PLAN.md` predicts the band peaks
  at 0.623 coverage at the shared half-life; no run has been left going long enough to reach it, and
  that figure is arithmetic rather than an observation.
- **One car, not twenty.** The distribution is per-car and does not change with field size, but the
  band's *width once twenty cars on twenty different lines contribute* has not been measured.

---

## 2026-09-24 — G, second pass: the cornering profile, and three defects it was hiding

```
Build:    Development, editor target, UnrealEditor-Cmd.exe headless with the editor closed
Power:    MAINS
Scene:    ARacetrackCircuit, Spa layout, 180000 cm lap, 800 cm wide, 3.125 cm/texel, 4 rows, 20 cars
Driven:   RubberAccumDemo.Demo.* automation tests, and one standalone -game run of L_SpaHarness
```

Everything in the previous G entry was measured through a test harness. This entry exists because
**the harness and the screen disagreed, and the harness was wrong.**

### The measurement that found it, taken in the live game rather than in a test

`RacetrackDemo.SurfaceReport`, a new console command that reads the accumulation buffer back over
48 columns and reports peak coverage per channel, run twice in a Play-In-Editor session:

```
Peak coverage: rubber 0.0822, skid 0.0000, marbles 0.0000 ... Debug view is all three channels
               with mask (R=1.000000,G=1.000000,B=1.000000,A=1.000000)
Peak coverage: rubber 0.0899, skid 0.0000, marbles 0.0000
```

**Exactly zero, not small.** Four hypotheses had been offered before this and all four were wrong;
every one was reasoned from the test harness. The first live read settled it in one command. The
cause and its four symptoms are in [PLAN.md](PLAN.md): `CorneringProfile` is not a `UPROPERTY` and
`OnConstruction` does not re-run for a map-loaded actor in PIE, so the profile was empty and
`GetSignedCorneringAtDistance` returned zero everywhere.

**Method note worth keeping.** Standalone `-game` runs `OnConstruction`, so every timing and
contrast figure in this file before today was taken on the working path and none of them is
invalidated. The broken path was Play-In-Editor only, which nothing automated exercises.

After the fix, from a real `-game` run of `/Game/Maps/L_SpaHarness`:

```
RacetrackCircuit_0 sampled 1200 cornering points over 180000 cm, tightest curvature 0.00197596
per cm, normalised against the 75th percentile at 0.00023151
```

Tightest radius **5.06 m**, normalisation radius **43 m**, so anything tighter than 43 m clamps.

### Slip against the cornering at the car's own position, one lap of the demo layout

`RubberAccumDemo.Demo.SlipSeparatesStraightsFromCornersOnTheDemoLayout`, one car, 5223 ticks at
1/60 s. Deposition read off the contacts the car actually registered, not recomputed.

Cornering at the car by percentile of lap time: p10 **0.057**, p25 0.133, p50 **0.484**, p75
**1.000**, p90 1.000, p99 1.000. A quarter of the lap is clamped at maximum by construction.

| Cornering bucket | Ticks | Mean slip before | Mean slip after |
|---|---|---|---|
| 0.0-0.1 | 1121 | **0.1805** | **0.0750** |
| 0.1-0.2 | 531 | 0.2539 | 0.1164 |
| 0.2-0.3 | 452 | 0.3124 | 0.1634 |
| 0.3-0.4 | 335 | 0.3283 | 0.2049 |
| 0.4-0.5 | 197 | 0.3575 | 0.2507 |
| 0.5-0.6 | 159 | 0.3747 | 0.2964 |
| 0.6-0.7 | 183 | 0.4134 | 0.3441 |
| 0.7-0.8 | 217 | 0.4330 | 0.3882 |
| 0.8-0.9 | 237 | 0.4643 | 0.4327 |
| 0.9-1.0 | 1791 | 0.4981 | 0.4969 |
| **Realised contrast** | | **2.76 to 1** | **6.63 to 1** |

The model's own range is 0.05 to 0.50, which is 10 to 1. Before the fix the straightest tenth of
the lap started a third of the way up that range, because deposition read a cornering value that
included 25 m of braking lookahead.

### Stroke geometry under time compression

`RubberAccumDemo.Demo.StrokesDoNotSmearAcrossTheTrackUnderCompressedTime`, 300 sampled ticks each.
A 20 cm contact patch is 6.40 texels; the track is 256 texels wide.

| | Longest stroke | Widest lateral span | Against mark width |
|---|---|---|---|
| Real time, 0.0167 s | 50.0 cm | 5.22 texels | 0.8x |
| Compressed 20x, **before** | 999.6 cm | **97.45 texels** | **15.2x** |
| Compressed 20x, **after** | 999.6 cm | **9.74 texels** | **1.5x** |

Stroke *length* is unchanged and is not a defect: the ribbon is unrolled, so a car holding its line
through a corner is a straight line in track space. Only the lateral term needed bounding.

Furthest a painted stroke endpoint reaches across the track, against a field whose widest wheel
sits at alpha **0.7749**: real time **0.7819** (1.4 cm of overshoot), compressed **0.8589**
(33.6 cm). **Zero endpoints past the track edge at either rate.**

### Every channel at steady state under the demo's own decay

`RubberAccumDemo.Demo.EveryChannelIsVisibleAtSteadyStateUnderDecay`. Deposition 16 with decay 16
reaches the same equilibrium as the demo's 2 and 2 eight times sooner, because equilibrium is
proportional to scale over rate. Half-life 4505 s of track time, so a 282 s time constant; ticked
2500 times at 0.3333 s, which is 833 s, about **3.0 time constants**.

| Channel | Peak exposure | Peak coverage |
|---|---|---|
| Buildup | 0.876953 | **0.9280** |
| Skid | 0.146729 | **0.3561** |
| Marbles, before sweeping | 0.303223 | 0.5973 |
| Marbles, after sweeping | 0.169434 | **0.3985** |

Sweeping costs the marble channel its peak because the deposits it removes are the ones that landed
on the racing line. That is the mechanism working, not a loss.

### Where the marble band actually lands, which is the measurement phase G owed

Across-track profile at steady state, summed over the 42 of 96 probe columns that are in a corner,
**folded by the sign of the local curvature so left and right handers superimpose**. Band 0 is the
outside of the corner.

| From the outside edge | Rubber | Marbles |
|---|---|---|
| 0-50 cm | 0.00 | 0.09 |
| 50-100 | 0.00 | 1.42 |
| 100-150 | 0.00 | 6.34 |
| 150-200 | 0.00 | 12.62 |
| 200-250 | 0.52 | 20.02 |
| 250-300 | 1.90 | 27.74 |
| **300-350** | 6.78 | **35.04** |
| 350-400 | 44.46 | 33.97 |
| 400-450 | 221.11 | 5.95 |
| 450-500 | 279.25 | 4.16 |
| **500-550** | **306.86** | 2.18 |
| 550-600 | 172.96 | 0.80 |
| 600-650 | 73.76 | 0.24 |
| 650-700 | 17.26 | 0.02 |
| 700-750 | 1.01 | 0.01 |
| 750-800 | 0.00 | 0.00 |

**The two peaks are 200 cm apart and nothing in the code reserves that gap.** Marbles peak
immediately outside where rubber begins, collapse by a factor of six under the racing line, and
thin monotonically to the outside edge. The clean line is produced by traffic sweeping what it
drives over.

**The first version of this measurement was wrong and is worth recording as a method note.** It
summed across the whole lap without folding, so left and right handers mirrored each other and both
channels appeared to peak near the centreline, 50 cm apart. The shape only became visible once the
fold was applied. An unfolded average over a circuit that turns both ways measures nothing.

### Not verified

- **Nothing in this entry was looked at on screen by the author of it.** The band profile, the
  contrast and the smear are all readbacks and contact counts. What the surface looks like is the
  user's call and was judged separately.
- **The sweep pass is not timed.** It is a second full canvas pass of roughly 80 modulate quads per
  frame, and it has no number in this file. `r.RacetrackSurface.TimePaint` brackets `PaintStrokes`
  only, so timing the sweep needs the same treatment extending to it.
- **One car, one lap, for the slip table.** The distribution is per-car and does not change with
  field size, but no twenty-car figure exists.
- **The 33.6 cm of invented lateral reach at 20x is bounded, not removed.** The exact fix is
  recorded against C4 and not taken.
- **Everything here is the Spa demo layout.** The Stadium figures elsewhere in this file are still
  Stadium figures and the two should not be compared.

---

## 2026-09-24 — F5 to F8: the query path measured, and a readback bug the measurement found

```
Build:    Development, editor target, UnrealEditor-Cmd.exe headless with the editor closed
Power:    MAINS, confirmed from Win32_Battery BatteryStatus 2 and BatteryStatus.PowerOnline True
          rather than assumed
Scene:    62500 cm lap, 800 cm wide, 3.1250 cm per texel, 2 rows, atlas 10000 x 512, on a 64 point
          closed circle. One submission per engine frame on an otherwise idle run
Driven:   RacetrackSurface.Performance.QueryCostAndLatency, .QueryReadbackRingDepth,
          RacetrackSurface.Query.LatencyCostsLessThanTheFieldVariesAlongTrack,
          RubberAccumDemo.Surface.GatherAgreesWithTheShaderAtTheSameRadius
Suite:    113 of 113 passing, no GPU crash, both build paths clean
```

**113 of 113, up from 103.** Ten new tests, one of which found a bug that had nothing to do with
what it was written for.

### The bug the measurement found, which is the most valuable thing in this entry

`RacetrackSurface.Query.LatencyCostsLessThanTheFieldVariesAlongTrack` acquires 41 query handles
after one has already resolved. That widens the gather target from 1 slot to 41, and the RHI thread
died with `E_INVALIDARG` at `D3D12CommandList.cpp:244` two frames later.

**`ResolveSurfaceQueries` logged a claim the code did not carry out.** The message read *"the
readback staging textures are rebuilt"*, but the call behind it was
`FSurfaceQueryReadback::Invalidate()`, which bumps the epoch and clears the resolved cache and
**never touches the `FRHIGPUTextureReadback` objects**. Those are created once per ring slot and
reused, and `RHIGPUReadback.cpp:139` documents the assumption they carry: *"Assume for now that
every enqueue happens on a texture of the same format and size (when reused)."* A staging texture
sized for a 1 texel source was handed a 41 texel one.

| | |
|---|---|
| Repro cost | **one test, 20 seconds**, against a twelve minute suite |
| Fix | `RebuildForANewSourceSize()`, which resets the ring on the render thread so the next enqueue builds staging textures at the new size |
| Guard | `RacetrackSurface.Query.GrowingTheQueryCountAfterResolvingStaysSafe` — one handle resolves, the count grows to 24, every one of them then answers with the painted mark. Recovered in **3 engine frames** |

**This is user facing, not a test artefact.** Any caller whose live query count rises at runtime —
a second car starting to query, a wheel added — grows the gather target under a ring already in
flight. Nothing before this test did, because everything before it held a fixed handle count.

It is the same shape as the reallocation bug F4 found: a resource resized underneath work the GPU
had not finished with. The method that found it is also the same, and it is the transferable part:
**shrink the repro before forming hypotheses.** One test reproduced it in 20 seconds, and the log
line that was lying named the cause outright.

### Latency, and the number the tolerance claim rests on

| Quantity | Measured |
|---|---|
| Latency | **2 query frames, 3 engine frames, 0.0500 s** of dilated world time |
| Along track travel over that, at 5500 cm/s | **183.3 cm, 58.7 texels** |
| Lateral travel over that, at 1.20 alpha/s | **0.0400 alpha, 16.0 cm, 5.12 texels** |

Measured on a swept skid from lap fraction 0.30 to 0.60, **300 cm wide**, at 100x the derived
deposition so coverage sits at **0.440** — in the responsive part of the saturation curve rather
than flattened against it. The band's lateral edges sit at alpha 0.375 and the lateral pairs sweep
from -0.5 to 0.5, so some of them straddle one.

The error is the coverage difference between the position submitted and the position the wheel
occupies when the answer lands. It is taken from **two resolved values of one run** — the field is
sampled at a point and at that point displaced by the measured latency — so no second reader has to
agree with the first for the number to mean anything.

| Direction | Median error | Worst | Coverage there | Worst as a fraction |
|---|---|---|---|---|
| Along track, inside the band | 0.000204 | 0.000207 | 0.440 | **0.05 %** |
| Laterally | 0.000000 | **0.232302** | 0.309 | **75 %** |
| Across a painted step edge | 0.000204 | 0.087805 | 0.421 | 21 % |

**This half confirms the proposal's tolerance argument and half contradicts it, and the
contradiction is the useful part.**

PLAN.md corrected the proposal's reason — the delay is tolerable not because the field is static
but because *"the field is smooth in the direction the wheel is moving fastest"* — and added that
*"lateral position is what barely moves in three frames"*. The first clause holds and is now
measured: along track the wheel moves 58.7 texels and the coverage it reads changes by 0.000204,
which is the fp16 noise floor of a value near 0.44 and not a gradient at all. A swept deposit is
uniform along its own direction of travel, by construction, because that is how it was laid.

**The second clause is where the error actually lives.** The lateral move is small — 5.12 texels —
but it is small against a field whose sharpest gradient runs *across* the band. A pair straddling
the band's lateral edge disagrees by **0.232**, three orders of magnitude more than the along track
case and 75 % of the coverage at that point. The median is zero because most of the band's width is
flat, so this is a tail and not a typical frame. But it is the tail a grip model would feel: it is
exactly the moment a car puts a wheel off the racing line.

So the honest statement of the tolerance is **not** "the error is small". It is: *the error is at
the noise floor wherever the field is smooth, and is bounded by the field's own gradient times the
latency displacement wherever it is not, which on this field peaks at 0.23 coverage at the edge of
an accumulated band.* Whether that matters is a question for whatever grip model consumes it, which
is outside this plugin's boundary by design.

### Query cost, at the demo's four points and at eighty

| Points | Gather GPU median | GPU minimum | per point | BuildTaps CPU | WorldToTrack CPU | per point |
|---|---|---|---|---|---|---|
| **4** | **0.1030 ms** | 0.0870 ms | 0.02575 ms | 0.0005 ms | **0.0155 ms** | 0.00388 ms |
| 80 | 0.1560 ms | 0.1460 ms | 0.00195 ms | 0.0082 ms | 0.3035 ms | 0.00379 ms |

At the demo's four points the whole query costs **0.1030 ms on the GPU and 0.0160 ms on the CPU**,
against the **0.3340 ms** decay pass already in the frame and 16.67 ms of a 60 Hz frame. It is under
a third of a pass this project already pays for every frame, and about 0.7 % of the frame.

**The gather is fixed cost dominated, and that is worth knowing before anyone optimises it.**
Twenty times the points costs 1.5 times the GPU time — 0.1030 ms against 0.1560 ms — so per point
the cost falls from 0.02575 ms to 0.00195 ms, a factor of thirteen. The clear and the canvas flush
dominate 18 tile draws per point. The consequence: **there is no per point saving available at four
points**, and the compute pass rejected in PLAN.md would be optimising the part that is already
free. If the gather ever needs to be cheaper, the target is the fixed cost, not the taps.

**R2's segment cache trigger is not approached and must not be described as the design target.**
R2 wrote it as *"any world-space caller reaching 80 contacts per frame in a real scene"*. The demo
reads one car, which is four, costing **0.0155 ms** of world to track conversion — 0.09 % of a
frame. The trigger stays unimplemented for a reason rather than by omission.

**One discrepancy recorded rather than smoothed over.** World to track at 80 points reads 0.3035 ms
here against the **0.1383 ms** R2 measured for the same count. Different circuit: this entry uses a
64 point closed circle of radius 9947 cm sized to the 62500 cm ribbon, R2 swept 18 to 1024 points at
radius 15000 cm. Both are below R2's 1 ms crossover and neither supersedes the other; they are not
the same measurement and should not be quoted as one.

### Ring depth 4, measured rather than inherited

One submission per engine frame, 30 frames at each depth, standalone ring, idle headless run.

| Depth | Submissions | Accepted | Dropped | Resolves seen | Median resolve gap |
|---|---|---|---|---|---|
| 1 | 30 | 30 | 0 | 28 | 2 |
| 2 | 30 | 30 | 0 | 28 | 2 |
| 3 | 30 | 30 | 0 | 28 | 2 |
| **4** | 30 | 30 | 0 | 28 | **2** |
| 8 | 30 | 30 | 0 | 28 | 2 |

**Depth buys nothing on this workload.** A ring of one drops nothing and answers exactly as fast as
a ring of eight. Depth 4 is what Nanite streaming, Lumen surface cache feedback and distance field
streaming each settled on independently with no shared engine constant tying them together, and
PLAN.md recorded that as convention rather than derivation. The derivation now exists and says
**one would do**.

The default stays at 4 and the reason is stated rather than assumed: the sweep measures a GPU
keeping up, and depth is insurance for one that does not. The hitch case below is what depth is
for, and it is cheap — four staging textures of one row each.

**What the "2 frames" is, precisely.** It is measured on the game thread from `CopyResolved`, so it
includes the render thread hand off and is not GPU time. That depth 1 never fills means the GPU was
finishing inside a single frame; the two frame gap is the cost of the answer crossing back, not of
the copy.

### F7: level load, a hitch, and actor destruction

Three separate cases, because they fail in three different ways.

| Case | Test | Result |
|---|---|---|
| Level load | `Query.ReadbackSurvivesTeardownWithACopyInFlight` | The ring outlived the game thread's pointer, died on the render thread, **no crash and nothing leaked** |
| A hitch | `Query.SurvivesAHitch` | **Recovered 2 engine frames** after a single 500 ms tick with a gather in flight: 3 submitted, 3 accepted, **0 dropped** |
| Actor destruction | `Demo.FourHandlesSurviveAChangeOfLeader` | The four handles keep **slot and generation** across the leader being destroyed. Acquired once, released once |
| Gather target growth | `Query.GrowingTheQueryCountAfterResolvingStaysSafe` | Recovered in 3 engine frames. **This is the one that failed before the fix above** |

The level load case does not need a world swap to be exercised. What a level change does to this
system is `Deinitialize` the subsystem while GPU copies are in flight, and the hazard is a render
command writing into freed memory. It cannot happen because every render command captures
`AsShared()` rather than `this`, at all three sites. The test submits a copy, releases on the render
thread, drops the last game thread reference, and flushes: the weak pointer is dead afterwards and
nothing crashed.

**The verdict F7 owed: the readback stays and the CPU grid is not built.** The evidence is the four
rows above plus the cost table — it survives all three cases, and at four points it costs under a
third of a pass already in the frame. The fallback was *"roughly an afternoon, public API
unchanged"* at **coarse** grid fidelity, and PLAN.md is explicit that a full fidelity CPU shadow is
not an afternoon. Nothing in these numbers argues for spending it.

### The gather and the shader agree exactly, not approximately

`RubberAccumDemo.Surface.GatherAgreesWithTheShaderAtTheSameRadius`, on a **2 row folded** ribbon of
62500 cm by 200 cm at 200x deposition.

| | |
|---|---|
| `SurfaceBlurTexels` | **3.2000**, the query's contact patch *radius* in texels: a 20 cm patch over 3.1250 cm per texel, halved |
| Points compared | 20, spanning the painted band's edge in **both** axes |
| Carrying a painted value | 12 of 20, so the agreement is not two readers agreeing an empty buffer is empty |
| **Worst disagreement** | **0.000342** |

F0 established that the display read and the query read are the same kernel at two radii, because
the shader's `Radius` scales tap *spacing* and so does `BuildTaps`. Setting them equal turns F5's
check from *"these ought to be close"* into a forced equality, which catches a wrong sample
position, a wrong fold **and** a wrong weight where an approximate check catches only the first.
0.000342 is fp16 rounding across an 18 tap sum and nothing else.

**The radius is 3.2 and not 6.4.** PLAN.md said to set `SurfaceBlurTexels` "to the contact patch
width", which reads as 6.4 texels for a 20 cm patch. `BuildTaps` computes
`ContactPatchWidth / (2 * SafeCentimetresPerTexel())`, so the agreeing value is the half width.
Setting 6.4 compares a tent against one of twice the radius and fails for a reason that is not a
bug. The test derives the number from the console variable and the ribbon rather than hard coding
it.

### Not verified

- **Nothing in this entry has been looked at on screen.** The editor was closed throughout and
  Unreal MCP was unreachable — port 8000 was not listening, and the server loads at session start,
  so restarting the editor would not have made the tools available either. The F5 overlay's
  appearance is a separate capture, recorded on its own; everything above is headless.
- **The hitch test does not prove staleness reporting under a real long frame.** It ticks the
  subsystem by hand with a 500 ms delta, which exercises the ring and the banked decay, but
  `World->GetTimeSeconds()` advances on engine frames and not on that call. `SecondsSinceResolved`
  read 0.0500 s before and after, which is the test's own clock and not a 500 ms frame's. What is
  demonstrated is that the ring does not wedge and loses no submission across a long tick.
- **The 1.20 alpha per second lateral rate is an assumption, not a measurement.** It is derived from
  `LineChangeResponsiveness` 2.0 against a plausible line change, as the fastest a car in this demo
  plausibly moves across the track. The lateral error figure scales with it, so if the real rate is
  half that, so is the 0.232. No run has measured the field's actual peak lateral rate.
- **One field shape, one deposition.** The tolerance numbers are taken on a single swept band at
  100x. A field built by twenty cars over many laps has a different lateral profile, probably softer
  at the edges, which would lower the 0.232. That has not been measured.
- **The ring depth sweep measures an idle machine.** Every depth behaved identically because the GPU
  was never behind. The sweep says nothing about which depth is right when it is, which is the case
  depth exists for.
- **`QueryCostAndLatency` times the gather, not the whole frame.** It calls `GatherTaps` in
  isolation through `SampleGpuWork`, so it excludes the readback copy and the drain. No `stat GPU`
  capture of the query inside a running demo frame exists.
- **The 80 point figure is the stress case and was never run in a real scene.** It is 80 synthetic
  points on a static buffer, not twenty cars reading four wheels each.
- **The gather target growth fix is tested at one transition.** One slot to 24, and one slot to 41
  in the latency test. No test shrinks the count, because `EnsureGatherTarget` never shrinks the
  target, and none grows it twice in a run.

---

## 2026-09-25 — F5: the query overlay on screen, and a capture defect it found

```
Build:    Development, editor target run as UnrealEditor.exe -game, windowed 1600x900
Power:    MAINS, confirmed from BatteryStatus.PowerOnline True on the day of the capture
Scene:    /Game/Maps/L_SpaHarness, twenty cars, Spa layout
Driven:   entirely from -ReleaseField and -ExecCmds, no keyboard. One process per still, killed
          after the grab, 45 s after launch
Settings: r.RacetrackSurface.DepositionScale 8, RacetrackDemo.TimeDilation 20,
          RacetrackDemo.FollowTheLeader 1, and the full scale stated per still below
Capture:  PrintWindow of the game window's own client area, PW_CLIENTONLY | PW_RENDERFULLCONTENT
```

**This is the look R5's lesson demands.** Every test in the F entry above is headless, and a render
path nobody has looked at is not verified however many of them pass.

| Still | Full scale | What it shows |
|---|---|---|
| `f5-query-overlay-leading-car-wheels` | **1.0** | The overlay as it now ships: the leading car's four wheels as four bar groups in a 2 x 2 block on clear road beside it. No text, no leader lines |
| `f5-query-overlay-with-values` | **0.25** | The iteration before the text came out: the same block with each wheel's three values printed above it and a line from each wheel's sample point to its bars |

**The shipped still reads from bar length alone.** At full scale 1.0 the two left groups' red bars
are about a third full and the two right groups' nearly empty: the car is on the accumulated line
with its left side and off it with its right. Skid and marbles are near zero at that moment, so the
green and blue frames sit empty — which is the frame doing its job, an empty bar rather than a
missing one.

The values off the second still, to six decimals as the label printed them:

| Wheel | Rubber | Skid | Marbles |
|---|---|---|---|
| FL | 0.770404 | 0.000000 | 0.001457 |
| FR | 0.388040 | 0.000000 | 0.000000 |
| RL | 0.768886 | 0.000000 | 0.001462 |
| RR | 0.352536 | 0.000000 | 0.000000 |

**The readout distinguishes the wheels, which is F8's point.** Both left wheels read about 0.77 and
both right wheels about 0.37. The two wheels on each side agree with each other to within 0.04,
which is the first thing a wrong sample position would break. Every red bar in that still is full
because 0.25 clamps anything above it; the numbers carried the difference there and the bar length
carries it in the shipped still.

**`DrawDebugString` rendered, so `L_SpaHarness` has a HUD.** PLAN.md recorded that the text needs
`PlayerController->MyHUD` and is silently dropped without one. The map's world settings are a binary
asset and were not read, so this was not knowable in advance. The labels appearing in the second
still is the evidence; the shipped overlay no longer draws text, so it no longer depends on it.

### F8's marble band, observed but not kept

F8's exit is that *the readout changes as a car crosses the marble band*. It was seen once, in an
intermediate still taken with the bars still under the car:

| Wheel | Marbles |
|---|---|
| FL | 0.153174 |
| FR | 0.127678 |
| RL | 0.067599 |
| RR | 0.026179 |

**That still was overwritten by a later framing change and is not kept**, so this table rests on an
observation made in the session rather than on an image anyone can open. Three further attempts, at
51, 57 and 62 seconds, all caught the leader on its own line with marbles below 0.014 on every
wheel. That is structural rather than bad luck: G measured the band 66.4 cm *outside* the line a car
drives, and the leader drives the line, so its wheels cross the band only on the way in and out of
a corner. A still that shows it is a matter of catching that moment, and none of the three did.

### Defect found by capturing: a desktop grab can photograph another application

C10.5 took every still as a *"desktop grab of the client rect"*: bring the game window forward,
copy that rectangle of the screen. **That copies whatever is on top, not the game.** Windows refuses
`SetForegroundWindow` to a process that does not already own the foreground, so when another
application had focus the grab returned that application's window at the game's coordinates. It
happened once in this session. **The image was deleted within the same minute and was never staged
or committed.**

The replacement is `PrintWindow` with `PW_CLIENTONLY | PW_RENDERFULLCONTENT`, which asks the game
window to render its own client area into a bitmap regardless of z-order. It cannot return another
window's pixels; its failure mode is a refused call or a black frame, both visible before a still
is kept. It also no longer takes the foreground, so a capture no longer interrupts whoever is using
the machine.

**Every C10.5 still was taken the old way** and was correct because the game happened to be in
front. They are not suspect — each was looked at when taken — but the method was, and any future
capture should use this one.

### The overlay changed twice after the first look, both times on request

The first stills drew each wheel's bars at its sample point, as PLAN.md specified, and **the car
body sat on top of them**. They moved to a block beside the car; then the per-wheel text and the
leader lines came out, leaving the bars alone. The query is still submitted at the true wheel
position throughout — only the drawing moved. What that costs is written down in PLAN.md under the
2026-09-25 amendment, and the short version is that the overlay is now a readout, not a position
check: a sample taken in the wrong place would draw in the right place, and that property is carried
by `Surface.GatherAgreesWithTheShaderAtTheSameRadius` instead of by the screen.

### Not verified

- **Only `Valid` has been seen on screen.** `NotYetResolved` lasts two query frames at the start of
  a run and `OutsideRegion` needs a wheel off the ribbon, and neither was photographed. Their
  colours are pinned by `Query.OverlayBarsScaleAndStatusColoursStayDistinct`, which is arithmetic
  and not appearance.
- **F8's marble band crossing has no kept image**, as above.
- **Standalone `-game` only.** The follow camera and the overlay have not been run in PIE.
- **One moment of one run per still.** Each is wherever the leader happened to be 45 seconds in.

---

## 2026-09-25 — A flaky readback test, found by the suite and fixed

```
Build:    Development, editor target, UnrealEditor-Cmd.exe headless with the editor closed
Power:    MAINS
Test:     RacetrackSurface.Query.ReadbackResolvedFrameNeverGoesBackwards, which predates phase F5
```

The sixth full run of this phase failed one pre-existing test with *"Twenty four submissions gave 0
readable caches ... with 0 resolves and 20 dropped"*. The five runs before it passed.

**The test asserted something the GPU did not promise.** It looped 24 times over `Submit` and
`FlushRenderingCommands` with no engine frame in between, then required that at least one copy had
resolved. F2 already established that the flush waits for the render thread and not the GPU, so
whether anything retires inside that loop depends on how busy the GPU happens to be.

| Run | Resolves | Dropped |
|---|---|---|
| Alone, three runs | 11, 19, 22 | 1, 0, 0 |
| Straight after `ReadbackLosesNoSubmissionWhenTheRingFills`, three runs | 10, 8, 18 | 7, 10, 0 |
| In the full suite, once | **0** | 20 |
| **After the fix**, straight after the burst test, three runs | **22, 22, 22** | **0, 0, 0** |

The test that sorts immediately before it fires sixteen submissions into a ring of two as fast as
the render thread will take them, so the GPU is still working through that burst when this one
starts. The pair was the two-test repro; the full suite only added more GPU work ahead of it.

**Fixed by submitting once per engine frame from a latent command**, which is the pattern the
project's traps list already prescribes and the one `Performance.QueryReadbackRingDepth` uses. The
assertion is unchanged. Two full runs afterwards: **113 of 113 both times**.

**This does not change what the product does.** Only the test's loop changed. The ring behaved
correctly throughout — every submission was accepted or counted as dropped, and no resolved frame
went backwards in any run.

---

## 2026-09-25 — Cars drove into each other under time compression, and sub-stepping fixed it

```
Build:    Development, editor target, UnrealEditor-Cmd.exe headless with the editor closed
Power:    MAINS
Scene:    ARacetrackCircuit, Spa layout, 180000 cm lap, the real 20-car grid from SpawnField,
          1400 cm apart
Driven:   RubberAccumDemo.Demo.TheGridNeverDrivesIntoItself, 300 s of game time per run, every car
          ticked once per frame; plus one -game still at TimeDilation 20
```

**Seen on screen first**, reported as *the second car slowly creeps inside the first*. The bars-only
F5 still, `f5-query-overlay-leading-car-wheels`, shows it: the leader is drawn with a second car
inside it.

**It is a frame-rate bug in the car, not in the grid.** Driven at real-time frames the grid never
closes up; driven at the frames `TimeDilation 20` produces, it does:

| Frame, in game time | What it stands for | Smallest gap, **before** | **After** |
|---|---|---|---|
| 0.0167 s | 1x at 60 fps | 796.4 cm | 796.4 cm |
| 0.333 s | 20x at 60 fps | 346.6 cm | **796.4 cm** |
| 0.667 s | 20x at 30 fps | **-100.4 cm**, a car drove through the one ahead | **796.4 cm** |
| 1.0 s | 20x at 20 fps | **-156.7 cm**, then car 1 locked onto car 0 at **0.0 cm** | **796.4 cm** |

The before column comes from a diagnostic run over 900 s rather than 300 s; every negative gap in it
appeared within the first 78 s. After the fix, every compressed run finds the same smallest gap as
real time, between the same two cars — car 11 behind car 10 — at the same moment, 267 s, and
**0.0 cm from the reference**.

**The mechanism.** A car integrated its speed and distance once per frame. At 20x a frame moves a
car 1000 to 3000 cm, further than its own 2500 cm braking lookahead, and at that step size the
update is no longer one-to-one: a car still on the straight jumps its full 3000 cm while the car
just ahead, already braking, advances 1650, so two different positions can map onto the same next
position. Once two cars share a position and a speed they share them from then on — the 0.0 cm row.
In continuous time that cannot happen, because every car follows the same speed-against-distance
curve and the car behind stays a fixed *time* behind.

**The fix** is `ASplineFollowingCar::DriveAlongTrack`, run once per sub-step of at most 1/60 s of
game time. The wheels, the contacts and the marble deposit still happen once per frame. At one
sub-step it is the old code exactly: the real-time rows of every test in the suite are unchanged.

**What it moved elsewhere**, from `Demo.StrokesDoNotSmearAcrossTheTrackUnderCompressedTime`, the
only test whose measured output changed:

| | Before | After |
|---|---|---|
| Real time, widest lateral span | 5.22 texels | 5.22 texels |
| Compressed 20x, widest lateral span | 9.74 texels, 1.5x the mark | **8.86 texels, 1.4x** |
| Compressed 20x, furthest endpoint | alpha 0.9713 | alpha 0.9976, still 0 past the edge |

**A stale figure found on the way, not caused by this.** The C10 entry above records the furthest
stroke endpoint as alpha 0.7819 in real time and 0.8589 compressed, against wheels that never pass
0.7749. The same test now reports 0.9992 in real time both before and after this fix. That entry
predates G's marble throw, which deposits up to 400 cm outward of a car's line by design, so the
reach it attributed to invented lateral motion is now dominated by marbles that are meant to be
there. The 9.74 texel smear figure in the same entry was still current until this fix.

**Suite: 114 of 114**, the new test included, no GPU crash.

**On screen:** `c4-leading-car-alone-at-20x-after-substepping` is the same `-game` capture as the
F5 stills — `TimeDilation 20`, 45 s in — and shows the leader as a single car.

### Not verified

- **The frame rate of the `-game` captures was never measured.** Which synthetic row matches them
  is inferred: the merge was visible by 45 s of wall time, and headless at 20x the merge only
  happened with frames of 0.667 s or longer, so the capture ran below 60 fps. No `stat unit` or CSV
  capture exists of it.
- **The cost of sub-stepping is not measured.** At 20x and 60 fps it is twenty sub-steps per car
  per frame. A sub-step is two cornering profile lookups and two interpolations, with no spline
  search — those stay once per frame — but no timing exists for it.
- **It does not touch the lateral extrapolation recorded against C4.** Strokes are still built from
  one contact per frame, so the 8.86 texels of compressed smear is the same bounded approximation it
  was, slightly smaller.

---

## 2026-09-25 — E5 to E7: skid decay coupled to local rubber, its cost, and two marks compared

```
Build:    Development, editor target. Tests headless through UnrealEditor-Cmd.exe with the editor
          closed; captures through UnrealEditor.exe -game, windowed 1600x900
Power:    MAINS. BatteryStatus.PowerOnline True, read before the baselines, before the suite of
          record at 14:48:31 local, and before the final suite
Scene:    Tests: the engine OpenWorld template map, no actors. Captures: /Game/Maps/L_SpaHarness,
          twenty cars, Spa layout
Driven:   Tests: C++ automation, the subsystem ticked by hand. Captures: -ReleaseField and -ExecCmds
          only, no keyboard, one process per view, each still taken by PrintWindow with
          PW_CLIENTONLY | PW_RENDERFULLCONTENT at the moment the log printed the matching report
Tests:    120 of 120 pass in the final suite, started 15:12:45 local. Both build paths clean
          (editor target and RunUAT BuildPlugin)
Command:  UnrealEditor-Cmd.exe RubberAccumDemo.uproject
          -ExecCmds="Automation RunTests RacetrackSurface+RubberAccumDemo"
          -TestExit="Automation Test Queue Empty" -unattended -nopause -nosplash -nosound
```

The design is in PLAN.md under *E5 to E7, expanded 2026-09-25*. Skid decays at `1 + k * E_rubber`
times the shared rate, with `k` the existing `SaturationConstant`, in an in-place compute pass.

### E6 first: the recorded 0.3340 ms did not reproduce

Taken before any change, on the committed build, with the test the 0.3340 ms came from,
`RacetrackSurface.Performance.DecayAtTheProductionAtlas`, 64 samples:

| How the test was run | Median | Minimum |
|---|---|---|
| Alone, first after boot | 0.7440 ms | 0.7080 ms |
| First of the `Performance` group | 0.7460 ms | 0.6570 ms |
| After the eight `Accumulator` tests | 0.4390 ms | 0.4220 ms |
| 48 full-suite logs in `C:/RSBuild`, 2026-09-24 and 25 | 0.3320 to 0.4140 ms | 0.2860 to 0.3200 ms |

**The same pass costs 2.2 times as much on a cold GPU as on a warm one.** Every earlier figure came
from deep inside a full suite. So E6 does not compare against the old number at all. It times the
new pass and the old one interleaved, sample by sample, in the same run.

### E6: the coupled pass against the C10 quad, interleaved

The C10 quad is redrawn by the test exactly as `ApplyDecay` drew it. It runs on a plain non-UAV
target, which is C10's own configuration, and on the UAV-capable target, to separate the pass from
the flag. The atlas is 14400 x 1024 RGBA16f, 117964800 bytes. The populated rows fill both targets
with a tiled 256-texel noise texture, which is a worst case for framebuffer compression.

| Run | Buffer | Coupled pass | C10 quad, plain target | C10 quad, UAV target |
|---|---|---|---|---|
| Final suite, 15:12 | Empty | **0.2350 ms** | 0.3430 ms | 0.3440 ms |
| Final suite, 15:12 | Filled with noise | **1.4570 ms** | 1.4270 ms | 1.4320 ms |
| Suite at 14:48, before the fallback was added | Empty | 0.2830 ms | 0.4420 ms | 0.4450 ms |
| Suite at 14:48, before the fallback was added | Filled with noise | 1.1560 ms | 1.1200 ms | 1.1600 ms |
| `Performance` group run, before the fallback was added | Empty | 0.3080 ms | 0.4950 ms | 0.4960 ms |
| `Performance` group run, before the fallback was added | Filled with noise | 1.1420 ms | 1.1060 ms | 1.1080 ms |

The fallback does not touch an RGBA16f target, so all three runs time the same passes.

- **The absolute figures move by up to 44 % between runs, and the comparison does not move.** That
  is the same clock-state effect as the baseline table above, and it is why the passes are
  interleaved.
- **On an empty buffer the coupled pass costs 0.62 to 0.69 times the quad it replaced**, in all
  three runs.
- **Populated, the three passes are within 4 % of each other in every run**, bandwidth-bound: the
  full 117964800 bytes are read and written in 1.142 to 1.457 ms, 162 to 207 GB/s. PLAN.md predicted
  about 0.9 ms from this GPU's nominal 256 GB/s. It also predicted that the 0.33 ms was only possible
  through compression of an empty target, and that the populated C10 quad would rise too. Both held.
- **The UAV flag costs the canvas nothing measurable**: 0.3430 against 0.3440 ms empty, 1.4270
  against 1.4320 ms populated, in the final suite.
- **What 0.3340 ms described was an empty buffer.** A real session lies between the two rows.
- **At Spa's 4504.6 s half-life** a pass runs 0.05683 times a second of track time. Taking the final
  suite, that is 0.0002 ms a frame at 1x and 60 Hz empty and 0.0014 ms populated, and at 20x it is
  0.0045 and 0.0276 ms a frame.

### E5: the UAV store truncates, and the pass now rounds for itself

`RacetrackSurface.Decay.CoupledPassMatchesTheClosedFormAtEveryTexel`. Rubber at four exposures, skid
and marbles are painted across a 256 x 256 buffer, and every texel is compared before and after one
pass against `SkidRetentionForProgress` in C++.

| One pass of | Texels compared | Carrying rubber and skid | Largest skid rate | Disagreements | Worst relative error |
|---|---|---|---|---|---|
| 1/256 half-life | 65536 | 17280 | 16.33x | **0** | 3.723e-4 |
| 1/4 half-life | 65536 | 17280 | 16.28x | **0** | 4.296e-4 |

**Before the fix the same test passed, and the pass was wrong.** The tally counts the values where
round-to-nearest and truncation give different half floats, and records which one the GPU stored:

| | Distinguishable values | Stored the nearest | Stored the truncation |
|---|---|---|---|
| Before, at 1/256 half-life | 8640 | 0 | **8640** |
| Before, at 1/4 half-life | 43680 | 0 | **43680** |
| After, at 1/256 half-life | 8640 | **8640** | 0 |
| After, at 1/4 half-life | 43680 | **43680** | 0 |

The fix is `RoundToNearestHalfFloat` in `SurfaceDecay.usf`: the value is rounded to a multiple of its
own half-float spacing before the store, so the store only converts values it represents exactly.
**Truncation is invisible in one pass and large over many.** Before the fix, E7's bare mark kept
0.4586 of itself over a half-life, against 0.5000, a half-life about 11 % short.

**Over a half-life of passes, measured directly on the same fixture**, 56684 rubber and marble values:

| 256 passes of one step each | Kept, against exactly half | Worst value | Effective half-life at 56 nominal laps |
|---|---|---|---|
| Coupled pass, after the fix | **+0.71 %** | 0.76 % | 56.6 laps |
| C10 canvas quad, as shipped | **-5.06 %** | 5.35 % | **52.1 laps** |

**C10's canvas pass ran its decay 7 % fast, and nothing had ever measured it.** Its equilibrium
arithmetic is CPU arithmetic and is unaffected; the C10.5 stills with decay on faded faster than
their captions state. The coupled pass replaces that path wherever it can run.

### E7: two identical marks, traffic over one, against a prediction made before the run

`RacetrackSurface.Decay.SkidOnTheLinePavesOverAndSkidOffItPersists`, on a 256 x 256 buffer through
the subsystem at decay rate 1, one pass per tick. Both marks were laid with skid 0.666504. Traffic
over mark A lays 0.002704 between passes, which holds the rubber under it at exactly 1.000000, the
apex equilibrium the half-life is calibrated against. Mark B has no rubber.

| Passes | Half-lives | Mark A kept | Predicted | Mark B kept | Predicted | Ratio | Predicted |
|---|---|---|---|---|---|---|---|
| 64 | 0.25 | **0.5004** | 0.5000 | **0.8462** | 0.8409 | 1.691 | 1.68 |
| 128 | 0.50 | **0.2502** | 0.2500 | **0.7117** | 0.7071 | 2.845 | 2.83 |
| 256 | 1.00 | **0.0625** | 0.0625 | **0.5037** | 0.5000 | **8.053** | **8.00** |

**At the calibrated half-life that is 56 laps: a mark on the settled apex keeps 6.25 % of itself and
an identical mark off the line keeps 50 %.** Mark B's +0.7 % is the rounding drift in the table
above. The assertion is 3 % relative, fixed in PLAN.md before the run.

### E7 with real cars: the capture

`RacetrackDemo.SkidMarkPair 1609 563`, a console command added for this capture and flagged as scope
in PLAN.md. After 1609 s of track time, 20 laps, it read the rubber across the middle of the longest
apex. It laid two identical 300 cm lockup marks there, each 134 coincident contacts sized to about
1.0 exposure: one where a mark's footprint holds the most rubber, one on bare track a quarter of the
width away. Settings: 20x dilation, decay rate 1, deposition 4 so the field builds a line in twenty
laps. The same pair was run twice, once per view.

| Laps after laying | Skid-only run: on the line | off it | rubber under the line | All-channels run: on the line | off it |
|---|---|---|---|---|---|
| 0 | 0.994629 laid | 0.994629 laid | 0.658 | 0.994629 laid | 0.994629 laid |
| 7 | 0.7496 | 0.9205 | 0.873 | 0.7388 | 0.9210 |
| 14 | 0.5454 | 0.8444 | 1.036 | 0.5351 | 0.8449 |
| 28 | 0.2418 | 0.7128 | 1.340 | 0.2414 | 0.7128 |
| 56 | **0.0328** | **0.5042** | 1.784 | **0.0321** | **0.5042** |

- **The bare mark keeps 0.5042 after one half-life in both runs**, the calibrated fade plus the
  rounding drift.
- **The mark on the line keeps 3 %, a ratio of 15.4 and 15.7.** That is steeper than the controlled 8
  because the field was still laying rubber: the line's exposure climbed from 0.66 to 1.78 at
  deposition 4, and paving tracks it.
- **Dilation held:** 563 s of track time passed between reports in 28.15 s of wall time, 20.0x. The
  log's frame counter wraps at 1000, so a frame rate cannot be recovered from it and none is claimed.
- **"Laps" in the report is track time over `GetLapSeconds`**, which runs 7.5 % fast (C10.4), so 56
  reported laps is one half-life exactly and about 52 laps actually driven.

Stills, in `assets/media/`:

| Still | Shows |
|---|---|
| `e7-skid-pair-laid-skid-only` | Both marks just laid, skid channel only, caption naming the pair |
| `e7-skid-pair-7-laps-skid-only` | 7 laps on, caption 75 % and 92 % |
| `e7-skid-pair-14-laps-skid-only` | 14 laps on |
| `e7-skid-pair-28-laps-skid-only` | 28 laps on |
| `e7-skid-pair-56-laps-skid-only` | 56 laps on, caption *"the mark on the line keeps 3 %, the mark off it 50 %"* |
| `e7-skid-pair-laid-all-channels` | The second run as laid: the left mark over the red rubber line, the right over bare track beside the blue marble band |
| `e7-skid-pair-14-laps-all-channels` | 14 laps on |
| `e7-skid-pair-56-laps-all-channels` | 56 laps on |

Every still was opened and renders the road, both marks and the caption. **The display maps coverage,
`1 - exp(-3E)`, not exposure**, so on screen the 56-lap mark on the line is dimmer than the bare one
but not in the 3 : 50 proportion of the numbers. Whether that reads well is not judged here.

### Formats, and the fallback the review asked for

This RHI reports typed UAV loads for `FloatRGBA`, `A32B32G32R32F` and `B8G8R8A8`. In-place decay is
nonetheless RGBA16f only, because the rounding is to half-float spacing. An adversarial review of the
plugin commits confirmed one defect: on an RHI without typed UAV loads for RGBA16f, decay would have
stopped entirely, where C10's canvas pass ran anywhere. **Any target without a UAV now decays uniformly
on the canvas instead, with one warning that skid is not paved over.**
`RacetrackSurface.Decay.BufferWithoutAUavDecaysUniformlyOnTheCanvas` forces it with an RGBA32f buffer:
400 of 400 texels with skid over rubber kept skid at exactly the shared 0.840896, and rubber was at
most 5.96e-8 off it.

The B4 sweep decays every format again, the others through the fallback:

| Resolution | RGBA8, fallback | RGBA16f, coupled in place | RGBA32f, fallback |
|---|---|---|---|
| 1024 | 0.1050 ms | 0.1200 ms | 0.2170 ms |
| 2048 | 0.1310 ms | 0.1380 ms | 0.6020 ms |
| 4096 | 0.2610 ms | 0.2730 ms | 2.1030 ms |

All three columns are from the final suite. Each buffer holds four strokes, so these are
near-empty-buffer figures, and they are not interleaved.

### Observed on the way, and not changed

**Additive strokes have no lateral falloff.** The brush's falloff is in its alpha, and
`SE_BLEND_Additive` is `CW_RGB, One, One`, so alpha never reaches the destination. A 12.8-texel
rubber stroke read back as 13 equal texels at 0.3582. It is why the capture command places a mark by
its footprint rather than by the single highest texel, which had picked the band's edge.

### Not verified

- **The fallback has never run on hardware that needed it.** It is exercised by forcing formats
  without a UAV on an RHI that supports typed UAV loads for all three.
- **The +0.71 % drift is one fixture on one GPU.** Another GPU's `exp2` and `round` were not checked.
- **The real-car figures are two runs of one configuration**, at deposition 4, not the calibrated 1.
  At deposition 1 the line needs about five times as many laps to build.
- **Paint cost with the UAV flag was not timed interleaved.** Only the decay pass was. The sweep's
  paint figures are in the same range as before, which is not the same claim.
- **Nothing was run in PIE.** The captures are standalone `-game`.
- **The looks of the stills are not judged.** They render, and whether they read is the user's call.
