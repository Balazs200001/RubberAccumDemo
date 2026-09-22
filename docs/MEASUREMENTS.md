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
Scene:    Engine OpenWorld template map, no actors, no vehicle systems, nothing else drawing
Method:   C++ automation test, RHI timestamp queries (RQT_AbsoluteTime) bracketing each pass
Command:  UnrealEditor-Cmd.exe RubberAccumDemo.uproject
          -ExecCmds="Automation RunTests RacetrackSurface"
          -TestExit="Automation Test Queue Empty" -unattended -nopause -nosplash -nosound
```

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
