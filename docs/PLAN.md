# PLAN

Source of truth for scope. Supersedes the milestone dates in `project-proposal.md` section 6.
The proposal's *content* still stands; only its calendar does not.

Revised 2026-09-21 (start of week 4). Revised again 2026-09-22 to insert
[phase R](#phase-r--track-space-ribbon-storage), a storage restructure from world-XY square
region to track-space ribbon, which supersedes B4's region-size decision and absorbs most of D5.

## Where the project actually is

Weeks 1-3 produced the proposal and three git repositories. No system code exists. The plugin is
an empty two-module skeleton. The week 2 feasibility gate never ran, so every number in the
proposal's architecture argument is still an estimate rather than a measurement.

Against the proposal's schedule that is M1, M2 and M3 outstanding with M3 due now.

## How this plan differs

Decided 2026-09-21: **nothing is cut**, and the work is re-cast as a dependency-ordered backlog
rather than week-locked milestones. Progress is judged at checkpoints against tasks completed, not
against a date the task was supposed to land on.

The cut list from the proposal (section "Work that can be removed, in order") is retained
unchanged as contingency. It is not invoked. If a checkpoint shows the backlog is not closing,
that list is the order things come off, and the decision is recorded here when it happens.

**Stated plainly, because it has to be defensible:** four weeks remain for six weeks of planned
work, and the least familiar part of the engine (the game-thread/render-thread split) is still
ahead. Carrying full scope is the user's call, made with that known. The purpose of the
checkpoints below is to make the consequence visible early enough to act on.

### Checkpoint: Wednesday 2026-09-24

Review scope against completed tasks. No cut decision before then.

#### Early review, taken 2026-09-22 (Wednesday's checkpoint is two days out)

Requested ahead of the actual date, so this is a progress read, not the final Wednesday verdict —
two more days of work happen before that one is due.

**Completed and demonstrated:** phases A and B in full, C0. All three build paths pass, 24
automation tests pass, three repos committed. B and C0 were both closed inside the single day
since the 2026-09-21 re-plan, including the feasibility gate that had been owed since week 2.

**Also completed, same day, after the editor came up:** C1 and C2 exit conditions demonstrated in
`/Game/Maps/L_TrackHarness`, the A7 Blueprint debt closed, `ASurfaceVerificationRig` and its map
deleted, and the first real-scene measurement taken. 27 of 27 automation tests pass.

**Untouched:** C3, C4, C5, and everything in phases D through I.

**Straight answer on the cut list: not invoked.** Since the 2026-09-21 re-plan, the closed set is
phases A and B in full, C0, C1, C2, the A7 debt owed from phase A, and the "not measured in a real
scene" gap owed from C0. That is one full phase, four tasks and two pieces of recorded debt inside
about a day and a half. On tasks-completed against time, the backlog is closing faster than it is
growing, and no contingency is needed on this evidence.

**What the day did not settle, and should not be read as settled.** Three things:

1. **Phase C is not closed.** C4's time compression is what turns "twelve seconds of Simulate"
   into the phase exit, and C3 is what makes traffic spread realistically rather than five cars at
   fixed offsets. The visible result is convincing and it is not the exit condition.
2. **Phase F remains the real risk and is entirely untouched.** PLAN.md named the
   game-thread/render-thread split on 2026-09-21 as the least familiar part of the engine. Nothing
   since has tested that judgement — every task closed so far has been either pure C++, a
   measurement, or editor wiring. The readback ring in F2 and the latency work in F6 are the
   tasks most likely to overrun, and the fallback (the CPU grid) exists precisely because of it.
3. **The editor/MCP dependency cost real time today** and was flagged in the earlier draft of this
   review as a pattern. It resolved by the editor being started and the session restarted, but
   two of two editor-dependent milestones so far have stalled on tooling rather than design. That
   remains a live risk for D5, E4, G and H5, which are all editor-heavy.

If a later checkpoint does show the backlog stalling, the cut order is unchanged: D5
(region-follows-track for the 1500 m circuit) and phase G's marbles first, as the parts least
connected to the project's central claim about the emergent racing line.

*Amended 2026-09-22: the first item on that cut list no longer exists as described.
[Phase R](#phase-r--track-space-ribbon-storage) removed the need for a region that follows the
track, so D5 is now streaming behaviour only and is a smaller thing to cut. The cut order itself
is unchanged and the list is still not invoked.*

## Ordering principle

Each phase ends in something observable. A phase is done when its exit condition is demonstrated,
not when its code compiles.

Phase B is first past foundations because it is the measurement the whole architecture rests on,
and it is cheaper to take now than to discover in phase G that the region strategy was wrong.

---

## Phase A — Public API and subsystem foundations

No engine-uncertain work. Everything here is shared by both candidate storage backends, so none
of it is wasted whichever way the phase B gate resolves.

| # | Task | Exit |
|---|---|---|
| A1 | `EAccumulationChannel`, `FSurfaceContact`, `FSurfaceStateSample` | Types compile standalone, no vehicle dependency |
| A2 | `FSurfaceRegion`: world bounds to normalised UV to texel, with a defined out-of-region result | Unit-testable pure math |
| A3 | `USurfaceAccumulationSubsystem` as a `UWorldSubsystem`, lifetime and region config | Subsystem instantiates with a world, logs its region |
| A4 | Write API `RegisterSurfaceContact`, with contact interpolation over *distance* | A contact at 55 m/s at 30 fps leaves a continuous streak, not dots |
| A5 | Per-frame contact queue and flush | Queue drains, ordering defined |
| A6 | `Build.cs` dependencies, `RACETRACKSURFACE_API` only where needed | Plugin builds in isolation via `RunUAT BuildPlugin` |
| A7 | Blueprint callable wrappers over the public API | Callable from a Blueprint actor |

*Exit: the write API exists, is callable from C++ and Blueprint, and stores nothing yet.*

**Status 2026-09-22: complete.** Builds in isolation (`BuildPlugin`), as the unity game target, and
as the editor target. Twelve automation tests pass headlessly.

**A7 debt closed 2026-09-22.** The wrappers were previously only compiled, never called. They are
now called from `BP_SurfaceRegionDisplay`'s `EventBeginPlay` in `/Game/Maps/L_TrackHarness`, doing
a real job — configuring the region from a map-editable struct and binding the accumulation target
into the debug material — rather than in a graph written only to prove they work.
`ConfigureSurfaceRegion` and `GetAccumulationTarget` are confirmed executing by their log output
during a live session; `ResetSurface` sits between them on the same exec chain, so it ran too,
though it logs nothing of its own.

**Still not exercised from Blueprint:** `GetSurfaceRegion` and `RegisterSurfaceContact`.
`RegisterSurfaceContact` is the significant one — it is the wrapper a plugin user is most likely
to call, and every current caller reaches it from C++ instead. Worth a deliberate Blueprint
exercise before release.

## Phase B — Feasibility gate (owed from week 2)

The measurement that the architecture argument depends on.

Amended 2026-09-22, B3 method. The original plan said to drive `stat GPU` and the CSV profiler
through MCP. The UE 5.8 toolsets expose no console-command or CVar-set tool — `EditorAppToolset`
offers `SearchCVars` for lookup only, and nothing anywhere executes a console command. So
`stat GPU`, `CsvProfile` and `FreezeRendering` are not reachable from MCP at all.

B3 instead times the passes directly with RHI timestamp queries inside a C++ automation test:
`RHICreateRenderQuery(RQT_AbsoluteTime)` bracketing each pass, read back with
`RHIGetRenderQueryResult`, gated on the RHI's timestamp-query support flag. This is a better
measurement than the original anyway: it isolates the passes under test rather than the whole
frame, returns a number in a log rather than one read off a screenshot, and re-runs unattended for
the later profiling passes in D6, E6 and H4. Run headlessly via `UnrealEditor-Cmd.exe`, and from
the live editor via `AutomationTestToolset`.

| # | Task | Exit |
|---|---|---|
| B1 | Persistent render target allocated and drawn into by the canvas path | A streak persists across frames — **met** |
| B2 | Receiving material function sampling by world position (authored through `MaterialTools` over MCP, logged in `EDITOR_SETUP.md`) | Streak visible on a surface — **met** |
| B3 | Measure: one paint call, N paint calls, full-buffer decay pass, via RHI timestamp queries in an automation test | Real numbers recorded in `docs/MEASUREMENTS.md` |
| B4 | Area vs texel density decision: region size, memory, channel count, precision | Decision recorded with the numbers behind it |
| B5 | Decide saturating accumulator: hardware blend vs double buffer | Decision recorded |
| B6 | Channel packing, with the constraint that skid and buildup are sampleable together | Layout recorded |

*Gate: four contact points at 60 Hz within roughly 0.5 ms GPU on a 4070 without batching. If not,
the flush strategy is redesigned before anything else is built. If no usable region size fits
memory at skid-mark density, the region strategy changes here.*

**Status 2026-09-22: PHASE B COMPLETE.** Gate passed, B1 to B6 all demonstrated.

Four contacts cost 0.3610 ms and 0.3040 ms across two runs, inside the 0.5 ms budget unbatched.
The flush strategy is not redesigned. Numbers and reasoning in
[MEASUREMENTS.md](MEASUREMENTS.md).

Decisions taken:

| # | Decision |
|---|---|
| B4 | 2048 square RGBA16f over a 200 m region, 32 MiB, 9.77 cm per texel. 4096 square stays configurable at 128 MiB. **Superseded on shape and extent by phase R, 2026-09-22. RGBA16f and 9.77 cm per texel survive; the square world-XY window does not** |
| B5 | Additive write of an unbounded exposure value, saturation applied at sample time. Rejects both hardware-blend saturation and the double buffer |
| B6 | One RGBA16f target: R buildup, G skid, B marbles, A unused and not writable through the canvas path |

Two findings that change later phases:

- **Paint cost is per contact, not per texel**, at roughly 0.075 ms per contact and flat across a
  64-fold range of buffer sizes. Eighty contacts costs 5.0 to 6.0 ms. **Batching moves from phase H
  to a prerequisite of phase C**: unbatched, the harness supports about six cars, not the twenty
  that C3 and E7 need. Added as C0 below.
- **A 1500 m circuit does not fit in one region at any resolution measured.** 4096 square still
  gives 36.62 cm per texel over 1500 m. The single-region design is a 200 m instrument, and the
  circuit case needs a following region or tiling. Folded into D5. *Resolved 2026-09-22 by
  [phase R](#phase-r--track-space-ribbon-storage), which is neither of those two options: a
  1500 m circuit is 15360 x 123 texels at 9.77 cm per texel, 15.1 MiB, in one unfolded ribbon.*

### B1 and B2 exit conditions, demonstrated 2026-09-22

Both were outstanding at the start of the day and both are now met, in
`/Game/Maps/L_SurfaceVerification` with `ASurfaceVerificationRig` sweeping one contact at
5500 cm/s across a 200 m plane.

- **B1, "a streak persists across frames": met.** A single frame paints roughly 92 cm of stroke.
  The captured frame shows an unbroken mark spanning the full 160 m sweep, so every frame before it
  is still in the buffer. That is the persistence proof, in one frame, without needing two.
- **B2, "streak visible on a surface": met.** The mark renders green, which is the G channel, which
  is `EAccumulationChannel::SkidMark`. The channel mask, the world-position-to-region-UV mapping
  and the saturation mapping are therefore all doing what they were built to do, because any one of
  them being wrong gives a black surface, a mark in the wrong place, or the wrong colour.

The run also confirms two earlier decisions are actually live rather than merely recorded:

```
LogRacetrackSurface: Published region centre (0.0, 0.0), world size 20000.0
                     and saturation constant 3.000 to MPC_RacetrackSurface
LogRacetrackSurface: Accumulation buffer allocated at 2048 x 2048, 33554432 bytes
```

33554432 bytes over 2048 square is 8 bytes per texel, so the B4 and B6 choice of RGBA16f is in
force, not just written down.

**Not verified.** The saturation curve itself is unmeasured. The mark is visibly bright, which
proves the mapping is being applied at all, but nothing distinguishes a coverage of 0.95 from one
of 3.98 on screen. `ExposureToCoverage` has no automation test, and it is now the contract between
the material and the phase F query path.

### Where the saturation constant lives, decided 2026-09-22

B5 left `1 - exp(-k * E)` needing the same `k` in the material and in the phase F query path.

`RacetrackSurfaceMapping::SaturationConstant` in `SurfaceAccumulationMapping.h` is the single
source of truth, at **3.0**. `ExposureToCoverage` beside it is what phase F calls directly.
`USurfaceAccumulationSubsystem` pushes the constant, the region centre and the region world size
into `MPC_RacetrackSurface` on `ConfigureRegion` and on first buffer allocation, and the material
function reads them as collection parameters. Nothing is typed into the graph, and the collection
asset defaults `k` to 0 so that a missing push shows up as no coverage anywhere rather than as a
stale second copy that quietly works.

3.0 is derived from B6 rather than chosen for looks: at the 0.002 per contact buildup increment
that the RGBA8 precision floor ruled out, 500 contacts give an exposure of 1.0, which maps to 0.95
coverage.

**Not verified.** The mapping has never been evaluated against a real buffer. That is what the
outstanding visual check is for, and no automation test covers `ExposureToCoverage` yet.

## Phase C — Test harness

Validation instrument, not demo dressing. Needed before D and E can be judged at all, because
buildup takes dozens of laps and marbles hundreds.

| # | Task | Exit |
|---|---|---|
| C0 | Batch the per-frame strokes into one draw rather than one draw per contact. Added 2026-09-22 from the B3 measurement | Eighty contacts within the four-contact budget — **met 2026-09-22** |
| C1 | Spline-generated track | Closed circuit in the level |
| C2 | Autonomous spline-following cars, no physics, driving the public API only | Cars lap unattended |

**Status 2026-09-22: C1 and C2 COMPLETE, exit conditions demonstrated.** Both live in
`Source/RubberAccumDemo/`, the host module, not the plugin. `ARacetrackCircuit` and
`ASplineFollowingCar` build against all three targets (`BuildPlugin`, the editor target, the game
target) and three new automation tests pass alongside the existing 24:
`RubberAccumDemo.Track.IsClosedLoopWithPositiveLength`,
`.WrapsDistanceBeyondTrackLength`, `.LateralOffsetMovesAcrossTrackWidth`. Run with
`-ExecCmds="Automation RunTests RacetrackSurface+RubberAccumDemo"` to cover both prefixes in one
pass; 27 of 27 pass.

**C1 exit — "closed circuit in the level": met.** `ARacetrackCircuit` placed in
`/Game/Maps/L_TrackHarness` generates a closed spline with `bClosedLoop` true, 18 points, and a
reparam table ending at **30921 cm of lap length**. Footprint is ±6600 by ±3600 cm, inside the
200 m region with margin.

**C2 exit — "cars lap unattended": met.** Five `ASplineFollowingCar` at five lateral offsets
complete closed laps in a Simulate session with no input, and leave five separately identifiable
rubber trails following their five distinct paths. Captured in
[`c1-c2-track-harness-five-cars-topdown.png`](../../2627-Y3A-PR-243663/assets/media/c1-c2-track-harness-five-cars-topdown.png).
Confirmed in the log rather than only on screen: the region is reconfigured and published, the
buffer allocates at 2048 square RGBA16f, and no car logs the missing-circuit warning.

**Phase C is still open.** C3, C4 and C5 are untouched, and the phase exit — *press play, walk
away, come back to a marked track* — needs C4's time compression. Twelve seconds of simulation is
not walking away.

**C3, C4 and C5 are held behind [phase R](#phase-r--track-space-ribbon-storage), decided
2026-09-22.** Both are written against the coordinate the surface is stored in — C3 varies speed
and slip with position along the track, C4 lengthens the per-frame stroke — and that coordinate is
being replaced. C4 in particular is the task that breaks phase R's straight-quad approximation, so
it needs the new domain in place to be built against at all.

### C1: how track width and the racing-line corridor are expressed

Decided 2026-09-22. `ARacetrackCircuit` exposes a single scalar `TrackWidth` (constant along the
whole circuit, not per-point) and a lateral offset `Alpha` in `[-1, 1]`, `0` at the centreline and
clamped at the edges. `GetWorldPositionAtDistance(Distance, Alpha)` is the one function that turns
a point on the circuit into a world position: it samples the spline at the (wrapped) arc-length
distance for the centreline, then moves along the spline's right vector by
`Alpha * TrackWidth * 0.5`.

This is the hook both later tasks need without either being implemented yet:

- **C3** spreads traffic by giving each car a different constant `Alpha` (already a per-car
  property on `ASplineFollowingCar`, unused for anything but a fixed offset in C2).
- **F**'s on-line/off-line query does not need a geometric "racing line" at all — the racing line
  is supposed to be emergent from measured rubber buildup, not authored — but it does need the
  same `(Distance, Alpha)` coordinate to interpret *where across the width* a buildup reading came
  from. That inverse mapping, world position back to `(Distance, Alpha)`, is not implemented; it
  is not needed until F and adding it now would be scope not asked for yet.

**Rejected:** per-spline-point width, and an authored second "racing line" spline distinct from
the centreline. Both add a parallel data structure to keep in sync with the centreline for a
project where nothing through phase E needs width to vary or a line to be pre-drawn — the racing
line is meant to come from where cars actually drive, not from a curve someone placed by hand.

**Track shape, decided 2026-09-22.** The circuit is a stadium: two straights joined by two 180°
semicircular corners, radius and straight length both editable. This is the simplest closed-form
shape that is guaranteed non-self-intersecting for any positive parameter values, which matters
more here than corner variety — a self-intersecting generated track would cost time to debug for
no benefit before phase E. Default dimensions (60 m straights, 30 m corner radius, 12 m width)
keep the whole circuit's footprint under the default 200 m single region from B4, since D5's
region-follows-track work for larger circuits has not happened yet; a track that does not fit the
active region would silently discard most contacts as outside-region.

*Amended 2026-09-22 by [phase R](#phase-r--track-space-ribbon-storage): the footprint constraint
is lifted. Storage is sized from the centreline, so track size is bounded by the 1600 m unfolded
limit rather than by a region box, and the default dimensions are now a harness convenience
rather than a requirement. The stadium shape itself stands; a non-self-intersecting track is
still what the ribbon needs, and more so — a track that crosses itself has two distinct
centreline distances at one world position, which the inverse mapping cannot resolve.*

Spline points use `ESplinePointType::Curve` (Catmull-Rom auto-tangent) rather than hand-computed
tangents for true circular arcs. Exactness was not needed by anything through C2's exit condition,
only closure and reasonable smoothness, and auto-tangent is the simpler code.

### C2: what the car does not yet do

`ASplineFollowingCar` calls the same `RegisterSurfaceContact` overload
`ASurfaceVerificationRig` already used, with the same velocity-based interpolation assumption (see
`BuildStroke`). Speed, slip amount, contact width, channel and the lateral offset are all constant
per car for now — C3 is what makes speed and slip vary with position and gives the offset a reason
to differ car-to-car. Channel defaults to `RubberBuildup` rather than `SkidMark`, since steady-
state lapping without braking events reads as tyre scrub building rubber, not a lockup mark; that
distinction is a C2 decision, not a C3 one, because it is about what a car with no physics is
plausibly doing to the surface, not about position-dependent behaviour.

If `Track` is left unset on a car instance, `BeginPlay` finds the first `ARacetrackCircuit` in the
world, so placing several car instances in a level does not need each one wired up individually.

`ASurfaceVerificationRig` is **deleted**, along with `/Game/Maps/L_SurfaceVerification`, which
contained nothing else once the rig was gone. The cars replaced its driving half and
`BP_SurfaceRegionDisplay` replaced its display half.
| C3 | Per-position speed and slip curves, lateral offset per car | Traffic spreads across the line — **met 2026-09-23** |
| C4 | Time compression with lap counter | 50 laps in about two minutes |
| C5 | Fixed-camera observation mode | Repeatable identical load per run |
| C6 | Shared racing line with per-corner variation. Added 2026-09-23, requested directly | A dominant line with distinguishable arcs around it in corners, reproducible from a seed |

*Exit: press play, walk away, come back to a marked track.*

### C3, implemented 2026-09-23. The C++ is done and tested; the exit condition waits on R5.

Five new automation tests, 41 of 41 passing, all three build paths clean. **The exit condition
"traffic spreads across the line" cannot be demonstrated until R5**, because nothing renders the
buffer correctly yet. What is demonstrated is the behaviour that produces the spread, by number.

**Cornering comes from the circuit's own geometry, not from sampling the spline.** `ARacetrackCircuit`
generates the stadium from `StraightLength` and `CornerRadius`, so it already knows where the
corners are in closed form. `GetCorneringAmountAtDistance` returns 1 inside a corner and 0 on a
straight by testing the local X of the point against half the straight length.

The alternative was central-differencing the spline tangent to recover curvature numerically. It was
rejected: no public UE 5.8 API returns curvature, so it needs a tunable step size that has to sit
above the reparam table's own resolution and below one segment, and it depends on closed-loop wrap
behaviour in `FInterpCurve` that could not be confirmed from the source. That is the clever
implementation where the obvious one was in the same file. The accessor is checked against the
geometry rather than against itself: the share of the lap it reports as cornering measures
**0.6110 against 0.6110** from `2*pi*R / perimeter`.

**Speed, slip and line are all functions of position now.**

| Quantity | On a straight | In a corner | Measured |
|---|---|---|---|
| Speed | `StraightLineSpeed` | x `CorneringSpeedFraction` | 3000 → 1650 cm/s |
| Slip, which sets stroke intensity | `StraightSlipAmount` | `CorneringSlipAmount` | 0.080 → 0.450 |
| Line | `BaselineLateralOffsetAlpha` | x `(1 - CorneringLineCommitment)` | 0.600 → 0.060 at commitment 0.9 |

Two mechanisms stop that being a square wave. The car reads cornering at its own position **and**
at `BrakingLookaheadCentimetres` ahead, taking the maximum, so it slows *before* the corner rather
than on entering it — measured at 2583 cm/s while still 538 cm short of the entry. Speed and line
then ramp toward their targets under `FMath::FInterpConstantTo` at bounded rates, so a car has a
finite braking distance and a finite rate of changing line.

**The car now uses the struct overload of `RegisterSurfaceContactInTrackSpace`.** Because the line
moves within a frame, `LateralOffsetAlphaPerSecond` is genuinely non-zero, and `BuildStrokes` uses
it to back-derive the stroke's start alpha. The six-argument overload leaves that field zero and
would paint each frame's stroke at a constant offset, losing the lateral movement.

**The circuit spawns the field.** `CarCount` and a per-driver derivation replace five hand-typed
offsets in the map. Each driver's baseline offset is spread across `DriverLateralSpread`, and its
line commitment is interpolated from `DriverLineCommitmentAtCentre` to `DriverLineCommitmentAtEdge`
by how far off-centre it starts — a driver already on the line has little to give up, one sitting
wide has a lot. The defaults reproduce the C2 capture's -0.4 to +0.4 spread exactly.

**The emergence tension, stated because it is the obvious oral-assessment attack.** C1 rejected an
authored racing line, and E and F rest on the line being emergent from measured buildup. A design
where cars follow a computed ideal line would make that tautological.

This design does not compute one. No component stores, looks up or targets a "best" lateral
position; each car responds only to local geometry at its own position, with its own coefficients.
What spreads the traffic is that drivers differ, and what concentrates it in corners is that each
driver independently gives up some of its own offset.

**The honest limit, which should be stated rather than defended:** the response formula and the
coefficient spread are still authored, so the *result* is a consequence of choices made here. The
strong form of the claim — cars discovering a fast line by measuring their own lap times — is a
controller-design project of its own and is deliberately out of scope. What C3 claims is the weaker
and still real thing: it authors the question, not the answer. What later phases read is the
time-integrated buildup through the saturating accumulator and the decay pass, which no car can
see. Any write-up should make the weak claim, not the strong one.

**Verified on screen 2026-09-23**, once R5 was met. The oval capture shows traffic fanning into
distinct lines on the straights and converging into a single band through both corners, which is
the design's central behaviour and was previously only a pair of numbers in a test. The five
map-placed cars were deleted; the circuit's field is the only population now.

### Added 2026-09-23, outside the original C3 scope: a Spa-shaped layout

Requested directly rather than planned. `ARacetrackCircuit` gains an `ETrackLayout` selector, and
`RacetrackLayouts::SpaCentreline` holds 76 points read off the circuit's plan view. The spline is
built from them, measured, then rescaled about its centroid to hit `AuthoredLayoutLapLength`.
`/Game/Maps/L_SpaHarness` runs it with twenty cars. Captured in
[`spa-harness-twenty-cars-rubber-topdown.png`](../../2627-Y3A-PR-243663/assets/media/spa-harness-twenty-cars-rubber-topdown.png)
and, at La Source, in
[`spa-harness-la-source-rubber-closeup.png`](../../2627-Y3A-PR-243663/assets/media/spa-harness-la-source-rubber-closeup.png).

**The lap is 1500 m, not a halved Spa, and the reason is the unfolded limit.** 16384 texels caps
the lap at 1600 m at production density. Full Spa needs 71721, halved needs 35860, quarter needs
17929 — none fit, and folding is not implemented. 1500 m is 21.4 % of Spa and gives
**15361 x 123 texels, 15115224 bytes**, printed at runtime, against the 15360 x 123 and 15.1 MB
this plan predicted for a 1500 m circuit before any of it was built.

Track width stays 1200 cm rather than scaling to 3.2 m, because at true scale two cars would not
fit side by side and there would be no lateral spread to observe. So this is Spa's *shape* at
21 % with a proportionally wide track, not a scale model, and it should be described that way.

**This forced C3's cornering signal to be generalised, and that is the interesting part.** The
stadium knew its corners in closed form because it generated them from `StraightLength` and
`CornerRadius`. An authored layout has no such parameters — the shape is data. So cornering is now
a profile sampled from the spline at construction and normalised by the track's own tightest
curvature.

That reverses the choice recorded above, and the reversal is defensible on its own terms:

- **The analytic test was right for a generated shape and is not available for an authored one.**
  It was never a general solution, only the correct one for the case that existed.
- **The stadium's closed-form answer becomes the oracle for the general implementation.** The
  cornering-fraction test still measures 0.6110 against `2*pi*R / perimeter`, so the sampled
  profile is checked against geometry the sampler knows nothing about. That is a stronger position
  than the analytic version was ever in, because the analytic version could only be checked
  against itself.
- **Normalising by the track's own tightest curvature has no magic constant** and is self-scaling,
  so it works at 1500 m or 7 km without retuning. On the stadium it still returns exactly 0 or 1;
  on Spa it grades corners, which turns speed and slip into genuinely continuous curves rather
  than the square wave the stadium produced.

**Not verified.** The centreline points were read off a plan view by eye, so the shape is
recognisably Spa but is not survey-accurate, and no corner radius matches the real circuit. The
twenty-car Spa run has not been profiled — the R6 figures are the oval at 3167 x 123, and Spa's
buffer is roughly five times larger, which affects the decay pass and not the paint pass on the
evidence of B4.

### Corrected 2026-09-23 after watching it run: the line aims at the apex, not the centreline

The model recorded above had a car scale its own baseline offset toward zero when cornering, so
every driver converged on `alpha = 0`. Seen on screen that is plainly wrong. The centreline is not
a racing line, and because all twenty cars aimed at the same place their paths pinched into a point
at every corner entry rather than fanning into a band.

Corrected: a car eases toward the **inside of the corner**, and which side that is comes from the
sign of the local curvature rather than from anything authored. `CorneringProfile` is stored signed
for that reason; `GetCorneringAmountAtDistance` returns its magnitude and
`GetSignedCorneringAtDistance` returns the value. The target is
`Lerp(BaselineAlpha, ApexAlpha * Sign(SignedCornering), Commitment * CorneringAmount)`.

Measured, two cars on the same 0.60 baseline: the uncommitted one holds **0.600**, the committed one
now reaches **-0.402** on the inside, where under the old model it reached 0.079 on the centreline.

The constant lateral rate was also replaced by easing. A constant rate turns every change of target
into a V in the painted path, which was the second half of what looked wrong.

**The emergence argument is unchanged in form and is worth restating against the new model**, because
"drive toward the apex" sounds more like an authored line than "drive toward the centre" did. It is
not: the car reads only the curvature at its own position, the side falls out of that curvature's
sign, and how far it commits is its own parameter. Nothing stores a line. What the design does not
do, and should not be claimed to, is model outside-entry and wide-exit — that comes out of the
bounded lateral rate as a side effect, not from intent.

### C3's exit condition is met, with the caveat that its demo tuning is not its design

*Traffic spreads across the line* is demonstrated: distinct bands on the straights converging into a
concentrated band through every corner, on both layouts.

Several values are demo tuning rather than results, and should be read that way: `GridSpacing` packs
the field into a train instead of spreading it around the lap, `ElevationExaggeration` at 2.5
overstates Spa's gradients by roughly double, and the observer, time-dilation and deposition
controls exist to make the harness watchable. None of them affect what the surface system stores.

**Deposition has its own keys rather than following time dilation, and the reason is the same one
behind the swept-share fix.** Time compression already accelerates buildup correctly: more laps
happen per real second, so more rubber lands per real second. Scaling deposition with dilation as
well would count the compression twice and, worse, make **rubber per lap depend on playback speed**
— a lap has to lay the same rubber whether you watch it at 1x or 20x. `.` and `,` move time, `]`
and `[` move the rate, and they are independent.

**Known defect, found by measuring rather than by looking: the field does not hold its grid
spacing.** `RubberAccumDemo.Car.FieldHoldsItsSpacing` starts two drivers 1400 cm apart and ticks
them through 3000 frames:

```
Two drivers on different lines and commitments started 1400 cm apart and ended 791 cm apart
after 3000 ticks, a drift of -609 cm
```

Live, the gap between the first two cars measured about 13800 cm after roughly five minutes, far
more than that test shows, so the drift is not a one-off transient.

Speed is a function of position only, so two cars should hold station. They do not, and the
probable cause is that speed is rate-limited by `FInterpConstantTo`: two cars at different points on
the lap are accelerating and braking differently at any instant, and a rate limit makes the
resulting speed-at-position depend on the approach rather than only on the position. Whether that
merely oscillates or accumulates is **not established**, and the live figure suggests it
accumulates.

Not fixed. It does not affect the surface system — contacts are registered wherever the cars are —
but it does mean the twenty cars stop reading as a race after a few minutes, and any future
measurement that assumes a known field distribution cannot rely on it. The test is deliberately left
asserting a loose bound so it records the behaviour rather than hiding it.

**The time controls expose a phase C4 problem early, and it is not fixed.** At 20x dilation and
100 fps a stroke is about 330 cm, which sits on the `sqrt(8 * R * CentimetresPerTexel)` bound of
342 cm for a 15 m radius corner at Spa's scale. Below about 10x there is margin; above it, or at a
lower frame rate, strokes in tight corners will visibly cut the corner. That is exactly the sagitta
failure this plan predicted for C4, arriving through the demo controls rather than through C4
itself. **Stroke subdivision remains C4's first task and is not started.**

**Status 2026-09-22: C0 COMPLETE.** Eighty contacts cost 0.0890 to 0.1150 ms against a four-contact
budget that was 0.2620 to 0.2820 ms, so the exit condition is met with room to spare — eighty
contacts now cost less than four did. Numbers, including the varying-heading case and the
batched-equals-sequential correctness check, in [MEASUREMENTS.md](MEASUREMENTS.md).

The approach that shipped is **not** the one proposed below. Investigation of the engine source
before implementing found `FCanvas::SetAllowedModes` / `GetAllowedModes` to be public, and
`Flush_GameThread` to early-return when the `Allow_Flush` bit is clear. So the per-item flush can be
suppressed on the canvas the Kismet helper already returns, and no custom `FCanvas` is needed at
all. The whole change is a scoped guard that clears `Allow_Flush` for the stroke loop and restores
it before `EndDrawCanvasToRenderTarget`.

That is strictly better than the proposal below on three counts, which is why it was taken:

- **Nothing is duplicated from the engine.** The proposal would have copied about 25 lines of
  `BeginDrawCanvasToRenderTarget`'s body into the plugin, including the render-target resolve. That
  copy would silently drift if Epic changed the original in a later 5.x.
- **The resolve stays Epic's problem.** `EndDrawCanvasToRenderTarget`'s
  `TransitionAndCopyTexture` is what returns the target to a shader-readable state for the material
  that samples it every frame. Hand-rolling meant owning that correctly; this way it is untouched.
- **It is a guard and two calls**, against a rewrite of the paint path.

The restore is not optional and is the one sharp edge: `EndDrawCanvasToRenderTarget` performs its
real flush through the same `Flush_GameThread`, so leaving the bit clear makes that a no-op too and
*nothing renders at all*, silently, with no warning. The canvas is also shared per world, so a
missed restore would disable canvas rendering for every other caller. Hence the RAII guard rather
than two bare calls.

### C0 approach as originally proposed, superseded by the above

**Root cause of the B3 cost, found by reading the engine source rather than assumed.**
`PaintStrokes` already issues every stroke inside a single `BeginDrawCanvasToRenderTarget` /
`EndDrawCanvasToRenderTarget` pair, so the one-call-per-frame structure the plan wants is already
there at the C++ call-site. The cost is coming from underneath it: `UKismetRenderingLibrary::
BeginDrawCanvasToRenderTarget` (`KismetRenderingLibrary.cpp:788`) hardcodes
`FCanvas::CDM_ImmediateDrawing` with the comment *"Draw immediately so that interleaved
SetVectorParameter (etc) function calls work as expected"*, and `FCanvas::DrawItem`
(`Canvas.cpp:2084`) calls `Flush_GameThread()` after every single item when the canvas is in that
mode. So every `K2_DrawTexture` submits and flushes to the render thread on its own, independent of
how many strokes share the call — which is exactly the flat ~0.075 ms-per-contact, resolution-
independent cost B3 measured.

**The batching this needs already exists in the engine; nothing bypasses it today.**
`FCanvasTileItem::Draw` (`CanvasItem.cpp:419`) resolves its batch through
`Canvas->GetBatchedElements(ET_Triangle, BatchedElementParameters, Texture, BlendMode)` — the batch
key is texture and blend mode only. Rotation and per-tile colour are baked into the tile's vertex
positions and vertex colour before the batch add (`PushRelativeTransform`), not carried as per-draw
state, so many differently-rotated, differently-coloured tiles sharing one texture and blend mode
already merge into one `FBatchedElements` batch and one draw call at flush. Every production call to
`PaintStrokes` (`FlushPendingContacts` never passes a `PaintMode`, so it is always `Additive`) uses
the single shared brush texture and a single blend mode for every stroke in the array, regardless of
which of the three channels each stroke targets — channel is just a vertex-colour mask. So the
batch key is already uniform across a frame's strokes; only the forced per-item flush is stopping
them from landing in one draw.

**Proposed fix.** Replace the two `UKismetRenderingLibrary` calls in `PaintStrokes` with a small
internal helper that builds its own `FCanvas` around the same render target resource, using
`FCanvas::CDM_DeferDrawing` (the class's own default) instead of the Kismet helper's hardcoded
immediate mode. The helper reproduces the handful of things the Kismet functions otherwise do around
the canvas — `FlushDeferredParameterCollectionInstanceUpdates` and the deferred-resource-update
render command on begin, the resolve-to-texture render command on end — because
`UKismetRenderingLibrary` does not expose a draw-mode argument to opt out of immediate mode any other
way; that duplication is the actual cost of this approach, not new rendering logic. The per-stroke
loop building `FCanvasTileItem`s is untouched. One explicit `Flush_GameThread()` after the loop
replaces the implicit per-item ones. `ApplyDecay` and `Clear` are unaffected; they already draw a
single item per call, so immediate mode costs them nothing.

**Rejected alternative.** A hand-rolled RHI path — build a vertex/index buffer of stroke quads once
per frame and issue one `RHICmdList.DrawIndexedPrimitive` from a render command with a small custom
shader. This also collapses to one draw call, and removes the `Canvas` dependency entirely, but it
reimplements textured-quad batching, blend-state selection and rotation math that
`FCanvasTileItem`/`FBatchedElements` already does correctly, for the same measured outcome. Per the
project's stated preference for existing engine systems over reimplementing them, this is only the
fallback if the deferred-canvas approach does not actually collapse to a small number of draw calls
once measured — the batch-key reasoning above says it should, but it has not been measured yet.

**Verification plan.** Re-run `RacetrackSurface.Performance.PaintAndDecay` unchanged and compare the
eighty-contact figure against the 5.99 ms (later 4.22 ms after the test-debt commits) baseline
already in `MEASUREMENTS.md`. Add a correctness check alongside it — same final buffer contents from
a batch of strokes as from issuing them one at a time — since changing the draw path must not change
what gets painted. Both numbers go in `MEASUREMENTS.md` as the C0 before/after.

**Superseded.** Kept for the record because the reasoning above it — the root cause, and the
batch-key analysis — is what the shipped fix rests on. Only the mechanism changed.

## Phase R — Track-space ribbon storage

Added 2026-09-22, between phase C and phase D. This is a storage restructure, not a feature. It
replaces the world-XY square region with a ribbon parameterised by distance along a centreline
spline and lateral offset across the track.

C3, C4 and C5 are deliberately held behind it. Time compression and per-position speed curves are
both written against the coordinate the surface is stored in, and that coordinate is about to
change, so building them first is work done twice.

### Why, with the numbers that forced it

The B4 sweep measured texel density against memory for a *square* region and concluded 2048 square
RGBA16f at 9.77 cm per texel. Applied to a real circuit rather than a 200 m window, that shape
fails on memory by a factor that no resolution setting can recover.

Spa-Francorchamps is the worked case: about 7.0 km of centreline, roughly 15 m wide, inside a
bounding box of about 2.5 by 1.5 km.

| Storage shape | Texels | RGBA16f | Notes |
|---|---|---|---|
| World-XY rectangle over the bounding box, 9.77 cm/texel | 25589 x 15353 = 393 M | **3.14 GB** | Exceeds the 16384 maximum dimension as well |
| Track-space ribbon, 7004 m x 15 m, 9.77 cm/texel | 71721 x 154 = 11.0 M | **88 MB** | Whole circuit |

The track surface is about 84,000 m² of a 3,750,000 m² box. **97.8 % of the square region's memory
stores grass.** The ribbon is roughly 36 times better and covers the entire real circuit rather
than a 200 m window of it.

Tiling does not fix this. Square tiles around a thin diagonal ribbon are still the wrong shape —
a tile is either mostly empty or too coarse, and the tile set has to be indexed by world position,
which is the thing the ribbon makes unnecessary.

The same arithmetic on the current harness circuit, which does fit today: 30921 cm of lap at
1200 cm wide is 3167 x 123 texels, **2.97 MiB against the 32 MiB** the 200 m square region
allocates for it. About eleven times better on a track the old design could already hold.

Every figure in this section is asserted or printed by
`RacetrackSurface.Ribbon.TexelExtentsFollowTrackDimensions`, so none of it is arithmetic done
once in a document and never checked again.

### What this supersedes

- **B4's region-size decision is superseded on shape and extent.** 2048 square over a 200 m window
  is replaced by a ribbon sized from the centreline. What survives B4 unchanged: RGBA16f as the
  format, 9.77 cm per texel as the density target, and the whole B4/B5/B6 measurement set. Those
  are properties of the buffer, not of how it is parameterised.
- **B4's recorded limit "a 1500 m circuit does not fit in one region at any resolution measured"
  is resolved rather than carried.** A 1500 m circuit is 15360 x 123 texels, 15.1 MiB, and fits
  unfolded.
- **Most of D5 is absorbed.** D5 was carrying "region-follows-track for the 1500 m circuit" as
  well as World Partition streaming; the cut list names it by that first description. The ribbon
  removes the need for a following region entirely. D5 keeps only genuine streaming behaviour.
- **C1's constraint that the circuit must fit inside the 200 m region is lifted.** The stadium's
  default dimensions were chosen to stay under it. Track size is now bounded by the unfolded
  limit below, not by a region box.
- **D3's "region edge" changes meaning.** A ribbon has lateral edges at `|alpha| > 1` and, on a
  closed loop, no longitudinal edge at all — the distance axis wraps. D3 is now a smaller question
  than it was.
- **The B2 material function's sampling method is superseded.** `MF_SampleRacetrackSurface`
  samples by world position through `MPC_RacetrackSurface`. Track-space storage cannot be sampled
  that way without inverting the spline in the shader. It samples by mesh UV instead. Recorded
  against R5 below and in `EDITOR_SETUP.md`.

Not superseded, explicitly: the per-contact paint cost and the C0 batching result, the RGBA8
precision floor, the additive-versus-translucent decision, `k = 3.0`, and the C0 flush-deferral
guard. All of those stand.

### The API decision

Taken before implementation, not discovered during it.

The plugin takes a `USplineComponent*` centreline at setup. **The public write and query API keeps
taking world positions**, and the plugin converts internally.

- The brief's agnosticism requirement is about *vehicle systems*. `USplineComponent` is core
  Engine, not Chaos Vehicles, so depending on it does not breach that boundary.
- World-space signatures mean phases D, E and F do not churn when the storage shape changes again.
- The conversion is centralised in one place where it can be cached, rather than duplicated in
  every caller.

A **track-space overload** is added alongside it, taking distance along the centreline and lateral
offset directly, for callers that already hold the coordinate. `ASplineFollowingCar` is switched to
it: the car already tracks exactly those two values, so the harness skips the spline search
entirely, and having both paths in use keeps both tested.

**The plugin does not depend on `ARacetrackCircuit`.** That is host code. The dependency is on a
spline component and a track width, both passed in at configure time.

### The unfolded limit, and designing for folding without building it

Maximum 2D texture dimension is 16384. At 9.77 cm per texel that is 1600 m of centreline. So:

| Track length | Along-track texels | Fits unfolded |
|---|---|---|
| 309 m, the current harness | 3166 | yes |
| 1500 m | 15360 | yes, just |
| 1600 m | 16384 | at the limit |
| 7004 m, real Spa | 71689 | no — needs 5 folded rows, about 101 MB |

**This phase implements the unfolded case only.** The ribbon type is designed so folding can be
added later — the along-track axis is a separate quantity from the buffer's X extent, and stroke
construction already emits a variable number of quads per contact — but folding is not built now.

The reason for the split is that folding brings row-boundary seams, and a stroke crossing a
boundary has to be drawn twice or it leaves a gap in the racing line. That is a defect that hides
until phase E, when the line is supposed to be continuous, and it should not be introduced on
speculation.

### The lap seam is the folding problem, arriving one phase early

A closed loop already has one seam: distance wraps at zero. A contact straddling it has its start
near the far end of the buffer and its end near the near end, and a single quad between them
paints a band across the entire ribbon. Five cars at 30 m/s on the 309 m harness circuit cross it
about once every ten seconds each, so this is not a corner case.

Stroke construction therefore splits a seam-crossing contact into two quads from the start. That
is the same machinery folding needs later, built now because the unfolded closed-loop case is
simply wrong without it.

### The curved-stroke approximation, and where it breaks

A straight world-space stroke is a *curved* stroke in unrolled track space, and it is drawn as a
straight quad. The error is the sagitta of the arc, about `s² / (8R)` for a stroke of length `s`
through a corner of radius `R`.

| Case | Stroke length | Corner radius | Sagitta | In texels |
|---|---|---|---|---|
| 30 m/s at 60 Hz | 50 cm | 30 m | 0.10 cm | 0.01 |
| 30 m/s at 30 Hz | 100 cm | 30 m | 0.42 cm | 0.04 |
| C4 at 20x compression, 30 Hz | 2000 cm | 30 m | 167 cm | **17** |

Real time is negligible. **C4's time compression is what breaks it**, and badly: a 17-texel bow on
a mark about 2 texels wide is not an approximation, it is a different shape.

The bound is `s <= sqrt(8 * R * CentimetresPerTexel)`. At 9.77 cm per texel that is 484 cm on a
30 m radius corner and 342 cm on a 15 m hairpin. The remedy when C4 needs it is to subdivide a
long stroke into segments under that bound, which is free on the write path because C0 made cost
flat in stroke count. **Recorded here rather than discovered in C4. Not implemented now.**

### Tasks

| # | Task | Exit |
|---|---|---|
| R1 | `FSurfaceTrackRibbon` and the world to (distance, offset) mapping, with headless tests | Pure math, no rendering, no assets. A point on the centreline maps to offset 0; offset sign is consistent around both left and right hand corners; distance wraps on a closed loop; a point beyond the track edge is reported out-of-bounds rather than clamped |
| R2 | Measure the conversion cost at 4 and at 80 contacts per frame | Number in `MEASUREMENTS.md`. **Bail-out gate, see below** — passed, re-taken on mains 2026-09-23 |
| R3 | Buffer domain and paint path moved to track space, including the lap-seam split | Contacts land on the ribbon, and no stroke is smeared across the buffer at the seam — **met 2026-09-23** |
| R4 | Read the painted stroke width back from the buffer | A texel profile, settling whether D1's "narrow" requirement is already violated — **met 2026-09-23** |
| R5 | Receiving material samples by mesh UV, and a road carrying track-space UVs | Rubber appears on the road where the cars drive — **met 2026-09-23** |
| R6 | Re-take the real-scene measurement and compare against the five-car baseline | Numbers in `MEASUREMENTS.md` against median 0.0460 ms, p99 0.4450 ms, 1346 samples — **met 2026-09-23** |

**Status 2026-09-23: PHASE R COMPLETE. R1 to R6 all met.**

**R5 met.** `MF_SampleRacetrackSurface` samples by mesh UV, and rubber appears on the road where
the cars drive. Captured in
[`r5-c3-rubber-on-road-five-cars-oval.png`](../../2627-Y3A-PR-243663/assets/media/r5-c3-rubber-on-road-five-cars-oval.png).
The five bands are separately identifiable, as C2 required, and the capture also shows C3 working:
traffic fans into distinct lines on the straights and converges into one band through both corners.

**The material edit was not the whole of R5.** With it done the road still rendered nothing,
because `GenerateRoadMesh` wound its triangles inside out and every face pointed at the ground.
Confirmed against `GenerateBoxMesh`'s top face in the engine rather than guessed, and fixed in
`dd588db`. **The mesh had been wrong since it was written, committed and measured against.** It
cost nothing numerically — the paint path never touches the mesh — but the only reason it survived
is that this phase had been carrying "nothing has been seen on screen" as an accepted gap. The
lesson is cheap to state and was expensive to learn: a render path that has never been looked at is
not verified, however many tests pass around it.

*Exit condition for the phase — "the same five cars lap and paint as they do today, on a ribbon
sized from the centreline" — is demonstrated, with the memory figures printed at runtime rather
than argued.*

**R6 met 2026-09-23, and it took the twenty-car measurement with it.** Twenty cars in the live
harness cost **0.0220 ms median against 0.0210 ms for five** — flat in stroke count, which is C0's
claim holding in a real scene rather than on a synthetic bench. Against the five-car baseline the
median halved and p90 improved eightfold, but most of that is the run being standalone `-game`
rather than Simulate-In-Editor; the paint pass did not get faster, the frame around it got quieter.
Full attribution and the surviving tail in [MEASUREMENTS.md](MEASUREMENTS.md).

**The measurement no longer needs the editor, Live Coding, or MCP.** `r.RacetrackSurface.TimePaint`
is `ECVF_Default`, so `-ExecCmds` sets it on a standalone `-game` command line. That replaces the
four-step temporary-source-edit recipe in `EDITOR_SETUP.md` and, unlike it, obeys this project's own
rule against measuring in the editor.

**R2 gate: passed.** Eighty contacts at the harness circuit's 18-point spline cost 0.5053 ms on the
worst of three runs, 3.03 % of a 60 Hz frame, comfortably inside any reading of the gate.

**The three runs spread by up to 4x, and the cause is that the laptop was on battery.** Cost is
linear in spline points and flat in contact count in all three, so the shape is solid; the constant
is not. The 1 ms crossover at eighty contacts lands between 64 and 256 spline points depending on
the run, and a real circuit has 70 to 256 points depending on authoring density — so the crossover
and the target overlap, and which side they fall on is not yet known. **The per-contact segment
cache is needed somewhere between this harness and a real circuit, is not needed for this harness,
and a mains-power re-take is what turns that into a number.** Not implemented.

A method rule follows for the rest of the project: **record the power state with every
measurement.** No entry in `MEASUREMENTS.md` before this one does.

### R2 re-taken on mains, 2026-09-23. The constant is now reliable and the question is answered.

Six headless runs on mains, three with the test unchanged and three after adding spline-point
candidates 128, 192 and 384. 36 of 36 tests pass in all six. Numbers in
[MEASUREMENTS.md](MEASUREMENTS.md).

- **The spread is gone.** Three mains runs agree to within 5 % where the battery runs spread by
  2.42x. Harness complexity at 80 contacts is **0.1383 ms, 0.83 % of a 60 Hz frame**, against the
  0.5053 ms recorded above.
- **The crossover is measured, not interpolated.** The test now computes and prints it: 238, 233
  and 230 spline points across the three runs, so **about 234**. A 7 km circuit at one point every
  100 m costs 0.33 ms; at one point every 27 m it costs 1.09 ms. The sparse end of the plausible
  range clears by a factor of three, the dense end crosses.
- **The per-contact segment cache stays unimplemented, now with a written trigger.** Implement it
  when a circuit exceeds about 230 centreline spline points, or when any world-space caller reaches
  80 contacts per frame in a real scene. Neither holds: `ASplineFollowingCar` uses the track-space
  overload and performs **zero** spline searches per frame, and repo-wide the world-space overload
  is reached only from the Blueprint wrapper and three automation tests.
- **The power-state caveat over the whole measurement file is narrowed by measurement.** The full
  suite re-ran on mains, so the paint and decay passes were re-taken for free. They did not move.
  Battery cost the game-thread search 2 to 3.7x and cost the GPU-timestamped passes nothing, which
  is a mechanism rather than a coincidence. **The pre-2026-09-23 GPU figures did not need a
  re-take; only the CPU wall-clock ones did.** The five-car real-scene entry is the one still open,
  and R6 re-takes it.
- **A conclusion recorded on 2026-09-22 was reversed.** The earlier entry called the editor run
  optimistic and the headless runs "the numbers of record". The editor run matches mains to within
  5 % at every complexity, so it was the only full-clock run of the three and the headless runs
  were the throttled ones.

**R4 closed a carried unknown and opened a smaller one.** A 20 cm contact patch stores exactly
19.53 cm — 2 texels at the production density, 8 at four times it. D1's "narrow" requirement was
never violated. The readback also showed every inked texel at exactly 1.0000 with no falloff at
all: the additive blend state is declared `CW_RGB`, so the brush's alpha channel takes no part in
either the blend factors or the written channels, and `BuildRadialBandTexels`' smoothstep is
discarded entirely on the production path. **The paint path draws hard-edged rectangles and always
has.** That is what D1 wants, so nothing is fixed now, but D2 and E4 start from a hard edge rather
than the soft one the brush implies.

Numbers for both in [MEASUREMENTS.md](MEASUREMENTS.md).

*Exit: the same five cars lap and paint as they do today, on a ribbon sized from the centreline
rather than a square world region, with the memory figures above demonstrated rather than argued.*

**R2 is a deliberate bail-out.** World to track-space conversion is a cost the old design did not
have: `FindInputKeyClosestToWorldLocation` is a search, and eighty of them per frame may not be
free. If it is expensive the fix is to cache the last known key per contact and search locally,
but that is an optimisation and it is not taken before the measurement says it is needed. If R2
looks bad the phase stops there and the result is reported rather than worked around.

### R4 and R5 carry two known traps

- **Deleting `FSurfaceRegion` while `BP_SurfaceRegionDisplay` holds one as an instance-editable
  member will break that Blueprint.** The Blueprint must lose the variable and the
  `ConfigureSurfaceRegion` call *before* the C++ type goes, not after.
- **Live Coding cannot introduce a symbol newly exported by one module and consumed by another.**
  This phase adds new plugin exports that the host module calls, so the new exports are batched
  into one change and one editor restart is expected. Recorded against the same rule in
  `EDITOR_SETUP.md`.

## Phase D — Skid marks at product quality

| # | Task | Exit |
|---|---|---|
| D1 | Velocity-oriented rotated brush quad, narrow | Marks align with travel direction |
| D2 | Immediate write: darken, drop roughness | Reads as a lockup mark |
| D3 | Defined behaviour at the ribbon edge | Defined, not accidental. Narrowed by phase R: a closed-loop ribbon has lateral edges only, and the distance axis wraps |
| D4 | Save, reload, reset | State survives a round trip |
| D5 | Behaviour under World Partition streaming | Correct across a streaming boundary. Phase R absorbed the region-follows-track half of this task; only streaming is left |
| D6 | First profiling pass | Numbers in `MEASUREMENTS.md` |

*Exit: correct under streaming, saving and reset, not only in a clean single level.*

## Phase E — Emergent racing line and the decay coupling

The clearest proof the mechanism works, and the reason to prioritise it over marbles.

| # | Task | Exit |
|---|---|---|
| E1 | Buildup channel, saturating accumulator | Approaches a ceiling rather than clipping |
| E2 | Decay pass over real time, frequency as a cost knob | Line fades over a session |
| E3 | Wash-out call for users to hook to rain | One call, measurable effect |
| E4 | Roughness and specular change with slight darkening, verified under Lumen and baked | Reads as laid rubber under both |
| E5 | **The coupling**: skid decay rate as a function of local buildup | Marks on the line are paved over, marks off it persist |
| E6 | Second profiling pass, focused on the decay pass | Numbers recorded |
| E7 | Validation: two identical marks, traffic over one only, compare intensity | A number, not an impression |

*Exit: a racing line that appears from being driven on, and a measured difference in skid
persistence on the line against off it.*

## Phase F — Query path

The half of the project that turns accumulation from an effect into a mechanic.

| # | Task | Exit |
|---|---|---|
| F1 | Gather compute pass sampling the targets at submitted positions | Averages a contact patch, not one texel |
| F2 | Readback ring with last-known-value cache | No stall, no stale-read crash |
| F3 | Local cache for an actor's own contacts (the 45 ms axle case) | Own contacts not subject to readback delay |
| F4 | Read API `QuerySurfaceState` against the buildup channel | Correct and defined values |
| F5 | Debug overlay drawing returned values beside the surface they came from | Wrong sample position is visible, not assumed absent |
| F6 | Measured latency in frames, and query cost at 4 and at 80 contact points | Tolerance argued in the proposal demonstrated |
| F7 | Survives level load, hitches, actor destruction | Decides readback vs grid |

*Fallback: the CPU grid, roughly an afternoon, public API unchanged. Taken only if F7 fails.*

*Exit: the query returns correct values and the debug overlay agrees with what is drawn.*

## Phase G — Marble distribution and marble query

**Pulled forward ahead of D, E and F on 2026-09-24, requested directly.** Recorded as a reorder
rather than done quietly.

Two things make it a better fit now than when it was written. **C10.2 built the channel viewer and
marbles is the one channel that is black in it** — the mask, the cycle and the caption all exist and
point at an empty buffer. And **G2 wants marble density driven by the buildup in adjacent cells,
which needed buildup to have a steady state.** Before C10.3 it had none: the line only ever got
darker, so "where is the line" depended on when you looked. It now settles at 0.9513 coverage at the
apex against 0.3074 on the straight.

**The cost, stated because it is the obvious attack.** Phase F is the half of the project the brief
cares most about — the gameplay query API is the deliverable, marbles are dressing — and the cut
list names phase G's marbles as one of the first things to go. Pulling G forward puts the least
defensible phase ahead of the most defensible one. That is the user's call, made with this written
down, and **F should be the phase after G rather than slipping further.**

**One dependency this reorder exposes.** G2 as written reads the buildup field to decide where
marbles go, and reading the field is phase F. Doing G first means either building a narrow read for
it or deriving placement from what the car already knows. That is the first decision G has to take
and it bears directly on the emergence claim, so it is not a detail.

*Resolved 2026-09-24 below: neither. The field read is rejected and the car's own position is used,
but not as a fallback — it is the better input, because the buffer is only an image of where traffic
ran and the car holds that fact exactly.*

### Tasks, revised 2026-09-24

The G1 to G4 above were written before C10 existed and before the ribbon was folded. Three of the
four change. They are rewritten rather than replaced, and what each one was is recorded against it.

| # | Task | Exit |
|---|---|---|
| G1 | Marble deposition on the car: one contact per car per tick, displaced outward, gated by cornering. *Was "cellular-noise clumping"; clumping moved to G5 by the D-C decision* | The B channel is non-zero after a lap, and zero on a lap with no corners |
| G2 | The displacement law: direction from the sign of local curvature, magnitude from a falling draw. *Was "density driven by buildup in adjacent cells"; superseded by D-A* | Deposits land on the opposite side to the car's own apex target, every time, on both layouts |
| G3 | Measure where the band lands across the track width, by readback | A texel profile across the ribbon showing a monotonic fall from the line outward, in `MEASUREMENTS.md` |
| G4 | Marbles visible in the channel view | The marbles view is no longer black. Free: the existing debug material already drives `Coverage.B` to emissive |
| G5 | Scattered dots in the host debug material, denser and individually bigger toward maximum | Dots, not a flat wash, with both count and radius rising with density. **Graph built and compiled 2026-09-24; not yet looked at** |
| G6 | Clearing when cars drive through | Repeated passes open a path. *Unblocked 2026-09-24, see below. Not started* |
| G7 | Channel wired into the query path | Per-wheel readout changes as a car crosses the band. **Belongs to F and moves there** |

*Exit: marbles accumulate off the racing line, denser nearer the line than at the edge, and the
distribution is a measured profile rather than an impression.*

### Decided: marbles are displaced deposits, not a field read (D-A)

**G2's original premise is rejected.** Reading the buildup in adjacent cells to decide where marbles
go is worse than the alternative on three counts, and only the first is about phase F not existing
yet.

1. **It cannot be done in the frame that needs it.** The decision has to be taken on the game thread
   at contact-registration time. The buildup lives in the render target the same pass is about to
   write. Sampling a target inside the draw that writes it is a read/write hazard, not merely
   expensive, and the alternative — `ReadRenderTargetRawPixelArea` plus `FlushRenderingCommands`,
   which is what the test suite uses — is a full game-thread stall on a 117 MB target.
2. **It is a proxy for something already in hand.** Buildup in adjacent cells is an estimate of
   *where traffic ran*. `ASplineFollowingCar` knows where it ran exactly, this tick, to double
   precision, for free. Reading it back off the GPU recovers a blurred, quantised, one-frame-late
   copy of a number the caller is holding.
3. **It would not have strengthened the emergence claim, which is the only reason it was in the
   plan.** Deriving marble position from the buffer sounds more emergent than deriving it from the
   car. It is not: the buffer's contents are themselves nothing but the accumulated record of where
   those same cars drove. The claim would be the same claim with a GPU round trip in the middle.

**What is decided instead.** A car deposits marbles at its own track-space position, displaced
laterally away from the apex it is turning toward. Nothing stores or looks up a line.

**The honest claim, and it is narrower than it first looks.** Borrowing the framing C3 and C6
already set:

> The line the marbles gather around is not chosen by any number in this code. How far they scatter
> from it is.

Unpacked, because the oral assessment will not accept the one-liner on its own:

- **Emergent:** *where the band is*, around the lap and across the width. The deposit is anchored to
  `LateralOffsetAlpha`, which is C3's and C6's result — a per-driver line, a triangular corner
  deviation, a bounded lateral rate. Change the field and the band moves. Two circuits with
  identical corners but different traffic get different bands.
- **Not emergent, and must not be claimed as such:** *the shape of the falloff*. The displacement is
  drawn from an authored distribution, and by the law of large numbers the accumulated profile
  converges deterministically to exactly that distribution. It is a per-texel formula realised by
  sampling rather than written in closed form. Calling it emergent because it is stochastic would be
  the overclaim C6's honesty section exists to prevent — C6's racing line emerges from *diversity
  across drivers*, which is a different mechanism from *resampling one fixed curve*.
- **Also not emergent:** the side. It is the sign of the local curvature.

**Rejected: deriving the side from geometry independently.** The deposit uses
`-FMath::Sign(SignedCornering)`, which is the negation of the exact term
`ASplineFollowingCar::Tick` already uses to build `ApexTarget` (`SplineFollowingCar.cpp:266`). That
matters for a reason beyond brevity: **the geometric meaning of a positive `SignedCornering` is
asserted nowhere in the source** — no comment, no log string, no test. Any independent derivation of
"which side is outside" would rest on an unverified handedness assumption about
`USplineComponent::GetRightVectorAtDistanceAlongSpline`. Mirroring the car's own apex term is
correct *whatever* the convention turns out to be, because the apex is the inside by construction.
This is the one place in G where the cheap option is also the rigorous one.

### Decided: the write is the existing track-space contact at a displaced offset (D-B)

No new plugin API. `RegisterSurfaceContactInTrackSpace` with `Channel = EAccumulationChannel::Marbles`.

**Track space rather than world space, which reverses C8's choice for this one call.** C8 decided
the wheels register through the world-space boundary and that decision stands unchanged for the
wheels. It does not carry to the marble deposit:

| C8's reason for world space | Whether it applies to the marble deposit |
|---|---|
| Exercises the engine-agnostic boundary the brief is about | No. The four wheel contacts already exercise it 80 times a frame. A fifth call exercises nothing new |
| Offtracking falls out of rigid-body placement | No. A displaced debris position is not a rigid-body point |
| Wheel velocity from the previous world position is exact and free | No. The deposit's along-track rate is `SpeedCentimetresPerSecond`, which the car holds exactly |
| Cost: one spline search per contact | Yes, and it is the deciding term. R2's written trigger — *"when any world-space caller reaches 80 contacts per frame in a real scene"* — is already tripped at exactly 80. Twenty more searches per frame buys nothing |

Phase R's own stated reason for keeping both overloads was *"having both paths in use keeps both
tested"*. That still holds with the wheels on one and the deposit on the other.

**One deposit per car, not one per wheel.** The four wheels are 80 cm apart, which is 25.6 texels.
The scatter is 200 cm wide, which is 64. Four deposits would be four samples of the same
distribution at origins that differ by less than half its width — that is a deposition-rate
multiplier wearing a physical costume, and the deposition rate is already a console variable. It
would also double the frame's contact count from 80 to 160.

**`LateralOffsetAlphaPerSecond` is set to zero on the deposit, and this is not a detail.**
`BuildStrokes` back-derives a stroke's start alpha from that rate (`SurfaceAccumulationSubsystem.cpp:459`).
The displacement is redrawn every tick, so the *implied* lateral rate between consecutive deposits is
enormous — a 0.5 alpha jump in 10 ms is 50 alpha per second — and the stroke would be painted as a
lateral smear from the previous draw to the new one, filling the whole band solid every tick instead
of scattering within it. Zeroing it says the correct thing physically too: debris lands where it
lands, it does not sweep sideways as it settles.

**Rejected: clamping the displaced offset to keep it on the track.** Two of the three designs
considered during planning clamped the offset into the remaining room before the track edge. That
is wrong for this brief specifically: clamping piles every out-of-range draw onto the boundary texel
and produces **a stripe at the kerb**, which is the one outcome the goal rules out by name. A
deposit whose displaced alpha leaves `[-1, 1]` is instead discarded by the plugin's existing
`ContainsLateralOffset` check and counted in `ContactsDiscardedOutsideRibbon`. That is physically
right — debris that goes past the edge is on the run-off, not on the track — and it contributes to
the falloff instead of fighting it.

### Decided: smooth density in the buffer, granularity at read time (D-C)

The buffer stores a scalar density per texel. Clumps are produced by the material when it is read.

1. **The brief's deliverable is a query API a vehicle reads.** F1 already averages a contact patch
   rather than one texel. Stored clumps would make `QuerySurfaceState` return a value depending on
   whether the sample fell on a clump or between two, and F1's patch average would then filter that
   noise straight back out. The clumps would cost storage and deliver nothing to the only consumer
   the brief cares about.
2. **Decay is multiplicative over the whole target, and that is the wrong operator on clumps.**
   Stored clumps decaying get *dimmer*; they do not get *fewer*. Debris being swept away should
   thin out, not fade in place. A smooth density decaying is exactly right.
3. **Writing clumps needs an onset threshold, which the paint path cannot express.** Same family as
   D7's max write.
4. **Presentation-side clumping is resolution-independent.** It can make dots both more numerous and
   individually larger, which stored clumps cannot do above one clump per 3.125 cm texel.

**The honest limit:** this is a product decision, not a correctness one. A host that wanted the
query to return clump geometry rather than clump density would have to store clumps, and this design
forecloses that. It is the right trade for *this* brief and would be the wrong one for a brief about
debris physics.

### Decided: the dots go in the host debug material (D-D)

C10.2's reasoning holds unchanged and the test it set is worth restating: *is this parameter
meaningful to a host that is not this demo?* A scattered-blue-dot renderer is not. It is a debug
read of a density, and a shipping host would render marbles with H1's parallax-occlusion material,
not with dots. So the dots go in `/Game/Materials/M_SurfaceAccumulationDebug` and
`MF_SampleRacetrackSurface` is not touched, which also keeps clear of the asset where
`MaterialTools.delete_expression` is known to halt the editor.

**The node design, with the engine behaviour checked rather than assumed.**

| Need | Node | Why |
|---|---|---|
| A stable per-cell random number to decide whether a cell carries a dot at all | `VectorNoise` with `VNF_CellnoiseALU` | Documented `MaterialExpressionVectorNoise.h:15-19` as *"Random color for each unit cell in 3D space. RGB output range 0 to 1"*. This is the only one of the five that is a normalised per-cell random |
| A dot shape whose radius is controllable | `VectorNoise` with `VNF_VoronoiALU`, **alpha output** | Documented `MaterialExpressionVectorNoise.h:40-46`: *"A = distance to closest point"*. Distance-to-seed is a radial field, so thresholding it is a disc |
| Cells anchored to the road, isotropic, correct across the fold | `AbsoluteWorldPosition` as the noise `Position` | The ribbon UV is anisotropic — U spans 450 m per row, V spans 8 m — so feeding UV gives ellipses, and it would need `CentimetresPerTexel` published to the collection. World position needs nothing published and is round by construction |

Dot presence is `CellNoise.R < Density` and dot radius rises with `Density`, so both the count and
the individual size increase toward maximum, which is the two-axis behaviour the goal asks for.

**Recorded because one of the three planning designs got it wrong:** `VNF_VoronoiALU`'s **RGB** is
the *position* of the cell's seed point, not a per-cell random value. Comparing it against a coverage
scalar does not give a stable per-cell threshold. The RGB/alpha split is the whole reason two noise
taps are needed rather than one.

### Decided: one decay rate in G, and per-channel rates turn out to be free (D-E)

**G lives with the shared half-life.** Not because per-channel decay is hard — it is not, see below —
but because G does not need it. With deposition under G's control, a faster marble half-life and a
higher marble deposition rate reach the *same steady state*; they differ only in the transient.
Building a per-channel rate here would be constructing E5's machinery under G's name and would
preempt the decision E5 has to take on its own terms.

**The finding that changes E5's cost, recorded here because it was measured here.**
`SE_BLEND_Modulate` is `TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>`
(`BatchedElements.cpp:490`), so `Dst = Src * Dst` **per channel**, RGB only, alpha untouched.
`ApplyDecay` already draws exactly this as a full-target quad with colour
`(Retention, Retention, Retention, 1)`. Changing that one `FLinearColor` to
`(RetentionR, RetentionG, RetentionB, 1)` gives three independent half-lives **in the pass that
already runs, at no extra cost**. E5 was priced assuming this needed either three passes or a new
render path. It needs neither.

**The honest limit, which is about the physics and not the plumbing.** Time-decay is standing in for
sweeping, and it is the wrong mechanism for it. Marbles are removed by cars driving through them,
not by elapsed time, and a uniform decay removes marbles from the empty outside of the track at
exactly the same rate as from where cars actually run — which is backwards. The right removal is G6,
and until G6 exists the marble field's equilibrium should be described as *what a uniform fade
produces*, never as *what sweeping produces*.

### G6 is unblocked, against what this plan previously recorded

The trap carried into this phase said clearing cannot be done because the canvas offers no negative
write. That is true of *additive* and it is false in general. `SE_BLEND_Modulate` above is
per-channel, so a small quad drawn over the wheel footprint with colour `(1, 1, k)` multiplies the
marble channel by `k` in that footprint and **leaves rubber and skid exactly as they were**. The
proof that the path works is not an argument — `ApplyDecay` is that draw, at full-target size, and
it is measured at 0.3340 ms.

This does not rescue D7. D7 needs `max`, which `Src * Dst` cannot express. It does suggest a route
D7 has not considered: modulate-to-zero followed by an additive write is *assignment* on a single
channel, which is D7's "refresh rather than accumulate" without a max blend. Two batches, not two
draws per stroke. **Recorded against D7, not acted on here.**

**G6 is nonetheless not implemented in this pass**, and the reason is scope rather than difficulty:
the request was for marbles that accumulate off the line and can be seen. Adding a sweep changes the
steady state that G3 is about to measure, so doing both at once confounds the only measurement the
phase has. It is the next task in the phase and it is now cheap.

### The arithmetic, predicted before the run

Stated in advance so the G3 readback either confirms it or contradicts it. Demo layout defaults:
180000 cm lap, 800 cm wide, 3.125 cm per texel, 20 cars. One alpha unit is 400 cm, which is 128
texels. `ContactWidth` is 20 cm, which is 6.4 texels.

- **Throw distance `MarbleThrowCentimetres` = 200.0**, which is 0.5 alpha and 64 texels. A quarter of
  the track width: wide enough to read as a band rather than a line, narrow enough to leave the
  inside of the corner clean.
- **Displacement draw is `MarbleThrowCentimetres * min(u1, u2)`.** The minimum of two uniforms has
  density `2(1 - x)`, a linear fall from the line to zero at full throw. This is C6's two-draw idiom
  with the mode moved to the near end — C6 uses `u1 + u2 - 1` for a mode at the centre. Mean throw
  66.7 cm, median 58.6 cm.
- **Deposit exposure.** `MarbleSlip = RollingSlip * CorneringAmount`, through the existing
  `SlipToExposure`. The second factor is what makes a straight deposit nothing, and it is required
  rather than cosmetic: `FMath::Sign(0)` returns `0` (`GenericPlatformMath.h:937-940`), so on a
  straight the displacement direction collapses and an ungated deposit would land **on the racing
  line**. In a full corner `RollingSlip` is `CorneringSlipAmount` = 0.50, so exposure is
  `0.50 * 0.002 * Scale` = `0.001 * Scale`.
- **Per-lap gain at the near edge of the band.** Each along-track texel column is crossed once per
  car per lap. A 6.4-texel stroke covers a texel at displacement `x` with probability
  `(W/D) * 2(1 - x/D)`; at `W/D = 0.1` that is 0.2 at the line and 0 at full throw. Twenty cars give
  `20 * 0.2 * 0.001 * Scale` = **0.004 exposure per lap** at the near edge.
- **Against the measured rubber rate.** The apex settles at 0.9513 coverage, which is exposure
  1.0075, at a 56-lap half-life — so rubber's gain at the apex is `1.0075 * (1 - 2^(-1/56))` =
  **0.01238 exposure per lap**. Marbles therefore build at **32 % of the rubber rate at the apex**.
- **Predicted marble equilibrium** at the same half-life: `0.004 / 0.01229` = 0.3255 exposure, which
  is **0.623 coverage** at the near edge of the band, falling linearly in exposure to zero at
  200 cm. That sits between the apex's 0.9513 and the straight's 0.3074, so the band should be
  plainly visible in the all-channels view and not merely in the isolated one.

**Every figure above is a prediction.** The one that is most likely to be wrong is the per-lap
coverage probability, because it assumes deposits tile the lap exactly once per car and ignores that
`CorneringAmount` is graded on Spa rather than binary as it is on the Stadium. G3 measures the
profile and the entry records the disagreement rather than quietly re-deriving the prediction.

### Traps found while planning, recorded before they cost anything

- **`AppendStroke` does not clamp lateral offset, and an out-of-range alpha lands in the wrong
  atlas row.** `NormalisedAcrossTrack(-1.5)` is `-0.25`, and `RibbonToAtlasInRow` maps that to
  `(Row - 0.25) / Rows`, which is inside the *previous* row's band. Today nothing reaches it because
  `RegisterSurfaceContactInTrackSpace` rejects `|alpha| > 1` on entry and the cars' lateral rate is
  small. The marble deposit keeps it unreachable by zeroing `LateralOffsetAlphaPerSecond`, so start
  and end alpha are equal and the entry check covers both. **Pre-existing, not introduced by G, and
  not fixed by G.**
- **Gating deposition on `bCommittedToACorner` would leak marbles onto the straight.**
  `UpdateCornerCommitment` (`SplineFollowingCar.cpp:109-134`) holds the flag latched for
  `SameCornerSeparationCentimetres` = 600 cm *after* cornering has stopped. The gate is live
  `CorneringAmount`, which has no latch.
- **Live Coding cannot take the new `UPROPERTY`.** `MarbleThrowCentimetres` is new reflection, so the
  editor must be closed and `Build.bat` run for real.

### Tests

| Test | Asserts | Built |
|---|---|---|
| `RubberAccumDemo.Car.MarblesGatherOutsideTheLineAndThinTowardTheEdge` | Every deposit lands away from the apex; the throw is bounded by its own parameter; the mean is a third of the maximum; the histogram falls monotonically; and the mean deposit sits outside the mean car line. Written against the car's own apex term rather than a handedness assumption, so it catches a sign error without depending on one | yes |
| `RacetrackSurface.Accumulator.MarbleStrokeWritesOnlyTheMarbleChannel` | A marbles stroke inks B and leaves R and G at zero, and a rubber stroke in the same buffer inks R and leaves B at zero | yes |
| `RubberAccumDemo.Car.RegistersMoreSlipInACornerThanOnAStraight` | Extended: a straight sheds no marbles, a corner sheds exactly one per car rather than one per wheel | extended |
| `RubberAccumDemo.Track.NoWheelFallsOffTheRibbon` | Extended: counts wheel discards separately from marble discards, since marbles are thrown outward and are meant to be able to leave the track | extended |
| `RubberAccumDemo.Track.MarbleBandProfileAcrossTheTrack` | The band read back out of the buffer across the track width | **no, see below** |

The three planned as separate side, straight and distribution tests collapsed into one, because they
share a sampling loop and splitting them would have ticked the same 4000 frames three times.

**The band readback test was not built, and that is a recorded gap rather than an oversight.** It
needs the full field ticked through the subsystem for several laps against a live render target, and
what it would assert — that the band falls away from the line — is what the deposit measurement
already establishes to 0.4 % of prediction. The composition it would additionally cover, lateral
offset through ribbon to folded atlas, is covered by `SurfaceFoldedRibbonTests`. The honest position
is that *this* band has not been read back from *this* buffer.

### Measured on implementing, 2026-09-24. One prediction was wrong and the fix was already in the file.

**G1 to G4 are met. 76 of 76 tests pass headlessly with the editor closed, both build paths clean.**
Numbers in [MEASUREMENTS.md](MEASUREMENTS.md).

**The distribution came out as derived.** Mean throw 66.40 cm against a predicted 66.67, widest
198.40 against a 200 cm bound, and the five bins fall monotonically at 1029, 916, 570, 342, 109
against a predicted 1068, 830, 593, 356, 119. The band sits **0.166 alpha, 66.4 cm, 21.2 texels**
outside the line the car itself drives.

**The prediction that was wrong: multiplying slip by the cornering amount is not a gate.** The plan
argued that because `FMath::Sign(0)` is `0`, the deposit had to vanish on a straight, and that
scaling the exposure by `CorneringAmount` would do it. The first run disagreed — 34 deposits landed
on the apex side and a straight registered five contacts instead of four. The cause is that C3's
amendment made the cornering profile **sampled and normalised** rather than analytic, so on a
straight it is small but not exactly zero, and **the sign of a value that is numerically noise is
arbitrary**. Scaling the exposure made those deposits harmless in intensity while leaving them on a
random side of the car.

The fix adds no tunable: the gate is `CorneringHere >= CornerExitCorneringAmount`, the threshold
`ASplineFollowingCar` already owns for deciding it has left a corner. A gate derived instead from
`HalfFloatGuaranteedChangeThreshold` was considered and rejected — it would have made *where marbles
deposit* depend on `r.RacetrackSurface.DepositionScale`, so turning the deposition up would have
silently widened the band into gentler curves.

**A consequence worth recording for D7 and E5:** this is the second time in two phases that a
value which is analytic on the Stadium and sampled on Spa has behaved differently at its zero
crossing. The Stadium is the oracle for the general implementation, as C3 argued, but it is a poor
oracle for *numerical* behaviour near a boundary, because on the Stadium the boundary is exact.

**One decision was taken during implementation and is not in the plan above.** The plugin's single
`ContactsDiscardedOutsideRibbon` counter became one counter per channel. It had to: marbles are
thrown outward and are *meant* to be able to leave the track, so a shared counter can no longer
distinguish *a bug threw a wheel off the ribbon* from *debris correctly left the track*, and
`NoWheelFallsOffTheRibbon` asserts the former is zero. At the default 200 cm throw nothing is
actually discarded, so the test would have passed either way — which is precisely why it was worth
fixing rather than leaving as a trap for whoever next raises the throw.

### G5 landed the same day, once the editor came up

Nine expressions added to `/Game/Materials/M_SurfaceAccumulationDebug`, one connection rewired,
nothing deleted, `MF_SampleRacetrackSurface` untouched. Node list, wiring and the Custom node's
HLSL verbatim in [EDITOR_SETUP.md](EDITOR_SETUP.md).

**The design collapsed to one threshold rather than two.** The plan called for a presence test
(`CellNoise.R < Density`) multiplied by a size term. Both fall out of a single expression instead:
`Radius = saturate(Density * 2 - CellRandom.r) * MaxRadius`. A cell with a low random draw crosses
zero first, so at low density few cells carry a dot; as density rises, more cells cross zero *and*
every radius already past zero grows. Count and size both rise, from one line.

**The plan's node table was wrong about `VNF_VoronoiALU` in a way worth recording.** It is right
that the node gives a dot primitive, but the distance is in the **alpha** channel and RGB is the
seed *position*. One of the three planning designs proposed comparing the R channel against a
coverage scalar, which would not have produced a stable per-cell threshold. Both facts are in the
engine header at `MaterialExpressionVectorNoise.h:15-19` and `:40-46`.

**Not verified: nobody has looked at it.** The graph compiles and the wiring was read back and
confirmed node by node, which establishes that it is wired as designed and not that it looks right.
`MarbleDotCellCentimetres` at 12.0 and `MarbleDotMaxRadius` at 0.42 are arithmetic, not judgement,
and both are instance parameters in the `Surface Debug` group so they are a slider rather than a
rebuild.

**One consequence for older captures.** `Multiply.A` was `Coverage` and is now
`AppendVector(Coverage.rg, MarbleDots)`. R and G are bit-identical, so every rubber and skid capture
stays comparable; the blue channel of the all-channels view is now a dot mask rather than a smooth
coverage, and is not.

### Not in this phase
- **G6**, per the section above.
- **G7**, which moves to phase F where the query path is built.
- **Per-channel decay**, which moves to E5 with its cost corrected downward.
- **The per-contact segment cache.** G adds no world-space contacts, so R2's trigger is not moved.

## Phase H — Marble volume, hardening, demo map

| # | Task | Exit |
|---|---|---|
| H1 | POM driven by the marble height mask | Convincing volume at close range |
| H2 | Step count as a scalability setting, distance fade to flat | Degrades gracefully |
| H3 | Scalability tiers wired to the standard system | Standard console variables work |
| H4 | Final profiling pass | Numbers recorded |
| H5 | Demo map: material and lighting swaps, a recognisable car, a drivable pawn | Fast-forward control prominent |

## Phase I — Communication and release

Reserved. No new features.

- Final presentation: five minutes, four slides, live demonstration
- Public technical write-up
- Fab listing, clean install verified into a fresh project
- Fixed-camera timelapse of one braking zone evolving

---

## C6 — A shared racing line with per-corner variation

Added 2026-09-23, outside the original phase C scope. Requested directly rather than planned, for
the same reason the Spa layout was: what the buildup *looks like* is the thing the project is
judged on, and the current traffic model produces the wrong shape for it.

### What is wrong with the shape C3 produces

C3 spreads cars across the track by baseline offset and converges them toward a single apex value
in corners. That is backwards from a real field. Cars fan out **on the straights**, where a real
field runs nose to tail on one line, and they converge to the **same** apex, where a real field
takes visibly different arcs. The rubber that falls out of it is a wide band on the straights and a
narrow one at the apex.

What is wanted is the inverse: one dominant line everywhere, with the spread appearing **in the
corners**, so the racing line is unmistakable and the variation around it is visible as separate
arcs rather than as a smear.

### The design

Three changes, none of them large.

1. **Collapse the baseline spread, to a number rather than a judgement.** `DriverLateralSpread`
   shrinks so the field runs close to a single line on the straights. It is not removed — see the
   emergence note below. "Close to a single line" is checkable, because on a straight
   `LateralOffsetAlpha` settles exactly to each car's own baseline, so the field paints static
   evenly spaced stripes whose spacing is

   ```
   AdjacentSpacing = (2 * DriverLateralSpread / (CarCount - 1)) * TrackWidth * 0.5
   ```

   At today's 0.4 spread, twenty cars and a 1200 cm track that is **25.26 cm**, which is wider than
   the 20 cm `ContactWidth` — so the stripes do not touch, and that gap is the whole reason a
   straight texel is crossed by one car per lap today rather than twenty.

   **The target is `AdjacentSpacing <= ContactWidth / 4`**, i.e. four-fold overlap rather than
   bare contact, which at the current field size and track width means
   **`DriverLateralSpread` of about 0.08** (5.05 cm spacing, a merged band roughly 116 cm wide on
   a 12 m track). Bare contact at exactly `ContactWidth` would leave visible scalloping where
   neighbouring stripes meet; a quarter of it is comfortably inside that. Expressed as the formula
   rather than as the constant so it survives a change to `CarCount` or `TrackWidth`.
2. **Seeded per-car randomness.** Each `ASplineFollowingCar` owns an `FRandomStream` seeded from
   a circuit-level seed plus its own grid index. This is not optional: automation tests and every
   figure in `MEASUREMENTS.md` depend on a run being reproducible, and an unseeded `FMath::Rand`
   would make the harness non-deterministic and quietly invalidate the C3 tests.
3. **One draw per corner, not per frame.** On a rising edge of `CorneringAmount` through an entry
   threshold, the car draws a deviation `D` and holds it for the whole corner:

   ```
   ApexTarget = (ApexLateralOffsetAlpha + D) * Sign(SignedCornering)
   ```

   `D` is drawn from a **triangular** distribution — the sum of two uniform draws, halved — rather
   than a uniform one. That is the whole trick for the requested look: a triangular distribution
   has a mode at the apex value, so most laps land on or near the same line and the outliers are
   genuinely outliers. A uniform draw would give an evenly smeared band with no dominant line in
   it.

   **A starting magnitude, to be tuned by eye rather than defended as derived:**
   `MaxCornerDeviationAlpha` of **0.20** against the existing `ApexLateralOffsetAlpha` of 0.7, so
   the apex target ranges over 0.5 to 0.9 and the corner band spans about 240 cm against the
   straight's 116 cm — roughly twice as wide, which is the visible difference the change is for.
   0.20 is chosen so that `0.7 + 0.20` lands exactly on a **±0.90 clamp**: the outer bound is
   reached rather than clipped, so no probability mass piles up against the limit and turns the
   tail of the distribution into a hard edge. Marks are 20 cm, which is 0.033 in alpha, so arcs
   need to differ by more than that to be separable at all — 0.20 gives roughly six resolvable
   lanes either side of the mode.

Per-frame noise is explicitly not the design. A value re-drawn every tick is jitter: the car
wobbles and the deposit averages back to the mean, leaving one blurred band. Holding `D` for the
corner is what makes a *different arc* rather than a noisy one.

### The emergence problem, which is the real decision here

The project's central claim is that the line is not authored. C3's defence is that it authors the
policy, not the path: no number anywhere is "the racing line", and the band is where independent
drivers happen to converge.

**Collapsing the baseline spread to zero would break that defence.** Every car would target the
same authored `ApexLateralOffsetAlpha`, and the line would literally be a constant in the header
with noise around it. The band would be decoration on an authored path, and the honest description
would have to change from *emergent* to *variation around a line I chose*.

So the spread shrinks rather than vanishing, and `CorneringLineCommitment` keeps varying per
driver. Cars still differ **systematically**, not only randomly, and the dominant line is still
where independent policies converge — it is just a much tighter convergence than before. Setting
`DriverLateralSpread` to 0 collapses this to the purely-authored version, which is a legitimate
thing to want for a screenshot and an illegitimate thing to describe as emergent. One parameter,
two claims, and the difference must be stated wherever the footage is used.

### Two interactions to expect

**The field-spacing drift gets more visible, not less.** Cars currently drift apart along the
track; the lateral spread hides it because they are on separate lines anyway. Put them on one line
and drifting cars close up on each other with nowhere to go, so the defect reads as cars
intersecting rather than as an uneven grid. This may promote the drift fix from *known defect* to
*prerequisite*.

**The saturation timescale changes shape, and by a derived amount.** Worked out from the constants
on 2026-09-23 rather than asserted. A pass deposits `slip * 0.002`, so an apex pass leaves 0.0010
and a straight pass 0.0002 — a fixed **5:1** that depends only on the two slip constants and can
never be changed by traffic. But today's contrast is **100:1**, because the other 20x is geometry:
at 25.26 cm stripe spacing against a 20 cm mark, a straight texel is crossed by *one* car per lap
while the apex texel takes all twenty.

| | exposure per texel per lap | laps to 63 % | laps to 95 % |
|---|---|---|---|
| Apex, today and after C6 | 0.0200 | 17 | 50 |
| Straight, today | 0.0002 | ~1667 | ~5000 |
| Straight, after C6 | 0.0040 | ~83 | ~250 |

C6 cancels the geometric term and leaves the slip term, so **contrast falls from about 100:1 to
about 5:1** and the straights speed up about twentyfold. That is the change working, not a side
effect of it: the total rubber laid on a straight per lap is unchanged at `20 * 0.0002`, and C6
only concentrates it into a twentieth of the area. Twenty separate thin stripes is the wrong shape;
one line is the point.

> **Corrected 2026-09-23 on implementing C6, by counting rather than deriving. Two of the three
> rows above are wrong, and the conclusion survives for the wrong reason.** The straight-after-C6
> row assumes all twenty cars cross one straight texel. They do not: the whole point of choosing
> `AdjacentSpacing <= ContactWidth / 4` is *four*-fold overlap, so about four cars cross a given
> straight texel, not twenty. The apex row's one-texel assumption fails in the other direction,
> because the `MaxCornerDeviationAlpha` draw deliberately spreads the corner arcs over more texels
> than the straight band occupies. `RubberAccumDemo.Track.CountedPassesOverATexelAgainstThePrediction`
> now counts real passes on the oval over 12.67 laps of a twenty-car field:
>
> | | texels inked | passes per texel per lap | exposure per lap | laps to 63 % |
> |---|---|---|---|---|
> | Busiest straight texel | 14 | 8.29 | 0.001658 | ~201 |
> | Busiest apex texel | 29 | 2.45 | 0.002447 | ~136 |
>
> **Measured contrast is 1.5:1, not 5:1 and not 100:1.** The apex is still the darker of the two,
> so a line does appear, but only just. The straight count of 8.29 is itself about twice the 4.22
> the static baseline spacing predicts, because on an oval this size the field has not finished
> lerping back to its baselines by the middle of the straight — residual convergence from the
> previous corner bunches it tighter than the grid does.
>
> The direction of the original argument holds: the geometric term collapses and the straights
> speed up by roughly an order of magnitude. The magnitudes do not. **`MaxCornerDeviationAlpha`
> now trades apex contrast against arc separability, and that trade is a by-eye call, not a
> derived one** — 0.20 buys a 283 cm corner band against a 137 cm straight band, at the cost of
> making the apex only 1.5 times darker per texel than the straight. Lowering it concentrates the
> apex and narrows the fan. This strengthens rather than weakens the phase E argument below.

> **Re-counted 2026-09-23 after [C8](#c8--four-tyre-contacts-per-car-and-the-circuit-rescaled-to-resolve-them),
> which moved all three terms at once.** Four wheels per car, an 800 cm track and a 3.125 cm texel
> change the geometry the counts rest on, so
> `RubberAccumDemo.Track.CountedPassesOverATexelAgainstThePrediction` was re-run over the same
> 12.67 laps of a twenty-car field, now counting each wheel rather than each car.
>
> | | texels inked | span | passes per texel per lap | exposure per lap | laps to 63 % |
> |---|---|---|---|---|---|
> | Busiest straight texel | 57 | 178 cm | 21.39 | 0.004278 | ~78 |
> | Busiest apex texel | 90 | 281 cm | 12.08 | 0.012077 | ~28 |
>
> **Measured contrast is now 2.8 to 1, up from 1.5.** The apex is close to twice as legible as it
> was, which is a result C8 did not set out to get and should not be claimed as its purpose.
>
> Both rates roughly doubled, because halving the track halves `AdjacentSpacing` and packs the same
> field into half the width. The contrast improved on top of that because a car's four wheels
> straddle the corner band rather than sitting on one line, so the apex gains the front-and-rear
> pair where the straight only gains the same coincident pair it already had. The straight count of
> 21.39 is again about 1.7 times the 12.67 the static stripe spacing predicts, for the same
> residual-convergence reason recorded above, and that ratio is now asserted rather than noted.
>
> **The band widths grew in texels and shrank in centimetres**, which is the rescale working: the
> apex band is 90 texels against 29, and 281 cm against 283, so the same feature is resolved three
> times more finely without changing size on the track.

**What this exposes is a limit of having no decay.** The 5:1 is a ratio of *rates*, and coverage
runs through `1 - exp(-3E)`, which compresses everything near the top. When the apex reaches 95 %
at about 50 laps the straight is at about 45 % — clearly separable. By the time the straight
reaches 95 % at about 250 laps the apex is at 99.99 % and the two are indistinguishable. So
**without the phase E decay the racing line is a transient**, most legible somewhere between laps
20 and 100 and flattening towards a uniformly saturated track after that. This is an argument for
phase E being load-bearing rather than cosmetic, and it is the honest answer to "what does this
look like after a 24 hour race".

**Settled 2026-09-23: it does not, and the report no longer makes it.** `ReportLapsToSaturation`
now derives the straight's geometric term from `GetAdjacentDriverSpacingCentimetres()` against the
contact width and prints both rows, so the figure moves with `CarCount`, `TrackWidth` and
`DriverLateralSpread` instead of being a constant in a log string. The apex row still assumes one
texel takes the whole field and is now labelled in this file as the upper bound it is. The original
text follows.

**Not derived, only asserted:** that `ReportLapsToSaturation`'s one-apex-texel assumption survives
C6. It is already approximate today, because `DriverLineCommitmentAtCentre` and `AtEdge` are 0.2
and 0.8 rather than 1.0, so no car's target ever fully reaches `ApexLateralOffsetAlpha`; and C6's
deviation deliberately spreads corner arcs across more texels, which pushes the same way. The apex
row above should be read as an upper bound on concentration rather than a prediction. Nothing has
counted actual passes over a texel against any of these figures, which is the same gap
`MEASUREMENTS.md` already records against the original 17-and-50 numbers.

**The slip model is symmetric about the apex, and that is a realism gap C6 does not fix.**
`GetCorneringAmountAtDistance` is the absolute value of signed curvature sampled from the authored
spline, with no braking, traction or direction-of-travel term, so for a constant-radius corner the
deposited slip profile is symmetric about the apex by construction. Real circuits rubber up most at
corner *exit* under traction and in braking zones. The one asymmetry the model does have —
`CorneringAmount` taking the max of current and lookahead curvature — extends the high-slip zone
earlier on *entry* only, so it biases the opposite way from reality. This affects the intensity
texture of a mark, not the position of the line, so it leaves the emergent-line claim intact. It is
recorded here as a known gap rather than scheduled, because fixing it means deriving slip from
longitudinal acceleration as well as curvature, which is a larger change than C6.

### Tests

- **Determinism**: two runs with the same seed produce identical lateral offsets at the same
  distances. Without this the rest of the harness is no longer reproducible.
- **Distribution**: over many corner entries, the deviations have a mode near zero and a spread
  inside the clamp. Asserting the *shape*, not a specific sequence.
- **Held through the corner**: the deviation does not change between corner entry and exit.
- **Stripes merge on the straight**: `AdjacentSpacing <= ContactWidth / 4` for the spawned field,
  asserted from the spawn parameters rather than judged from a screenshot. This is the test that
  makes "nose-to-tail" falsifiable, and it is the same arithmetic that produced the 100:1 figure
  above, so if it fails the saturation table is wrong too.
- The existing `DriversTakeDifferentLinesThroughACorner` will need revisiting — it asserts a
  committed driver crosses to the inside from a wide baseline, and the baselines are about to stop
  being wide.

*Exit: twenty cars run visibly nose-to-tail on the straights and fan into distinguishable arcs
through a corner, the same seed reproduces the same run, and the accumulated buffer shows one
dominant line with separable arcs around it at a corner rather than a single smear.*

### Decided on implementing, 2026-09-23

**The field-spacing drift stays a known defect. It is not promoted to a prerequisite.** The
deposit does not depend on it. Every quantity in `ASplineFollowingCar::Tick` and the contact it
registers is a function of that one car's own `DistanceAlongTrack` and `LateralOffsetAlpha` and
the track's fixed curvature profile, with no term reading any other car, so drift changes *when*
cars are near each other and never *where* any car deposits. Since every car completes whole laps,
every car paints its line at every distance whatever the drift has done to the formation. The
graded artefact is the buffer, and the buffer cannot tell.

What C6 does change is the cost of the defect on screen: with the lateral spread at 0.075 the
whole field is inside about one car width laterally, so a pair whose along-track gap drifts to
under the 220 cm car length now reads as meshes intersecting rather than as cars on separate
lines. The measured rate is -609 cm per 3000 ticks for one pair against 1400 cm of grid spacing,
so a single long run will show it. That makes it a prerequisite for *one particular way of
filming the exit condition* — a single continuous run used for both the nose-to-tail shot and the
buffer shot — and not for the exit condition itself, which a short early clip plus a buffer
capture demonstrates honestly. Recorded rather than fixed, because the cheapest fix that
guarantees no overlap is a car correcting toward a formation slot, which is a controller and not
an independent policy, and would need the same caveat as `DriverLateralSpread = 0`.

There is a cheaper experiment that costs the emergence claim nothing, and it is not done here:
`MEASUREMENTS.md` blames `FInterpConstantTo`'s rate limit on speed for the drift, and raising
`SpeedChangeCentimetresPerSecondSquared` would test that without adding any coupling between
cars, since each car's speed stays a function of its own position. That is a separate change with
its own measurement.

**The corner-entry trigger chattered, and the release is what needed fixing.** Drawing on a
rising edge of `CorneringAmount` through 0.15 with a 0.05 release gave **2.82 draws per corner**,
not one, which is nearer the per-frame jitter the design exists to avoid than the single held arc
it asks for. The cause is in the C3 cornering profile, not in C6: sampling the trigger signal
itself every 25 cm over one oval lap finds two genuine low windows of 2475 cm and 2450 cm and two
spurious notches **25 cm wide**, single-sample artefacts of the 150 cm `CorneringSampleSpacing`.
Debouncing the *draw* by distance was the wrong mechanism and left 1.57 draws per corner.
Debouncing the *release* is the right one: a car stays committed until the track has been below
the exit threshold continuously for `SameCornerSeparationCentimetres`, so a notch narrower than
that cannot end a corner. At 600 cm — 24 times the measured notch and a quarter of the measured
genuine window — the oval gives exactly **one draw per corner**, with every gap between draws
landing on 15460 ± 15 cm, half a lap. The profile notches themselves are left alone; fixing them
means touching the curvature signal that speed and slip also read, which is a C3 change and needs
its own measurement.

---

## C7 — Freezing the field without pausing the game

Added 2026-09-23, outside the original scope. Requested directly: a key that stops and starts the
field at runtime, the level starting stopped, and **the camera still flyable while it is stopped**.

### Why this is not `SetGamePaused`

The obvious implementation is `UGameplayStatics::SetGamePaused`, and it does not work here. The
camera is the possessed pawn, and this project has no custom GameMode, PlayerController or Pawn —
`Game class is 'GameModeBase'` in the runtime log, so the pawn is Epic's stock `ADefaultPawn` with
a `UFloatingPawnMovement`, and all of its movement is legacy `BindAxis` bindings
(`DefaultPawn.cpp:138-144`) that do not fire while paused.

Making that pawn fly under a real pause needs four things true at once: the pawn ticking when
paused, its movement component ticking when paused (registered separately from the actor's tick),
`bExecuteWhenPaused` on each of its six axis bindings, and
`APlayerController::bShouldPerformFullTickWhenPaused`. The fourth is **protected**
(`PlayerController.h:1730`), so it needs a `APlayerController` subclass and a GameMode change to
install it. Without it mouse-look is discarded every frame and the render camera never re-syncs to
the pawn, so even working WASD would be invisible. That is four classes and a scattered
re-derivation of flags Epic only ever wires together inside `ADebugCameraController`, to pause a
demo whose only moving parts are twenty cars.

`SetGlobalTimeDilation(0)` is not an alternative either: `AWorldSettings::SetTimeDilation` clamps
to `MinGlobalTimeDilation`, which `BaseGame.ini` sets to 0.0001, so exactly zero is unreachable and
the field would creep.

### What it does instead

`ARacetrackCircuit` keeps the field it spawned and calls `SetActorTickEnabled` on each car. Nothing
is paused, so the camera, input and everything else are untouched by construction rather than by
repair. `P` toggles it, `bStartPaused` defaults true.

The deposit stops with the cars, but as a fact about this scene rather than a guarantee. The
subsystem keeps ticking; `FlushPendingContacts` simply finds nothing queued, so `StrokesResolvedThisFrame`
stays empty and `PaintStrokes` is never reached. The buffer holds whatever it had.

**There are two ways into `PendingContacts`, not one.** `RegisterSurfaceContactInTrackSpace` is the
one `ASplineFollowingCar::Tick` uses, and freezing the cars closes it. The other is
`RegisterSurfaceContact`, the world-space overload, which is public on the subsystem and exposed to
Blueprint through `URacetrackSurfaceBlueprintLibrary::RegisterSurfaceContact` — that is the
engine-agnostic API the whole plugin exists to offer. Nothing in this scene calls it today, which
is the only reason the buffer is stable while frozen. **Anything wired to that boundary later — the
Chaos vehicle the brief anticipates, a player-driven pawn — will keep depositing straight through
the freeze**, and the freeze would then have to move behind the API rather than in front of it.

Time dilation and deposition keys are deliberately not gated on the frozen state. A disabled tick
function is off the tick list entirely, so a dilation change has no effect until the field is
released, at which point it applies — which is the behaviour you want from a control you are
setting up *while* the field is stopped.

### What it is not

**This is not a game pause and must not be described as one.** `UGameplayStatics::IsGamePaused`
reports false the whole time, because nothing goes through `APlayerController::SetPause`. Real time
and audio time keep advancing. The naming says `field`, not `game`, for that reason.

One consequence to know about: it freezes **the cars the circuit spawned**, tracked in
`SpawnedField`. A hand-placed `ASplineFollowingCar` would keep ticking and depositing straight
through the freeze, which would read as a bug. `EDITOR_SETUP.md` records the five hand-placed cars
being removed when the circuit started spawning its own field, so there are none today. Freezing by
`TActorIterator` instead would catch them, and was rejected because a circuit freezing cars it does
not own is the wrong ownership for a harness that could hold two circuits.

*Exit: the demo starts with the field on the grid and nothing moving, `P` starts and stops it, and
the camera flies freely in both states.*

---

## C8 — Four tyre contacts per car, and the circuit rescaled to resolve them

Added 2026-09-23, outside the original scope. Requested directly, as two changes that were asked
for together and turn out to be one change: **each car lays four separate tyre tracks rather than
one line down its centre, and the circuit is rescaled so a single corner holds up when zoomed
into.**

They are one change because of a number worked out below: at today's scale the front and rear
wheels of a car lay tracks **0.8 texels apart**, so four wheels would store as two. The rescale is
what makes four wheels four.

### The rescale is a one-dimensional decision, not a three-dimensional one

`AuthoredLayoutLapLength`, `TrackWidth` and `SurfaceCentimetresPerTexel` look like three knobs.
At the unfolded cap they are one.

`FSurfaceTrackRibbon::MaximumTexelsPerRow` is 16384, which is the D3D 2D-texture dimension cap
rather than a project choice, so the along-track axis cannot exceed it however the other two
move. Spending that axis fully gives `LapLength = 16384 * CentimetresPerTexel`, and then

```
Bytes = 16384 * ceil(TrackWidth / CentimetresPerTexel) * 8
```

**Memory depends only on the across-track texel count.** Nothing else in the ribbon can move it.
32 MiB is exactly 256 across-track texels at the cap, and today's ribbon uses 123 of them.

That reframes the brief's own table. Its rows grow from 16.4 MB to 65.5 MB because they hold
`TrackWidth` at 1200 while shortening the lap, which fattens the track relative to the layout by
the same factor it sharpens the texel. Hold the across-track texel count instead and the memory
is flat, and the decision reduces to: **how much circuit, against how fine a texel.**

### The memory budget, which is not the 64 MB the brief provisionally allowed

B4 recorded the committed default as **32.0 MiB**, and disqualified 128 MiB in terms: *"too much
to impose by default in a plugin others install"*. 64 MB has no ruling of its own — RGBA32f at
2048 square reached it and was disqualified on precision and decay cost rather than on bytes.

Phase R then delivered the same 1500 m circuit in **15114240 bytes**, less than half the B4
commitment, and never spent the difference. So the budget for this change is the headroom phase R
created inside a figure phase B already signed off. **The ceiling is 32 MiB and it is not
exceeded.**

### Decided: 480 m of lap, 800 cm of track, 3.125 cm per texel

Every figure is an exact ratio against what is already running, which is the reason this
combination was chosen over several that are nearer the brief's suggestions.

| | today | C8 | factor |
|---|---|---|---|
| `AuthoredLayoutLapLength` | 150000 cm | **48000 cm** | ÷ 3.125 |
| `SurfaceCentimetresPerTexel` | 9.765625 | **3.125** | ÷ 3.125 |
| `TrackWidth` | 1200 cm | **800 cm** | × 2/3 |
| `RoadSegmentLength` | 400 cm | **128 cm** | ÷ 3.125 |
| Along-track texels | 15360 | **15360** | unchanged |
| Across-track texels | 123 | **256** | × 2.08 |
| Buffer | 15114240 B, 14.41 MiB | **31457280 B, 30.00 MiB** | × 2.08 |
| A 20 cm contact patch | 2.05 texels | **6.40 texels** | × 3.125 |

`48000 / 3.125 = 15360` and `800 / 3.125 = 256`, both exact. Neither axis carries any `ceil`
slack, which is not true today — `1200 / 9.765625 = 122.88` rounds up to 123, and the lap length
straddles a `ceil` boundary badly enough that the same circuit has logged both 15361 and 15360
along-track texels on different runs (`MEASUREMENTS.md`, *A correction to the recorded buffer
size*).

**The along-track extent deliberately does not change.** 15360 is the value the project already
runs, allocates and measures, and it keeps the same 1024-texel margin under the cap.

#### Why not 400 m at 2.44140625, which is the obvious candidate

It is exactly four times today's density, it is the density
`SurfaceStrokeWidthTests.cpp:68` already exercises, and `40000 / 2.44140625 = 16384` exactly. That
last property is the reason to reject it: it lands **on** the cap, not under it. The Spa spline is
rescaled about its centroid to hit `AuthoredLayoutLapLength` and then measured, and the measured
result has already been seen to land a fraction either side of the requested figure. A fraction
over here gives `ceil(16384.0001) = 16385`, `FitsUnfolded()` returns false, and
`USurfaceAccumulationBuffer::Allocate` refuses the allocation — so the whole surface silently does
not exist, with one `Error` in a log as the only symptom. A design that is correct only when a
floating-point rescale rounds the right way is not a design.

Rejected for the same reason: any pairing that spends the along-track axis to 16384.

#### Why not keep `TrackWidth` at 1200

Holding 1200 caps the density at 4.8828125 cm per texel (246 across-track texels, 32.24 MB) and a
lap of 800 m. That is only a 2x density gain for the whole cost of the rescale, and it leaves the
contact patch at 4.1 texels — still too few for a mark to have a profile rather than an edge.

#### Why 800 cm and not 600

600 cm is affordable — 246 across-track texels at 2.44140625, 28.83 MiB — and was rejected on
headroom rather than on memory. A wheel sits 40 cm off the car's line and its contact patch
reaches 50 cm, which is `50 / (TrackWidth/2)` in alpha. A contact outside `|alpha| <= 1` is
**silently discarded** by `RegisterSurfaceContactInTrackSpace`, counted only in
`ContactsDiscardedOutsideRibbon`, which nothing reads. Worst realistic car alpha is 0.735
(baseline 0.075, commitment 0.8, apex target clamped at 0.9), and body yaw under maximum lateral
rate adds about 0.06 more:

| `TrackWidth` | outer wheel alpha at 0.735 | with maximum yaw |
|---|---|---|
| 600 | 0.902 | 0.961 |
| **800** | **0.860** | **0.905** |
| 1200 | 0.907 → 0.940 | — |

600 leaves 0.039 of margin against a silent discard. 800 leaves 0.095. The test added below
asserts the discard count is zero over a real field run, so this stops being a judgement either
way, but the margin is the reason not to sit on the edge of it.

800 cm also brackets the one proportion in the layout that can be derived rather than chosen.
The track cannot scale with the lap — the cars are a fixed physical size, which is the harness
convenience `EDITOR_SETUP.md` already flags — so the honest reference is **track width in car
widths**. Real racing runs about 7.5 (15 m of Spa against a 2 m car). Today's harness runs 12.
800 cm against the 100 cm car runs **8**.

### What this supersedes

**`EDITOR_SETUP.md`'s "Why 1500 m and not Spa, halved" is superseded, not contradicted.** Its
reasoning was correct and is now answering a different question. It asked for the largest round
lap that fits the cap *at a fixed production density of 9.7656 cm per texel*, and 1500 m was the
right answer to that. C8 removes the fixed density from the question: the density is now chosen
first, for what a corner has to look like close up, and the lap follows from it. Both documents
carry the same table; only the free variable moved.

Not superseded: RGBA16f, `k = 3.0`, the additive write, the C0 flush-deferral guard, the lap-seam
split, and the ribbon shape itself.

### Four wheels: the model, and why a naive one gives two tracks

`ASplineFollowingCar` is a point on a spline. Its rotation is the spline tangent, it has no
wheelbase, and adding four lateral offsets to it lays **two** tracks, because front and rear
wheels at the same lateral offset trace the same curve. Two of the four would be exactly
redundant.

The fix is the textbook kinematic bicycle model, and it is chosen because it produces the wanted
behaviour as a consequence rather than as a term added to get it:

- **The tracked point is the rear axle.** `DistanceAlongTrack` and `LateralOffsetAlpha` describe
  where the rear axle is. This is the standard convention and it is the one that makes the rest
  fall out.
- **Body heading is the rear axle's own velocity direction**, which is the no-slip rear-wheel
  constraint. The car therefore gains a yaw independent of the spline tangent whenever it is
  moving laterally, which it does not have today.
- **Front axle = rear axle + heading × wheelbase.** A chord along the tangent of an arc lands
  outside that arc, so the front axle traces radius `sqrt(R² + Wheelbase²)` while the rear traces
  `R`. The rear cuts inside by `Wheelbase² / (2R)` with no code saying so.
- The four wheels are then the two axles ± `WheelTrack/2` along the body's right vector.

On a straight the heading is the tangent, the axles coincide laterally and the car lays **two**
tracks. That is correct: a car going straight leaves two tyre tracks, not four. Four appear in
corners, which is where a racing surface shows them.

**This is the number that ties the rescale to the wheels.** Offtracking as a multiple of the
contact patch is `Wheelbase² / (2 R * ContactWidth)`, which is independent of texel size and
depends only on the corner radius — so only on the lap length:

| | corner radius | offtracking | in contact patches | in texels |
|---|---|---|---|---|
| Today, 1500 m, fast corner | 1258 cm | 7.8 cm | 0.39 | **0.80** |
| C8, 480 m, fast corner | 403 cm | 24.3 cm | 1.22 | **7.8** |
| C8, 480 m, slow corner | 222 cm | 44.1 cm | 2.21 | **14.1** |

Four wheels at today's scale would store as two everywhere but the very tightest corner. At
480 m the front and rear tracks separate by one to two mark widths, which reads as two marks. The
radii are measured from the authored centreline over an 80-unit and a 40-unit chord respectively;
the test added below prints the real figures so these stop being arithmetic.

#### Wheelbase and wheel track, and no mesh change

| | value | derivation |
|---|---|---|
| `Wheelbase` | **140 cm** | axles at ±70 on a 220 cm body, leaving 40 cm of overhang at each end |
| `WheelTrack` | **80 cm** | wheel centrelines at ±40 on a 100 cm body, so a 20 cm contact patch spans 30 to 50 cm and sits inside the body rather than proud of it |

**The mesh does not change.** It is a scaled `/Engine/BasicShapes/Cube` standing in for a car, the
wheels as defined sit inside it, and modelling wheel geometry is scope that was not asked for.
`SetActorLocation` moves from the tracked point to `RearAxle + Heading * (Wheelbase / 2)`, so the
box still straddles both axles once the tracked point becomes the rear axle rather than the
centre.

Both are `UPROPERTY` with accessors rather than file-scope constants, matching how `ContactWidth`
is already exposed, because the tests below assert against them.

### The API decision: the wheels register through the world-space boundary

Priced both ways, as asked.

|  | world-space `RegisterSurfaceContact` | track-space `RegisterSurfaceContactInTrackSpace` |
|---|---|---|
| Cost | one `FindInputKeyClosestToWorldLocation` per contact | zero spline searches, as today |
| Geometry | correct by construction; offtracking falls out of rigid-body placement | hand-derived, needs local signed curvature in 1/cm, which nothing exposes |
| New engine surface | none | a new curvature accessor on `ARacetrackCircuit`, and an arc-length correction `(1 - kappa * y)` for a point at lateral offset y |
| Stroke velocity | exact, from the wheel's own previous world position | needs `omega x r` per wheel to be derived separately |
| Seam | handled by the existing conversion | world space has no seam, but track space does |
| Brief | exercises the engine-agnostic boundary the plugin exists to offer | does not |

**Decided: world space.** Three reasons in order.

1. It is the boundary the brief is about, it is what a real vehicle system would call, and
   `PLAN.md` C7 already records that **nothing in this scene calls it** — the only callers repo-wide
   are the Blueprint wrapper and three automation tests. Four wheels is the natural thing to
   exercise it with.
2. The track-space version reimplements a coordinate transform the plugin already owns, and it
   gets the interesting part — offtracking — from hand-derived curvature rather than from
   geometry. Per this project's stated preference, the obvious implementation over the clever one.
3. Wheel velocity from `(Current - Previous) / dt` in world space is exact and free, and it is
   what makes the stroke land where the wheel actually went.

**The cost is real and it trips R2's own written trigger.** R2 recorded: *"Implement [the
per-contact segment cache] when a circuit exceeds about 230 centreline spline points, **or when
any world-space caller reaches 80 contacts per frame in a real scene**."* Twenty cars times four
wheels is exactly 80. The Spa spline is 77 points; R2's linear-in-points result puts that near
0.32 ms per frame, about 7 % of the measured 4.484 ms frame.

`RacetrackSurface.Performance.TrackSpaceConversion` already measures both the full search and
`FindInputKeyOnSegmentClosestToWorldLocation`, so the cache is already priced. **C8 measures the
figure at 77 points and reports it. It does not implement the cache**, because that is plugin
optimisation work that was not asked for and the trigger is a recommendation to the reader of
this file, not a licence to expand scope. If the measured figure is bad, it is reported as bad.

### Per-wheel slip, and what makes a wheel lock

Each wheel carries its own `SlipAmount` and its own `EAccumulationChannel`. Three wheels keep the
existing `Lerp(StraightSlipAmount, CorneringSlipAmount, CorneringAmount)` on `RubberBuildup`; a
locked wheel carries `LockedWheelSlipAmount` on `SkidMark`.

**The trigger is braking, and it reuses C6's mechanism rather than inventing one.** C6 established
draw-once-on-a-rising-edge-and-hold, with the *release* debounced by distance because debouncing
the draw did not work. The same shape applies here:

- `BrakingAmount = clamp((Speed - TargetSpeed) / (StraightLineSpeed * (1 - CorneringSpeedFraction)), 0, 1)`,
  which is the speed deficit normalised by the largest deficit the model can demand. Self-scaling,
  no magic constant, and it is already computed in `Tick`.
- On a rising edge through `BrakingEntryAmount`, draw once: whether this braking event locks a
  wheel at all, and if so which **front** wheel. Held until the car has been below
  `BrakingExitAmount` continuously for `SameBrakingEventSeparationCentimetres`.
- Front only. Under braking, load transfers forward and the rear locks only on trailing throttle
  or a handbrake, neither of which this model has.

**The lockup draws from a second `FRandomStream`, not the existing one.** Seeded from the same
`DriverSeed` through a fixed offset, so the run stays reproducible from `FieldRandomSeed` plus
grid index exactly as before. The reason for a second stream rather than more draws on the first
is that C6's corner-deviation distribution is a **measured** result with a recorded shape, and
interleaving new draws into that stream would move every C6 figure for a reason unrelated to C6.
Decorrelating costs one `FRandomStream` and keeps the earlier measurement reproducible.

`LockupProbability` defaults to 0.15. That is a by-eye demo value in the same class as
`MaxCornerDeviationAlpha`, and it is labelled as one rather than defended as derived.

### The C6 stripe-merge condition, restated

C6 asserted `AdjacentSpacing <= ContactWidth / 4` so that the field paints one merged band on a
straight rather than separate stripes with gaps. Four wheels put a second population of marks at
a fixed wheel-track offset, so that one sentence now has to be two.

**Within a wheel band, the condition is unchanged, and so is the number.** Adjacent cars'
*corresponding* wheels are still `AdjacentSpacing` apart, so the formula and the fourfold-overlap
target say exactly what they said. At the new track width the spacing improves from 4.737 cm to
**3.158 cm** against the same 5 cm target, because halving the track halves the spacing while the
contact patch stays 20 cm.

**Between the two wheel bands there is a new structure, and it is not forced to merge.** Each
band is `2 * DriverLateralSpread * TrackWidth / 2 + ContactWidth` wide — 80 cm — and they sit
`WheelTrack` = 80 cm apart, so at this field spread they close up exactly. Whether they merge is
a consequence of the field spread against the wheel track, and both answers are physically
correct: a lightly used road shows two strips, a heavily used one shows a single band.

So the test asserts what must be true and **reports** what is a consequence:

- asserted: `AdjacentSpacing <= ContactWidth / 4`, unchanged, now per band;
- asserted: `WheelTrack > ContactWidth`, without which four tracks could never be four;
- reported: band width, band separation, and whether the two close up.

**One observation worth keeping for the write-up.** With four wheels the dominant line is a
dominant *pair*, and the racing line — the thing this project claims emerges — is the centreline
between them, which no tyre ever touches.

### The saturation figures all move, and by how much is a count, not a derivation

Every figure in the corrected C6 table is per texel per pass, and three separate terms change:
the track halves in width, the texel shrinks 3.125-fold, and each car now lays four contacts of
which two coincide on a straight. Deriving the product of those is exactly the mistake the C6
entry already records making twice.

`ReportLapsToSaturation` and
`RubberAccumDemo.Track.CountedPassesOverATexelAgainstThePrediction` are both re-run, the counting
test is extended to count per wheel rather than per car, and the corrected C6 table above and the
matching `MEASUREMENTS.md` entry of 2026-09-23 are rewritten from the counted result.

The direction is predictable and the magnitude is not: halving the track halves `AdjacentSpacing`,
which doubles how many cars cross a straight texel; halving it also halves the corner fan in
centimetres while the texel shrinks further, which cuts passes per apex texel. Both rates move
and the 1.5:1 contrast may move either way.

### Tasks

| # | Task | Exit |
|---|---|---|
| C8.1 | `Wheelbase`, `WheelTrack`, `LockupProbability`, `LockedWheelSlipAmount` on `ASplineFollowingCar`, with accessors | Compiles, defaults as tabulated |
| C8.2 | Rear-axle tracked point, heading from velocity, four wheel world positions, per-wheel previous position for exact velocity | Four distinct contacts a tick in a corner, two coincident lateral offsets on a straight |
| C8.3 | Register through `RegisterSurfaceContact(WorldPosition, ...)` | The world-space boundary is exercised by the scene for the first time |
| C8.4 | Braking-triggered lockup on one front wheel, second `FRandomStream`, `SkidMark` channel | One dark mark among four, reproducible from the seed |
| C8.5 | `AuthoredLayoutLapLength` 48000, `TrackWidth` 800, `SurfaceCentimetresPerTexel` 3.125, `RoadSegmentLength` 128 | 15360 x 256, 31457280 bytes, logged at allocation |
| C8.6 | Rewrite `RegistersMoreSlipInACornerThanOnAStraight`, which hard-asserts one pending contact a tick | Asserts the new model rather than the old one |
| C8.7 | Restate and rewrite `SpawnedStripesMergeOnTheStraight` per the two conditions above | Both asserted, band geometry reported |
| C8.8 | Extend `CountedPassesOverATexelAgainstThePrediction` to count per wheel | New counts in `MEASUREMENTS.md` from a real run |
| C8.9 | New: zero contacts discarded outside the ribbon over a field run | The silent-discard path is checked, not assumed |
| C8.10 | New: offtracking sign and magnitude, and the authored layout's tightest radius | Rear inside front, by a printed number |
| C8.11 | Confirm the Stadium layout still fits the cap at 3.125 | 9895 x 256, 19.33 MiB, asserted |
| C8.12 | `L_SpaHarness` instance overrides updated, logged in `EDITOR_SETUP.md` | The demo map runs the rescaled circuit |

### Known interactions, recorded rather than fixed

**The authored centreline has kinks tighter than the corners it draws, and finer texels will
expose them.** The 76 points were read off a plan view by eye, which `EDITOR_SETUP.md` already
records as not survey-accurate. Measured over a 400 cm road ring at today's 1500 m, the tightest
radius on the layout is **277 cm** against a half-width of 600 — so `GenerateRoadMesh` already
folds its inside edge through the centre of curvature somewhere near 66 % of the lap, and has done
since it was written. It is invisible at 9.77 cm per texel on a 425 m circuit. At 480 m and 800 cm
wide the same point has a radius of about 122 cm against a half-width of 400, so the fold gets
about 13 % deeper in relative terms without changing in kind. **Not fixed**: smoothing the
authored data is a change to the curvature signal that speed, slip and the C6 trigger all read,
which is a C3 change needing its own measurement. C8.10 measures and prints it so it is on the
record.

**The sagitta bound tightens with the texel.** `s <= sqrt(8 * R * CentimetresPerTexel)` falls from
988 cm to 313 cm for the fast corner and to 233 cm for the slow one. C3 already measured a stroke
of about 330 cm at 20x dilation and 100 fps, so **stroke subdivision moves from "C4's first task"
to a defect visible in the demo's own time controls above about 10x.** Still C4's task, still not
started, now with a smaller margin.

**`SameCornerSeparationCentimetres` is a car property tuned against the oval.** The oval is not
rescaled, so 600 cm still gives exactly one draw per corner there. On a 480 m Spa the straight
windows between corners are 3.125 times shorter, and if the shortest genuine window falls under
600 cm a car will hold one deviation for a whole lap. Measured and reported by C8.10 rather than
pre-emptively retuned.

**`GridSpacingCentimetres` deliberately does not scale with the lap.** At 1400 cm the field's
train covers 58 % of a 480 m lap against 19 % of a 1500 m one, so the grid reads as more spread
out. Scaling it to 450 would restore the proportion and would also close the gap between cars to
about one car length, against a measured along-track drift of 609 cm per 3000 ticks — which would
promote the known drift defect from *visible in a long run* to *visible immediately*. C6 decided
not to pay that, and the same decision holds. The buffer cannot tell either way.

*Exit: each car lays four separately identifiable tyre tracks, a single locked wheel appears as one
dark mark among the four, a corner fills the screen without texels reading as blocks, the buffer
allocates at 15360 x 256 for 31457280 bytes, and the saturation figures in `MEASUREMENTS.md` come
from a re-run count rather than from this document's arithmetic.*

### Measured on implementing, 2026-09-23. Four predictions held, two did not.

All 55 automation tests pass headlessly with the editor closed, and both build paths are clean.
Full numbers in [MEASUREMENTS.md](MEASUREMENTS.md); what follows is only where this section was
wrong.

**The buffer figures were exact.** `15360 x 256 texels, 31457280 bytes` at `3.125000 cm per texel`,
with a 20 cm contact patch storing as `6.40` texels and 1024 texels of margin under the cap. The
readback confirms the stored mark: **6 inked texels, 18.75 cm** against a 20 cm patch, where the
same test reads 2 texels at the old density. The Stadium lands at 9895 x 256, 19.33 MiB.

**The bicycle model validated better than it needed to.** On the Stadium's constant 3000 cm
radius, with a driver holding the centreline at zero line commitment so nothing but the wheelbase
separates the axles, the rear axle tracks **3.156 cm inside the front against 3.267 cm** from
`Wheelbase² / (2R)` — 3.4 % off a formula nothing in the code computes. On a straight the same
measurement gives 0.063 cm, so the two-tracks-not-four claim holds to within a millimetre.

**The offtracking table above was pessimistic, and it was measured at the wrong moment.** Over a
settled lap of the rescaled Spa a car's front and rear tracks separate by up to **56.76 cm, 2.84
mark widths**, and exceed one mark width over **5.1 %** of the lap. The table predicted 1.22 to
2.21 mark widths from steady-state offtracking alone.

The reason it undershot is the more interesting half, and it is a mechanism this section missed
entirely. **Two different effects separate the axles, and they act in opposite directions.**
Steady-state offtracking puts the rear inside the front. During turn-in the body is yawed into the
corner, so the front axle — one wheelbase along the heading — is displaced toward the *inside*
instead, and at the lateral rates this model reaches that term is the larger of the two. The first
version of the four-tracks test asserted "rear inside front" at the tightest point on the lap,
measured the front 19.43 cm inside the rear, and failed. Both readings are correct vehicle
behaviour; the test was asserting one of them everywhere. It now checks the steady-state sign on
the Stadium, where there is a genuine constant-radius corner to be steady in, and checks only
*resolvability* on Spa, where the transient dominates.

**The road-mesh fold gets about twice as bad, not 13 % worse.** That estimate held the measuring
chord at a fixed 200 cm; the honest comparison holds it at one road-ring spacing, which scales with
the layout. Measured, as the tightest three-point radius over one `RoadSegmentLength`:

| | tightest radius | half width | ratio |
|---|---|---|---|
| Spa at 1500 m, 1200 wide, 400 cm rings | 277 cm | 600 | 0.46 |
| **Spa at 480 m, 800 wide, 128 cm rings** | **89 cm** | **400** | **0.22** |
| Stadium at 480 m equivalent, 128 cm rings | 352 cm | 400 | 0.88 |

The fold ratio is `TightestRadius / HalfWidth`, and holding the chord at a fixed fraction of the
lap makes it proportional to `LapLength / TrackWidth`. That ran at 125 and now runs at 60, so the
fold is 2.08 times deeper — **and it is unavoidable, not a bad choice.** The cars are a fixed
physical size, so a shorter lap has to carry a proportionally wider track, and a proportionally
wider track folds harder on the same kinks. Holding the old ratio at a 480 m lap would need a
384 cm track, which fails the wheel-alpha headroom above. Whether it is visible is a judgement for
a capture, not a number.

The Stadium's 0.88 is new information and is not the same defect: it is a curvature spike at the
straight-to-corner knot, where a Catmull-Rom through the generated points has no curvature
continuity. It has always been there and is marginal.

**The wheel-alpha headroom was conservative.** Twenty cars over 6000 ticks put the widest wheel at
alpha **0.7864** against the 0.860 to 0.905 predicted, with **zero** contacts discarded outside the
ribbon. The margin is 0.21 rather than 0.10, so the choice of 800 over 600 has more room than it
was justified on.

**The two wheel bands abut exactly, as predicted.** Each band measures 80.0 cm against an 80 cm
wheel track, overlapping by 0.0 cm, and the whole feature spans 160 cm, 20 % of the track.

**The world-space conversion costs 0.3658 ms**, measured at 80 contacts on a 77-point spline rather
than interpolated: the timing test now carries 77 as a candidate because that is what
`RacetrackLayouts::SpaCentreline` holds. That is 2.2 % of a 60 Hz frame and about 8 % of the
4.484 ms frame this scene actually runs at. The segment-cache path measures **0.0087 ms at the same
point, 42 times cheaper** and flat in spline points. **The cache is still not implemented**, and
the cost of not implementing it is now a number rather than an estimate.

### Corrected 2026-09-23 on looking at it: 480 m is below the geometric floor, and the lap length is wrong

Seen on screen, the road **passes through itself at every tight corner**. This section already
carried the fold as "a pre-existing defect made about twice as deep", which treated it as a
degradation. It is not. It is a **hard constraint, and 480 m violates it.**

A generated road offsets `TrackWidth/2` either side of the centreline, so the inside edge crosses
its own centre of curvature wherever the local radius falls below the half width. That is not an
artefact, it is the offset curve being undefined there, and it produces the crumpled overlapping
geometry visible in a close-up of any slow corner.

| `TrackWidth` | never folds above | comfortable above | authored kinks clean above |
|---|---|---|---|
| 800 cm | 477 m | 954 m | 1726 m |
| 1000 cm | 596 m | 1192 m | 2158 m |
| 1200 cm | 715 m | **1430 m** | 2589 m |

**480 m at 800 cm wide clears the bare threshold by three centimetres** — the fastest corner
measures 403 cm of radius against a 400 cm half width — and fails on every corner tighter than
that. The pre-C8 configuration of 1500 m at 1200 cm sat just above the *comfortable* line at
1430 m, which is why it looked right. That was not judgement, it was a threshold nobody had
written down.

**The consequence for C8: the texel decision is sound and the lap decision is not.** Everything
measured above about density, offtracking, the lockup and the budget stands. The lap length does
not, and it cannot be fixed by any setting that keeps the along-track axis under 16384 at
3.125 cm per texel — 954 m of lap at that density needs 30528 texels.

**This is the argument for folding, and it is a stronger one than resolution.** Row folding is not
polish on how the surface looks; it is what allows a lap long enough for the road geometry to be
valid *and* a texel fine enough to resolve a tyre mark. The two requirements are otherwise
mutually exclusive.

| Target lap | Along-track at 3.125 | Rows | Texture | Bytes |
|---|---|---|---|---|
| 1500 m | 48000 | 3 | 16384 x 768 | 100.7 MB |
| **1726 m, kinks clean at 800 cm** | 55296 | 4 | 16384 x 1024 | **134.2 MB** |
| 2600 m | 83200 | 6 | 16384 x 1536 | 201.3 MB |
| 7004 m, full Spa | 224128 | 14 | 16384 x 3584 | 469.8 MB |

**An interim that works today without folding**, if the demo has to run before that lands: 1000 m
of lap at 800 cm wide and 6.5104 cm per texel is 15360 x 123, 15.1 MB, a 3.07 texel mark, and
clears the comfortable fold threshold. Worse than C8 on density, better than pre-C8, and valid.

**`ElevationExaggeration` must not be raised to compensate for the smaller layout.** Doing so was
tried and reverted. The authored Z values are integers and 35 of the 76 points step by exactly one
unit, so the profile has a quantisation floor: at 7.8125 on a 480 m lap one integer step becomes a
77 cm rise over about 620 cm of track, a 12.4 % gradient step, and the circuit visibly terraces.
At the default 2.5 the same step is 4 %. Restoring Spa's drama needs a longer lap, not a larger
multiplier — the same conclusion the fold reaches by a different route.

**A defect this change exposed, which predates it.** `ARacetrackCircuit::PostInitializeComponents`
was the only caller of `ConfigureTrackRibbon`, and `AActor::PostActorConstruction` only runs
`PostInitializeComponents` when `World->AreActorsInitialized()` (`Actor.cpp:4436`), which is false
in the editor world the automation tests spawn into. **No host test has ever configured the surface
ribbon.** It did not matter while `ASplineFollowingCar` used the track-space overload, which needs
no centreline; the moment the wheels went through the world-space API every contact was discarded
for want of one, and the counting test was silently measuring against the ribbon struct's own
defaults of 1200 cm and 9.7656 cm per texel rather than against the circuit. `PublishTrackRibbon`
is now called from `OnConstruction` and `SetLayout` as well, which also means editing `TrackWidth`
or the layout in the editor resizes the ribbon instead of waiting for the next play session.

---

## C9 — Folded ribbon rows, so the along-track axis can exceed 16384 texels

Added 2026-09-23, directly after C8's correction and because of it. C8 chose the texel size for what
a corner has to look like close up, and then discovered the lap length that follows from it puts the
road through itself. **This phase exists to make those two requirements stop excluding each other.**

The argument is [C8's own correction](#corrected-2026-09-23-on-looking-at-it-480-m-is-below-the-geometric-floor-and-the-lap-length-is-wrong)
and is not repeated here. The short version: a generated road offsets `TrackWidth/2` either side of
the centreline, so the inside edge crosses its own centre of curvature wherever the local radius
falls below the half width. At 800 cm of track that needs **954 m of lap to never fold and 1726 m
for the authored kinks to be clean**, and 954 m at 3.125 cm per texel already needs 30528 along-track
texels against a hardware cap of 16384.

### Decided: 1800 m of lap, 800 cm of track, 3.125 cm per texel, four rows

| | C8 as written | `L_SpaHarness` interim | **C9** |
|---|---|---|---|
| Lap | 480 m | 960 m | **1800 m** |
| `TrackWidth` | 800 cm | 800 cm | **800 cm** |
| `SurfaceCentimetresPerTexel` | 3.125 | 6.25 | **3.125** |
| Along-track texels | 15360 | 15360 | **57600** |
| Across-track texels | 256 | 128 | **256** |
| Rows | 1 | 1 | **4** |
| Allocation | 15360 x 256 | 15360 x 128 | **14400 x 1024** |
| Bytes | 31457280, 30.00 MiB | 15728640, 15.00 MiB | **117964800, 112.50 MiB** |
| A 20 cm contact patch | 6.40 texels | 3.20 texels | **6.40 texels** |
| Geometry | **folds at every slow corner** | clears the comfortable line | **clears the kink line by 74 m** |

Every figure divides exactly, which is the property C8 chose its numbers for and the reason a
fraction-long rescale cannot push a `ceil` over a cliff:

```
180000 / 3.125 = 57600      57600 / 4 = 14400      800 / 3.125 = 256
```

**Why 1800 and not 1726.** 1726 m is the measured threshold itself, and the Spa spline is rescaled
about its centroid to hit `AuthoredLayoutLapLength` and then *measured*, landing a fraction either
side of the requested figure. Sitting on a threshold is the same mistake C8 rejected 400 m at
2.44140625 for. 1800 m clears it by 74 m and leaves 1984 texels of margin under the 16384 cap.

**Why not 1728 m**, which is 55296 along-track texels and a rounder 4 x 13824: it clears the kink
threshold by 2 m. Same objection, worse margin.

**`TrackWidth` stays 800 cm.** C8 measured the widest wheel at alpha 0.7864 with zero contacts
discarded, a margin of 0.21 rather than the 0.10 it was justified on. Widening to 1200 would push
the kink threshold to 2589 m and cost another 50 % of memory for nothing this phase needs.

**`RoadSegmentLength` stays at its C++ default of 128 cm**, and `L_SpaHarness` drops its 256 cm
override. `180000 / 128` is 1406.25, and `GenerateRoadMesh` rounds to 1406 segments at 128.02 cm, so
this does not need to divide. At the tightest authored radius the sagitta is about 4.9 cm against
69 cm at the ratio-preserving 480 cm, and the interim capture already recorded visible faceting at
256 cm on a shorter lap.

### The C++ default becomes the demo, and the 32 MiB commitment is retired

C8 left the project with **the C++ defaults broken and the map overriding them into something
valid**, which is backwards. This phase resolves it in the direction the map was already pointing:
the C++ defaults become the real configuration and `L_SpaHarness` clears all three overrides.

That also settles the budget question, which B4 had left as a commitment to 32.0 MiB with 128 MiB
disqualified in terms: *"too much to impose by default in a plugin others install"*. **That argument
is retired on its own premise.** The plugin is not being published, so there is no third party having
a default imposed on them, and the reason to keep a small default was never anything else. Recorded
as a reversal rather than quietly dropped: B4's reasoning was correct for a plugin that ships, and
this one does not.

So `FSurfaceTrackRibbon`'s own defaults move to the demo's numbers too, rather than staying a small
safe fallback that disagrees with everything else in the project. **The consequence worth stating:
a default-constructed ribbon is now folded, so the folded path is the default path** and every test
that does not deliberately configure otherwise exercises it. A fold that only runs in the demo would
rot; this one cannot.

112.50 MiB is 1.4 % of this laptop's 7948 MB of dedicated VRAM.

### Row layout, and the one formula

Rows stack in Y. The along-track axis is cut into `RowCount` equal spans of lap, each laid out left
to right across the full atlas width, and each row occupies a band of `AcrossTrackTexels` rows of
atlas texels.

```
RowCount     = ceil(AlongTrackTexels / MaximumTexelsPerRow)
TexelsPerRow = ceil(AlongTrackTexels / RowCount)
AtlasWidth   = TexelsPerRow
AtlasHeight  = RowCount * AcrossTrackTexels
```

`RowCount * TexelsPerRow` is at least `AlongTrackTexels`, never less, so the effective texel is at
worst a shade finer than asked for and never coarser. At the chosen configuration both divisions are
exact and there is no slack at all.

The mapping from a ribbon coordinate `(u, v)`, `u` the fraction of the lap and `v` the fraction
across the track, to the atlas:

```
s        = u * RowCount
row      = clamp(floor(s), 0, RowCount - 1)
atlasU   = s - row
atlasV   = (row + v) / RowCount
```

The `clamp` is what makes `u = 1` land at the end of the last row rather than the start of a row that
does not exist.

**Two properties this buys, both of which matter more than they look.**

- **An atlas texel stays isotropic in world terms on both axes.** `atlasU * AtlasWidth` is a texel
  along the track and `atlasV * AtlasHeight` is a texel across it, at the same centimetres per texel.
  So `PaintStrokes`, which draws a rotated quad of `(travel + width, width)` canvas pixels and
  assumes a straight line between two points is the right shape, **needs no change at all**. The
  fold lands entirely in `AppendStroke`.
- **`RowCount == 1` is the identity.** `s = u`, `row = 0`, `atlasU = u`, `atlasV = v`. The unfolded
  path is not a special case, it is the general case with one row, and every existing test that
  configures a small ribbon keeps its current meaning.

`NormalisedAlongTrack`, `NormalisedAcrossTrack` and `TrackToNormalised` keep meaning *ribbon* space
and are unchanged. The fold is a separate, separately named step applied after them, so the one test
that asserts on `TrackToNormalised` stays valid.

### The write path: the lap seam stops being a special case

`BuildStrokes` today splits a contact whose travel crosses the lap seam into two quads. Row
boundaries need exactly the same treatment, and **the lap seam is one of them** — it is the boundary
between the last row's end and the first row's start, so it coincides with a row boundary by
construction at exactly one place. Phase R said it was designing for this; this is the phase that
collects.

So the `if (bCrossesSeam)` branch is replaced by a walk. Boundaries sit at every multiple of
`TrackLength / RowCount`, including `0` and `TrackLength` themselves. The unwrapped span from the
stroke's start to its end is cut at every boundary it crosses, and each piece is emitted separately.
`Travel` is already clamped to one lap, so the walk is bounded at `RowCount + 1` pieces.

**The trap this design avoids by construction.** A piece that ends exactly on a boundary must map to
`atlasU = 1` of the row it is in, not `atlasU = 0` of the next one, and `floor` cannot tell those
apart. So the row is chosen **from the piece's midpoint**, which is strictly inside one row for any
piece of non-zero length, and both endpoints are then mapped within that known row. A stationary
contact sitting exactly on a boundary is the one ambiguous case, and either answer draws the same
dot with the same half clipped, which is what the lap seam already does today.

**A defect folding would otherwise introduce, and the fix.** `PaintStrokes` draws a quad half a brush
width longer than the travel at each end, so a contact at the track edge overhangs `v = 1` by half a
contact width. Today that falls off the canvas and is lost. Folded, row *r*'s overhang would land in
row *r+1*'s first across-texels — rubber from one part of the lap appearing at the far edge of a
different part. `FCanvas::SetRenderTargetScissorRect` (`CanvasTypes.h:423`, applied per pass at
`CanvasRender.h:21`) clips each stroke to its own row band and removes it exactly. It costs one call
per stroke and no memory.

That is affordable because `BeginDrawCanvasToRenderTarget` hardcodes `CDM_ImmediateDrawing`
(`KismetRenderingLibrary.cpp:788`) and `FCanvas::DrawItem` flushes after every item
(`Canvas.cpp:2084`), so **every stroke is already its own pass today.** There is no batching to lose,
either to the extra splits or to the scissor. With `RowCount == 1` the rect is the whole target and
nothing changes.

The stroke carries its own `RowIndex` rather than having the buffer infer it from the stroke's Y,
because a stroke lying exactly on `v = 1` of row *r* would infer row *r+1*.

### The read path: the fold is the shader's problem too, and it does its own bilinear

`MF_SampleRacetrackSurface` already does its sampling in **one** `MaterialExpressionCustom` node, a
3x3 tent named `BlurredExposure` taking `Tex`, `UV`, `TexelSize` and `Radius`. The fold goes inside
that same node. It gains one input, `Rows`.

The node's `UV` is the mesh's UV0, which `GenerateRoadMesh` writes as `(distance/lap, across)` — so
**the shader already receives a ribbon coordinate** and the tent is taken in ribbon space, with every
tap folded independently. A tap that steps past the end of a row therefore folds into the start of
the next one, which is the physically adjacent surface. The tent stops being a source of boundary
artefacts and becomes correct across them.

Two things the sampler would still get wrong, and what is done about each:

- **Across the track.** `atlasV = (row + v)/RowCount` puts `v = 0` and `v = 1` exactly on the
  boundary between two rows' bands, where a bilinear tap blends 50/50 with a neighbouring row's edge.
  Fixed by clamping `v` into the half-texel inset before folding, which reproduces clamp addressing
  within the band and costs one `clamp`.
- **Along the track.** At a row end the correct neighbour is in the next row, which is nowhere near
  it in the atlas. **Folding would regress something that is currently correct by accident**: the
  render target's `AddressX` defaults to `TA_Wrap` (`TextureDefines.h:498` makes `TA_Wrap` the first
  value), and on an unfolded ribbon wrapping past the end of the lap *is* the right neighbour,
  because the lap is a loop.

  So the shader does its own bilinear along the track: the along-track coordinate is taken to whole-
  lap texel index space, the two neighbouring texel indices are wrapped around the lap, each is
  folded into the atlas **separately**, each is sampled with its U snapped to the texel centre, and
  the two are blended by hand.

**Why snapping rather than a point sampler.** A bilinear sampler handed a UV at a texel centre
returns that texel exactly, weights 1 and 0, so snapping U gives an exact sample in X while leaving
the sampler's bilinear in Y — which is wanted, and correct, inside a band. It means the shader never
depends on the render target's `Filter` or `AddressX`, both of which are invisible from the material
and neither of which is currently set deliberately. `View.SharedPointClampedSampler` is reachable
from a Custom node and was the alternative; it was rejected because it would replace one invisible
dependency with another and would also kill the wanted bilinear in Y.

**The price, stated plainly:** two texture fetches per tap instead of one, so **18 per road pixel**
against 9. This is a measurement owed, not an estimate, and it is the first thing C9.M records.

`SurfaceBlurTexels` **keeps its default of 1.0.** At 3.125 cm per texel a 1-texel tent widens a
6.4-texel mark by about 31 %, and it is a real candidate for the smearing in the current captures —
but changing it in the same phase that changes what a boundary looks like would confound the only
check that matters, which is looking at the result. It is a parameter, so it is now an A/B with a
slider rather than a rebuild, and it is a judgement to be made on screen rather than derived here.

### Keeping the C++ and the HLSL from silently disagreeing

The mapping exists twice by necessity: `FSurfaceTrackRibbon` writes it and the Custom node reads it.
A silent disagreement puts rubber in the wrong place and nothing in the current test suite would
notice.

**One number crosses the boundary, not a formula.** `RowCount` is published to
`MPC_RacetrackSurface` as a scalar, by the same `PublishRibbonToMaterials` that already publishes
`SaturationConstant`. Everything else the shader needs it derives from `TMTM_TexelSize`, which is a
per-draw preshader constant read from the actual bound texture — so the atlas width and the
across-track texel count cannot disagree with the texture that is really there, because they are read
off it.

**The guard is a round trip through the real compiled shader, not a convention.** A stroke is painted
at a known ribbon coordinate through the C++ path; `DrawMaterialToRenderTarget` then renders the
material into a small target, which supplies UV 0..1 across it
(`KismetRenderingLibrary.cpp:220` draws the material as one full-target tile); that target is read
back with `ReadRenderTargetRawPixelArea`, which the stroke-width test already uses. **The mark has to
come back at the ribbon coordinate it was put at.** If the write side and the read side disagree
about the fold by so much as a row, the mark comes back somewhere else and the test fails.

Whether a material renders under the headless automation command is **not yet established** and is
the main risk in this phase. If it does not, the fallback is an editor-module test asserting the
asset's Custom node code matches a canonical string in the runtime header — which proves the asset
matches the repository but not that the HLSL matches the C++, and would be recorded as the weaker
guard it is.

**`RowCount` defaults to 0 in the collection**, matching the deliberate fail-loud default already
used for `SaturationConstant`. The shader takes `max(Rows, 1)`, so an unpublished row count makes a
folded ribbon read as though it were unfolded — which puts every mark at a quarter of its correct
along-track position, four times over. That is not subtle, and the failure mode it guards is exactly
the one C8 found: a configuration path that never runs.

### Not in this phase

- **The per-contact segment cache.** R2's trigger is tripped and the cost is measured at 0.3658 ms
  against 0.0087 ms. It is a world-to-track conversion change and has nothing to do with the fold.
  Doing it here would put two unrelated changes in one set of measurements.
- **Narrowing `ContactWidth` from 20 cm.** It costs texels per mark and would open a gap between
  wheel bands that currently abut at exactly 80.0 cm. It is a vehicle decision, not a storage one.
- **The four-tracks resolvability figure.** A longer lap makes the 5.1 % worse, not better, and that
  trade is real and still unfaced. C9.M measures it again at 1800 m so the number moves with the
  change rather than being carried stale.
- **`ElevationExaggeration`.** Stays at 2.5. A longer lap is what restores Spa's elevation, and this
  phase delivers 3.75 times the lap C8 left, which is the correct fix arriving by the correct route.

### Tests

Rewritten rather than deleted, because they are correct today and assert a model that is changing:

| Test | Becomes |
|---|---|
| `RacetrackSurface.Ribbon.RejectsTrackTooLongToFitUnfolded` | Asserts the row count steps at the right lap lengths and that both atlas dimensions stay under the cap, rather than that a long track is refused |
| `RacetrackSurface.Ribbon.TexelExtentsFollowTrackDimensions` | Same shape, against the new defaults |
| `RubberAccumDemo.Track.SpaLayoutFitsTheUnfoldedRibbon` | Asserts four rows, 14400 x 1024, 117964800 bytes and the margin under the cap |
| `RubberAccumDemo.Track.BothLayoutsFitTheCapAndTheMemoryBudget` | Asserts the *atlas* fits the cap and the committed figure is the one above, with the Stadium still unfolded at one row |

Added:

- A stroke that crosses a row boundary splits, and the pieces together span the travel.
- A stroke that crosses a row boundary **and** the lap seam at once, which is the last row's end
  joining the first row's start.
- The atlas mapping round-trips, including at `u = 0`, `u = 1` and exactly on every boundary.
- A stroke painted at a known ribbon coordinate reads back from the atlas at the predicted texel.
- The end-to-end round trip through the material described above.

*Exit: a lap long enough that no slow corner passes through itself, at a texel fine enough that a
20 cm contact patch is 6.4 texels; the buffer allocates at 14400 x 1024 for 117964800 bytes; a mark
crossing a row boundary is indistinguishable from one that does not; both build paths clean; all
tests passing headlessly with the editor closed; and a capture of the tightest corner showing the
road turning cleanly.*

### Measured on implementing, 2026-09-23. The arithmetic was a texel out and one prediction was backwards.

All 63 automation tests pass headlessly with the editor closed and both build paths are clean. Full
numbers in [MEASUREMENTS.md](MEASUREMENTS.md); what follows is only where this section was wrong.

**The atlas is 14401 x 1024, not 14400.** The Spa rescale lands a hair over 180000 cm, so `ceil`
gives 57601 along-track texels and each row carries one spare. **117972992 bytes, 112.51 MiB**, an
extra 8192 bytes. The exact-division argument above is still why the configuration was chosen — it
keeps the slack to one texel per row instead of hundreds — but the exactness does not survive a
measured rescale, and the byte assertion in the host test is a one-percent tolerance because of it.
The exact figure is still asserted against an exactly-constructed ribbon in the plugin's own test.

**Folding did not make four tracks worse, which is what this section inherited from C8 and repeated.**
Measured over a full settled lap the separation is **58.73 cm, 2.94 mark widths, resolvable over
7.7 % of the lap**, against C8's 56.76 cm, 2.84 and 5.1 %. The inherited reasoning was that
offtracking is `Wheelbase² / (2R)` and gentler corners reduce it, which is true of the steady-state
term and wrong about which term dominates — C8 had already found the turn-in transient is larger,
and that term scales with lateral rate rather than radius.

**What the same test found instead is a real loss.** The separation no longer falls away on
straights: on the straightest 1.5 % of the lap it is still 28.93 cm, 49 % of the corner maximum,
where at 480 m it was comfortably under half. At this lap length the axles are separated by
racing-line transitions rather than by centreline curvature, and the transitions are long enough to
cover most of what the cornering profile calls straight. The "two tracks on a straight" claim now
rests entirely on the Stadium, where there is genuinely straight track.

**The test had also stopped measuring.** `TicksAcrossTwoLaps = 6000` was sized for a 480 m lap and
sampled 773 ticks of the second lap at 1800 m. It now runs until the car has completed two laps and
samples 5240. **Both numbers above come from the fixed loop**; the figures the broken loop produced
were different and wrong.

**The road still passed through itself, and the lap length was not the reason.** Seen on screen at
1800 m, Campus still crumpled. The authored polyline there is fine — its tightest three-point radius
is 1066 cm against a 400 cm half width — but the Catmull-Rom through it dips to **167 cm**, because
the corner turns about 110 degrees across segments of 42 to 53 units and no cubic follows that
without bulging. Clamping the tangents was tried first and moves it from 167 cm to 164 cm, which is
nothing: the tangent at that point is already shorter than its neighbouring segments, so a clamp
never fires. **The authored Campus was opened up instead**, with one extra point so the spline has
the control to track the arc, and the tightest radius on the whole lap is now **455 cm at La
Source**, which is the genuine hairpin. Figures in [MEASUREMENTS.md](MEASUREMENTS.md).

`RubberAccumDemo.Track.RoadNeverPassesThroughItself` now encodes the exit condition numerically
rather than leaving it to a capture: it walks both road edges ring by ring and fails if either stops
advancing along the centreline, which is what the offset curve crossing its own centre of curvature
looks like.

**Measured cost.** The read path went from 9 texture fetches per road pixel to 18 and it cost
**0.6 % of GPU time**, 2.351 to 2.365 ms, because the scene is render-thread bound and always was.
The paint pass went from a 0.030 ms median to **0.210 ms**, seven times worse, while its p99.9 is
unchanged at 0.978 against 0.999 ms — and this phase cannot say how much of that is the per-stroke
scissor and how much is a render target 7.5 times the bytes, because no capture exists with four
rows and the scissor off.

**A console variable could not release the field for a capture and two command-line switches
replaced it.** `-ExecCmds` runs at frame 1, after `BeginPlay` has read the variable, so the first
frame captures measured a frozen grid and the one that did not only ran because a human pressed `P`.
`-ReleaseField` and `-FreezeField` are read through `FParse::Param` and do not depend on ordering.

**Not verified.** The 0.6 % GPU figure is not evidence that manual bilinear is cheap in general,
only that it is free in a render-thread-bound scene. No rubber has been judged on screen at this
configuration. The Stadium's inside edge advances 1.51 cm per 128 cm ring, a ratio of 1.01, which
is a pinch point rather than a fold and is left as found.

---

## C10 — The demo has to be legible in a recording, without narration

Added 2026-09-24, requested directly after looking at C9's surface and liking it. Everything in this
phase serves one goal: **someone watching a video of the demo should be able to tell what changed
and when, without a voiceover and without reading a log.**

Today every control writes to `UE_LOG` and nothing else. `ApplyTimeDilation` logs
`Time dilation now %.2fx`, `ApplyDepositionScale` logs its own line, `SetFieldPaused` logs the
release — and none of it is on screen. In a recording the dilation changes and the viewer has no way
to know it happened.

### C10.1 — Put every control change on screen

`GEngine->AddOnScreenDebugMessage` is the whole mechanism. It was explicitly chosen as the simplest
thing that gets text on screen; it does not need to be pretty and it does not need a HUD class.
Verified signature, `Engine.h:2196-2200`:

```
ENGINE_API void AddOnScreenDebugMessage(uint64 Key, float TimeToDisplay, FColor DisplayColor,
    const FString& DebugMessage, bool bNewerOnTop = true,
    const FVector2D& TextScale = FVector2D::UnitVector);
```

Every existing control announces itself: `P` for the field, `.` and `,` for time dilation, `]` and
`[` for the deposition scale, and the two controls this phase adds. Each message carries the value it
moved to, not just that it moved. **There is no on-screen text anywhere in the project today**, so
this is greenfield.

#### Decided: every message is keyed, and the key is a hash of a name rather than a small integer

`AddOnScreenDebugMessage` keeps two stores (`UnrealEngine.cpp:12466-12523`). `Key == (uint64)-1`
appends to `PriorityScreenMessages` and never replaces anything; any other key indexes a
`TMap<int32, FScreenMessageString>` and **overwrites that entry in place**. Unkeyed is wrong here:
holding `]` for six steps would leave six deposition lines stacked on screen, and a viewer cannot
tell which is current. So each control owns a key and its line updates in place.

That makes key collision a real failure: the map is engine-wide, so two features choosing the same
small integer silently overwrite each other's line, which is precisely the illegibility this phase
exists to remove. Keys are therefore `GetTypeHash(FName(TEXT("RacetrackSurface.DecayRate")))` and so
on — self-describing at the call site and collision-free without either module knowing the other's
key list. `UKismetSystemLibrary::PrintString` derives its own key the same way
(`KismetSystemLibrary.cpp:529-534`), so this is the engine's own idiom rather than an invention.

#### Decided: ownership follows who can observe the change, not who owns the key

The circuit owns the keys and the plugin owns the console variables, so neither module can announce
everything. Splitting it by *who observes the value changing* resolves it without either module
reaching into the other:

| Value | Changed by | Announced from |
|---|---|---|
| Field frozen or released | `SetFieldPaused` | host, beside the existing `UE_LOG` |
| Time dilation | `SetGlobalTimeDilation` | host, beside the existing `UE_LOG` |
| Debug channel view | a host-only material parameter | host, beside the existing `UE_LOG` |
| Deposition scale | `r.RacetrackSurface.DepositionScale` | **plugin, from the variable's own change callback** |
| Decay rate | `r.RacetrackSurface.DecayRate` | **plugin, from the variable's own change callback** |

The two console-variable rows are the interesting half. `IConsoleVariable::SetOnChangedCallback`
(`IConsoleManager.h:687`) fires on every `Set`, so the announcement is attached to the value rather
than to the key that moved it — and the message then appears whether the change came from a key
press, from the in-game console, or from `-ExecCmds`. The host's `ApplyDepositionScale` deliberately
gets **no** on-screen call of its own, because it already reaches the value through `Set` and would
otherwise announce twice.

`TAutoConsoleVariable` has a public constructor taking that delegate directly
(`IConsoleManager.h:2045`), so this costs one extra constructor argument on a variable that already
exists, and the callback is a free function in the anonymous namespace that already holds
`CVarDepositionScale`. **Nothing new is exported. No new `RACETRACKSURFACE_API` symbol.** The plugin
gets no on-screen *presence* — it gets on-screen *announcements for the two values it owns*, which
is the narrowest thing that is correct.

**Rejected: exporting a shared `AnnounceOnScreen` helper from the plugin** for the host to call too.
Recording scaffolding is not part of "contact data in, surface state out", and a host that is not
this demo would inherit a symbol that has nothing to do with surface state.

**Rejected: `RegisterConsoleVariableSink_Handle`**, which the engine's own comment at
`IConsoleManager.h:680` recommends over `SetOnChangedCallback`. A sink is called with no arguments
(`CallAllConsoleVariableSinks`), so it cannot say which variable changed or to what, and using it
would mean caching and diffing every value every frame. The engine's caution is specifically about
setting *other* console variables from inside the callback and cascading; this callback only formats
a string, so the hazard it warns about does not arise.

**Rejected: `UKismetSystemLibrary::PrintString`.** It always writes a log line as well
(`KismetSystemLibrary.cpp:458-538`) and prefixes the text with the PIE net mode and, under
`DO_BLUEPRINT_GUARD`, a Blueprint name. Every handler here already has its own precisely worded
`UE_LOG`, so this would either duplicate it or fight it.

**One consequence to record rather than fix.** The whole mechanism is compiled out under
`#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)` (`UnrealEngine.cpp:12468`), and
`UEngine::bEnableOnScreenDebugMessages` can switch it off at runtime. Every build this project makes
is Development, so this is not a limitation today, and it is the correct behaviour for a plugin that
might one day ship: the announcements cost a shipping game nothing because they do not exist in one.

### C10.2 — Look at one accumulation channel at a time

**The sketch this section replaces was wrong about what the material does today, and the correction
changes what the task is.** `MF_SampleRacetrackSurface`'s `Coverage` output is a **Vector3, not a
sum** — R is buildup, G is skid, B is marbles, and `M_SurfaceAccumulationDebug` drives it straight
into `MP_EmissiveColor` (`EDITOR_SETUP.md:105-113`, `:152-163`). So the debug view is already colour
coded and a viewer *can* already tell rubber from a skid mark by hue.

What a viewer cannot do is see one channel on its own. Rubber and skid overlapping read as yellow,
and a faint channel sitting under a strong one is invisible. So this task is **isolation**, not
colour coding, and it is a smaller change than the sketch assumed.

#### Decided: a vector mask in the host material, not a branch, and not in the plugin's function

The selector is one `VectorParameter` named `DebugChannelMask`, multiplied into `Coverage` before
`MP_EmissiveColor`. The four views are four values of that parameter:

| View | Mask | Reads as |
|---|---|---|
| All channels | `(1, 1, 1)` | today's image, unchanged |
| Rubber buildup | `(1, 0, 0)` | red |
| Skid marks | `(0, 1, 0)` | green |
| Marbles | `(0, 0, 1)` | black, and that is correct |

**Which asset carries it is the decision that matters, and only one answer keeps the plugin's
function reusable.** `MF_SampleRacetrackSurface` is the seam a host integrates against, and it
already offers everything a host could want: `Coverage` plus `Buildup`, `Skid` and `Marbles` as
separate scalars. Which of those a host shows, and whether it cycles between them, is that host's
business. Putting a debug-view selector inside the function would hand a parameter that means
nothing outside this demo to every future integrator. `M_SurfaceAccumulationDebug` lives in `/Game`,
is host content, and exists for exactly this. **The mask goes there and the plugin's function is not
touched at all**, which also avoids the one asset where `MaterialTools.delete_expression` is known to
halt the editor.

**Why a mask multiply and not a branch.** An `If` chain over the four outputs would work, but the
mask has a property the chain does not: **the all-channels view is a multiply by `(1,1,1)`, which is
provably the image C9 captured**, not merely an image that should match it. Every capture in
`MEASUREMENTS.md` was taken through the unmasked path, and that comparison is worth keeping exact.
The mask is also three added nodes against five, evaluates the existing sample once rather than
inviting the four outputs to be evaluated separately, and needs no branching at all.

**The mask is the one the paint path already uses.** `USurfaceAccumulationBuffer::ChannelMask`
(`SurfaceAccumulationBuffer.cpp:162-175`) already maps `EAccumulationChannel` to the
`FLinearColor` a stroke is written with. The view reuses that same function rather than repeating the
table, so **the channel the view isolates cannot disagree with the channel the write targets** — if
one is ever changed the other follows. That is the only reason this phase adds
`RACETRACKSURFACE_API` to anything: the one static function now genuinely crosses the module
boundary. The all-channels case is `FLinearColor::White`, which is not a channel and stays host-side.

**Marbles will be black and that is correct.** `ASplineFollowingCar::ChannelAtWheel` returns
`SkidMark` for a locked wheel and `RubberBuildup` otherwise; nothing writes `Marbles` before phase G.
The on-screen message names the channel so an empty view reads as *nothing here yet* rather than as
a bug.

**Rejected: four material instances swapped with `SetMaterial`.** Four assets that do not exist,
four texture bindings to keep pointed at the live accumulation target, and a material swap per
keypress, for the same image a single `SetVectorParameterValue` on the MID the circuit already
creates produces.

**Rejected: a `StaticSwitchParameter`.** Changing one compiles a new shader permutation on first
use. That is the wrong tool for a value driven by a keypress during play.

**The C++ has to work before the asset does, and this session cannot author the asset.** The editor
is not running and nothing is listening on the MCP port, so the material edit is a separate,
later step. `UMaterialInstanceDynamic::SetVectorParameterValue`
(`MaterialInstanceDynamic.h:109`) writes into the instance's own parameter array and does not
validate the name against the parent, so setting `DebugChannelMask` before the parameter exists is a
silent no-op: the cycle logs and announces correctly and the image simply does not change yet. That
is the intended degradation, and it is also the reason the parameter's default must be `(1,1,1,1)` —
an absent or unset parameter has to mean *show everything*, never *show nothing*.

### C10.3 — Decay, and a rate control from 0 to 20x

**Decay does not run.** `USurfaceAccumulationBuffer::ApplyDecay(WorldContext, Retention)` exists,
is measured in `SurfaceBudgetTests` and `SurfaceBufferTimingTests`, and **has no production caller
at all**. Nothing in the demo has ever faded. The racing line that C9 measured is a pure
accumulation, which is why `MEASUREMENTS.md` records it as *"a transient, most legible somewhere
between"* two lap counts — it only ever gets darker.

So this is not a keybind on an existing feature. It is **phase E's coupling arriving early**, and it
has to answer four questions the sketch left open: what clock decay runs on, what a rate of 1x means,
how often the pass runs, and how 0x is guaranteed to reproduce C9.

#### Decided: decay runs on dilated world time, the same clock deposition already runs on

`USurfaceAccumulationSubsystem` is a `UTickableWorldSubsystem`, and `UWorld::Tick` does
`DeltaSeconds *= Info->GetEffectiveTimeDilation()` at `LevelTick.cpp:1596` and then passes that same
`DeltaSeconds` to `FTickableGameObject::TickObjects` at `LevelTick.cpp:1821`, so the `DeltaTime` that
`FlushPendingContacts` already uses is dilated. The demo confirms it from the other end: C3 recorded
a stroke of about 330 cm at 20x and 100 fps, which is 1650 cm/s x 0.01 s x 20.

Decay must ride the same clock, and the argument is the one C3 already made for deposition. A lap
has to lay the same rubber whether you watch it at 1x or 20x; equally, **a lap has to lose the same
rubber**. If decay ran on real time, fast-forwarding a capture would change the ratio of deposit to
decay and therefore change the coverage a texel settles at, purely as an artefact of playback speed.
Keeping both on the dilated clock makes the equilibrium a property of the track and the traffic
rather than of how fast you are watching.

**Consequence, recorded rather than fixed:** decay keeps running while the field is frozen with `P`.
C7 already established that freezing the field is not pausing the game — the subsystem keeps ticking
and real and dilated time both keep advancing — and it deliberately did not gate the time and
deposition keys on the frozen state either. Decay follows the same rule. At the derived half-life the
loss over a few seconds of inspection is far below what the buffer can represent, and an operator who
wants the surface held exactly still has 0x.

#### Decided: the rate multiplies the decay constant, not the half-life, so 0x is a clean off

The primitive is a multiply by a retention factor, clamped to `[0,1]`, applied to the whole target
through `BLEND_Modulate`. A half-life `H` gives a decay constant `lambda = ln2 / H`, and retention
over an elapsed time `t` is `exp(-lambda * t)`. The runtime control scales `lambda`:

```
Retention(t, Rate) = exp(-(ln2 / H) * Rate * t) = 2^(-Rate * t / H)
```

Scaling `lambda` rather than `H` is what makes 0x well defined. `H / Rate` at `Rate = 0` is a
division by zero, and `H * Rate` at `Rate = 0` is a half-life of zero, which is *infinitely fast*
decay — the buffer vanishing on the next application, the exact opposite of what 0x has to mean.
Scaling the constant gives `lambda = 0` and a retention of exactly 1 for any elapsed time.

Composing across frames is exact: the product of `2^(-Rate*dt_i/H)` over a set of frames is
`2^(-Rate*sum(dt_i)/H)`, which depends only on the total elapsed time. So the result is frame-rate
independent by construction rather than by tuning, which is the property C0 established for
deposition and the reason the sketch ruled out a per-frame retention constant.

#### Decided: 0x means the pass does not run at all, and that is a test

A retention of exactly 1.0 is not good enough. It still costs a
`BeginDrawCanvasToRenderTarget` / `K2_DrawTexture` / `EndDrawCanvasToRenderTarget` round trip over
112.50 MiB, it still pushes every texel through the canvas blend pipeline, and C9 measured a frame
that did none of that. So `Rate <= 0` returns before touching the accumulator or the buffer, and the
test asserts the *absence of the call*, not the value it would have passed.

**The rate therefore defaults to 0**, and three independent reasons agree on that:

1. **Every figure in `MEASUREMENTS.md` was taken with nothing fading.** Defaulting to 0 makes the
   default configuration the documented baseline, so no past number is quietly invalidated and no
   future measurement is accidentally taken against a different surface model.
2. **One existing test already ticks the subsystem.** `SurfaceReadPathTests.cpp:160` calls
   `Subsystem->Tick(1.0f)`. A non-zero default would put a decay pass inside a test written before
   decay existed. At 0 the 64 tests keep measuring what they measured.
3. **Turning decay on during a recording is more legible than having it always on.** The viewer sees
   the announcement, then sees the fade start. A surface that has always been fading shows an effect
   with no visible cause, which is the thing this phase exists to prevent.

`-ExecCmds` is sufficient to force 0 or any other value for a capture, unlike `bStartPaused` in C9:
that failed because `BeginPlay` reads it at frame 0, whereas the first decay pass cannot happen until
a measurable amount of track time has been banked.

#### Decided: the pass runs on a fixed amount of decay, not on a fixed interval in seconds

Per-frame is ruled out twice over, once on cost and once on precision, and the precision argument is
the one that also rules out the obvious fix.

**Cost.** Extrapolating the B4 sweep — 2048 square RGBA16f, 33554432 bytes, 0.2400 ms — linearly in
bytes touched, which is B4's own stated reading, puts a full-buffer pass at 117964800 bytes at about
**0.845 ms**. Against a measured 2.365 ms GPU and 4.725 ms frame, that is 36 % of GPU time every
frame for a change no single frame can show. *This is an extrapolation from a square buffer to a
14:1 folded atlas and is not a measurement. Taking it is the first task below.*

**Precision, which is the sharper constraint.** The target is RGBA16f. A half float carries an
11-bit significand, so one unit in the last place is between `2^-11` and `2^-10` of the value, and
round-to-nearest discards any multiply moving a value by less than half of that. Half an ulp is
between `2^-12` and `2^-11` relative depending on where in the binade the value sits, so
**`2^-11 = 4.883e-4` is the perturbation that is guaranteed to change every value**, and anything
below `2^-12 = 2.441e-4` is guaranteed to change none. At a half-life of about 1500 s, one frame at
60 Hz has a retention of `1 - 7.7e-6` — **63 times below even the easy threshold, so it rounds back
to the same bits and the decay pass does nothing at all, every frame, while still costing 0.845 ms.**
Silently.

A fixed interval in seconds does not fix this, and that is why the sketch's phrasing is replaced. At
1x and a 1500 s half-life, one second of retention is `1 - 4.62e-4`, which is *below* the `4.883e-4`
that guarantees a change — so a one-second interval at 1x moves some texels and rounds away on
others, depending on where each value happens to sit. An interval short enough to look continuous at
20x is too short to resolve at 1x, and one long enough to resolve at 1x visibly stair-steps at 20x.
**No single interval in seconds works across the rate range.**

So the interval is expressed in decay rather than in time. The subsystem banks dimensionless progress
and applies a pass when it crosses one step:

```
Progress   += Rate * DeltaSeconds / H
apply when  Progress >= 1 / StepsPerHalfLife
Retention   = 2^(-Progress)      then Progress = 0
```

At a default of 256 steps per half-life the retention per application is `2^(-1/256) = 0.997296`, a
move of `2.70e-3`, which clears the `4.883e-4` guaranteed-change threshold by 5.5 times and is
between 2.8 and 5.5 units in the last place everywhere in the normal range. **The retention factor
is then a constant, independent of rate and of frame rate** — every
application does the same amount of work, and only how often it runs changes. That makes the cost
model trivial: `Rate * StepsPerHalfLife / H` passes per dilated second, so 0.162 per second at 1x
against 3.24 at 20x, or **0.0023 ms per frame at 1x and 60 Hz**.

Using the actual banked progress in the exponent rather than the nominal step keeps a hitch or a
frame-time spike exact rather than approximated, for the same reason C0 derived deposition from
distance actually swept rather than from an assumed cadence.

`StepsPerHalfLife` is the cost knob phase E2 already called for — *"decay pass over real time,
frequency as a cost knob"* — arriving here with a measurement behind it rather than a slider.

#### The half-life is a demo calibration, and it is derived, not chosen

`H` lives beside `SaturationConstant` and `ExposurePerUnitSlip` in `SurfaceAccumulationMapping.h`,
as a constant rather than a console variable, for the same reason `k = 3.0` is one: it is the
contract the rest of the system is derived against, and a second copy that can drift is the failure
mode that file exists to prevent.

Its value is derived from the equilibrium the demo should settle at. A texel under a steady deposit
`D` of exposure per dilated second approaches `E = D / lambda`, so asking the apex to settle at an
exposure of 1.0 — which is 0.95 coverage through `1 - exp(-3E)` — fixes the half-life:

```
H = ln2 * LapSeconds / ApexExposurePerLap
```

`ApexExposurePerLap` is `CarCount * WheelsSharingATrack * SlipToExposure(CorneringSlipAmount, 1)`,
which is `20 * 2 * 0.001 = 0.04` at the current defaults. `LapSeconds` is **not yet measured** and
setting the constant is the second task below.

**This is worth more than a fade.** `MEASUREMENTS.md` records the racing line as a transient with no
settled state — it only ever gets darker, so there is a best moment to look at it and no equilibrium
to measure. With decay there is one, and the apex-to-straight contrast becomes a steady-state
property of the traffic rather than a function of how long the capture ran. **That is the first time
this project can state a racing line as a number that does not depend on when you looked.**

**Stated plainly, because it is the obvious attack:** the half-life is calibrated against this
demo's own field, so it is demo tuning in the same sense `GridSpacing` is. What is *not* tuning is
the shape — dilated clock, rate on the constant, exact frame-rate independence, 0x as a true skip.
A host with different traffic must retune the half-life, which is why the circuit logs the
equilibrium its own field implies on every run rather than leaving it in this document.

#### Not in this phase

- **E5's coupling.** `ApplyDecay` multiplies the whole target by one factor, so every channel decays
  at the same rate. Making skid decay a function of local buildup needs a different pass and is E5.
- **Decay applied per region or per row.** The pass is the whole atlas. Nothing needs less yet.
- **Retuning `StepsPerHalfLife` from the measured cost.** The default is set by the precision floor,
  which is the binding constraint; the measurement below only has to confirm the cost is not also
  binding.

### Tasks

| # | Task | Exit |
|---|---|---|
| C10.0 | Measure the full-buffer decay pass at the real 14400 x 1024 RGBA16f atlas | A number in `MEASUREMENTS.md` replacing the 0.845 ms extrapolation |
| C10.1 | On-screen announcement for every control, keyed, with the two console variables announcing from their own change callback | Pressing any control changes a line on screen that carries the new value |
| C10.2 | `DebugChannelMask` on `M_SurfaceAccumulationDebug`, cycled by a key, reusing `ChannelMask` | Each channel can be viewed alone; the all-channels view is unchanged from C9 |
| C10.3 | The decay pass, its progress accumulator, and the rate and step console variables | A surface that fades, at a rate that is exactly frame-rate independent |
| C10.4 | Measure the lap time and set the half-life constant from it | `H` recorded with the lap time it came from |
| C10.5 | Capture the recording: every announcement, each channel in turn, decay off then on | Files in `assets/media/`, listed in `MEASUREMENTS.md` |
| C10.6 | Console commands for the host controls. Added 2026-09-24, see below | `-ExecCmds` reaches every control, so a capture needs no keyboard |

### Tests

Added:

- The retention formula is exactly frame-rate independent: 600 steps of 1/60 s and 1 step of 10 s
  give the same retention to float tolerance.
- A rate of 0 returns a retention of exactly 1.0 for any elapsed time, and — separately, because it
  is the stronger claim — **ticking the subsystem at rate 0 for a simulated hour issues no decay
  call at all.** Asserted by the buffer contents being bit-identical, since the call itself is not
  observable from outside.
- The retention at one step is resolvable in half-float precision: `1 - 2^(-1/StepsPerHalfLife)`
  exceeds the `2^-11` guaranteed-change threshold with margin. This is the assertion that fails if
  anyone later raises `StepsPerHalfLife` past the point where decay silently stops working.
- Progress accumulates and the pass fires the expected number of times over a known span at a known
  rate, and the product of the retentions it applied equals the closed-form retention over that span.
- The debug view cycles All, Rubber, Skid, Marbles and wraps, and each view's mask is the one
  `USurfaceAccumulationBuffer::ChannelMask` gives for that channel, with All exactly `(1,1,1)`.
- The decay rate control reaches 0 from the slowest non-zero rate and returns to it, and clamps at
  20x.

Rewritten: none. Nothing existing asserts a model this phase changes, which is itself the point of
defaulting the rate to 0.

### Corrected 2026-09-24 after trying the keys: both decay keys were wrong, for two different reasons

C10.1 shipped decay on `'` and `;`. **Neither worked**, and the two faults are worth separating
because only one of them is the kind a careful reading would have caught.

**`;` collided with an engine default.** `BaseInput.ini:54` binds `Semicolon` to
`ToggleDebugCamera`. `DebugExecBindings` are dispatched separately from an actor's `InputComponent`,
so both fired: the debug camera toggled *and* the handler ran. The debug camera then took and
released input focus on alternate presses, which is why the message appeared every second time and
looked intermittent rather than wrong.

**`'` was never delivered at all.** `EKeys::Apostrophe` is a registered key with the display string
`'` (`InputCoreTypes.cpp:666`), so it compiles and reads correctly at the call site, but the string
`Apostrophe` appears **nowhere under `Runtime/ApplicationCore/`** — the Windows layer never produces
that key event. Pressing it produced no log line of any kind, which is the observation that settles
it. *The registration and the absence are verified; the platform key-map path was not traced, so the
mechanism is inferred rather than proven.*

**Remapped to `M` faster and `N` slower**, checked against `BaseInput.ini`'s `DebugExecBindings` and
against the project's own bindings before choosing. The confidence that they are delivered is not a
grep: `P` and `C` are bound by this same code and demonstrably work, and plain letter keys go
through the same path.

**The rule this leaves.** A key is not chosen by picking a plausible `EKeys::` entry. It is checked
against `Engine/Config/BaseInput.ini` for a `DebugExecBindings` collision, and against something
already known to be delivered on this platform. `EKeys::` compiling proves neither.

**Decay itself was never broken**, and the distinction matters for the record: the console path
drove it end to end for the C10.5 captures, and `c10-decay-off` against `c10-decay-on-20x` is the
same run at rate 0 and rate 20. The rate simply never left 0, because the key that raises it was
dead and the key that lowers it already returns 0 from 0.

### Traps carried into this phase

- **New `UPROPERTY` members and new reflection do not survive Live Coding.** This phase adds a stored
  `UMaterialInstanceDynamic` on the circuit and new console variables. The editor must be closed and
  a real build run, exactly as `EDITOR_SETUP.md` records for C9.
- **The material edit cannot happen this session.** The editor is not running and nothing is
  listening on the MCP port. The C++ lands first and is correct without the asset; the asset edit is
  its own step, its own approval and its own `EDITOR_SETUP.md` entry.
- **Additive material edits only.** `MaterialTools.delete_expression` halts the editor. The mask is
  three added nodes plus one rewired destination pin.
- **A key binding is not verifiable headlessly.** No automation test has a `PlayerController`, so
  nothing in the suite exercises a key. Check the binding against `BaseInput.ini` before choosing it
  and press it once on a real run; the build passing says nothing about it.

*Exit: a recording of the demo shows, on screen, every change the operator makes; the channels can be
inspected one at a time; decay runs at a rate that is exactly frame-rate independent, and taking it
to zero provably does not run the pass at all, so the C9 figures stand.*

### Measured on implementing, 2026-09-24. One prediction was 2.5 times out and one decision moved modules.

C10.0 to C10.4 are done. **74 of 74 tests pass headlessly with the editor closed and both build paths
are clean.** C10.2's C++ is done and its material edit is not; C10.5 has not happened. Full numbers
in [MEASUREMENTS.md](MEASUREMENTS.md); what follows is only where this section was wrong.

**The decay pass costs 0.3340 ms at 14400 x 1024, not the 0.845 ms extrapolated above.** B4's
"decay cost tracks bytes touched" does not hold from 33554432 to 117964800 bytes: 3.52 times the
bytes cost 1.39 times the time. **That weakens the cost half of the case for not decaying every
frame and it should not be restated as written.** 0.34 ms is 7 % of a 4.725 ms frame — expensive,
not disqualifying. What survives untouched is the precision argument, which is not a matter of
degree: a per-frame pass at 60 Hz moves a texel by 1.281e-5, **38 times below the 4.883e-4 that a
half float is guaranteed to notice**, so it would cost its full price and change nothing at all.
Measured, not argued.

**The half-life moved out of the plugin and is held in laps, not seconds.** The section above put
`DecayHalfLifeSeconds` in `SurfaceAccumulationMapping.h` beside `SaturationConstant`. That is wrong,
and measuring lap time is what showed it: **56 laps is 822 s on the Stadium and 4505 s on Spa**, so
no single constant in seconds is right for both layouts, and the demo would either never build a
line on one or erase it on the other.

The fix is better than the original and is the correct ownership:

- The plugin keeps the mechanism and a `FallbackDecayHalfLifeSeconds` that is only ever used by a
  host that configures nothing.
- `USurfaceAccumulationSubsystem::SetDecayHalfLifeSeconds` is the one new exported setter, called
  from `ARacetrackCircuit::PublishTrackRibbon` beside the ribbon it already publishes.
- `ARacetrackCircuit` owns `DecayHalfLifeInLaps` and derives seconds from `GetLapSeconds()`, which
  integrates its own cornering profile against the car's speed parameters.

**And the equilibrium justifies it rather than merely permitting it.** A texel under a steady
deposit settles at `E = ApexExposurePerLap * HalfLifeInLaps / ln2` — **lap time cancels out
entirely**. Expressed in laps the calibration is a property of the traffic and transfers between
layouts unchanged; expressed in seconds it does not. 56 laps is `ln2 / 0.012471` from the measured
apex deposit, and it settles the apex at **0.9513 coverage against the straight's 0.3074, an
equilibrium contrast of 3.09 to 1**.

**`GetLapSeconds` runs 7.5 % fast on both layouts** because it assumes a car is at its target speed
everywhere and ignores the bounded braking and acceleration rates. Stated rather than corrected: the
bias is consistent, it is a calibration input, and the test asserts the prediction against a ticked
lap within 15 %.

**The zero-rate claim got stronger than planned.** The Tests section above proposed asserting
bit-identical buffer contents, because the absence of a call is not observable from outside. A
`GetDecayPassCount()` accessor makes it directly observable instead, so the test asserts **zero
passes over a simulated hour** rather than inferring it. One accessor for a much clearer assertion.

**"Rewritten: none" did not survive.** `RubberAccumDemo.Track.CountedPassesOverATexelAgainstThePrediction`
gained the equilibrium assertions, because it is where the measured apex deposit already lives and
duplicating that machinery to assert against it would have been worse.

**A provenance defect in `MEASUREMENTS.md` was found on the way and is recorded, not fixed.** The
8.2:1 contrast, the 0.012471 apex figure and the laps-to-63 % counts were all measured on the
**Stadium**, and the table does not say so. `CountedPassesOverATexelAgainstThePrediction` spawns a
default circuit, and `Layout` defaults to `Stadium`; **17 of the 23 host tests that spawn a circuit
never call `SetLayout`**, so they run the oval. The equilibrium arithmetic is layout-independent so it is unaffected, but **re-measuring the
contrast on the demo layout is owed** and is not part of C10.

**C10.2's material edit landed 2026-09-24 once the editor came up.** `DebugChannelMask` is a
`VectorParameter` defaulting to `(1,1,1,1)`, masked to RGB and multiplied into `Coverage` before
`MP_EmissiveColor`. Three nodes added, nothing deleted, the plugin's function untouched. Logged in
[EDITOR_SETUP.md](EDITOR_SETUP.md) by path.

### C10.6 — Console commands for the host controls, added 2026-09-24 because the capture needs them

Not in the plan above. Added on discovering that **C10.5 cannot be driven at all** otherwise.

Unreal MCP exposes PIE control and screenshots but **no key input and no console-command execution**
— `EditorAppToolset` still offers `SearchCVars` for lookup only, which is the same limitation B3
recorded on 2026-09-22. The demo starts with the field frozen, decay at 0 and the view on all
channels, and each of those moves only on a key press. So through MCP the demo can be started and
photographed and nothing else, and every capture would be a human pressing keys.

That collides directly with this project's own method rule: *"Where MCP can run a measurement, run it
rather than handing me a placeholder."* The enabling change is small: **every host control gets an
`FAutoConsoleCommand` alongside its key**, so the same handler is reachable from `-ExecCmds`, from
the in-game console and from a key.

This costs nothing in design terms and repays more than the capture:

- The deposition scale is already a console variable and was already drivable this way. This makes
  the field freeze, the time dilation, the decay rate and the channel view consistent with it
  instead of being the odd ones out.
- C9 needed two command-line switches, `-ReleaseField` and `-FreezeField`, precisely because
  `bStartPaused` is read in `BeginPlay` before `-ExecCmds` runs at frame 1. A console command does
  not have that problem for anything set *after* `BeginPlay`, which is all of these.
- Every future capture and every future demo state becomes reproducible from one command line
  rather than from a remembered sequence of keypresses.

**The keys stay.** The commands call the same handlers, so there is one implementation and two ways
in, and the on-screen announcement fires either way because it is attached to the value rather than
to the key.

*Exit for C10.6: `-ExecCmds` can reach the field, the dilation, the decay rate and the channel view,
and a capture needs no keyboard.*

---

## D7 — A lockup writes a full mark and a repeat lockup refreshes it

Requested 2026-09-24 after looking at `c10-view-skid-marks`. **Not implemented. Not started.**
Recorded here with the constraint measured, because it is a change to the write path and belongs to
phase D rather than to C10.

The proposal: a locked wheel should take the skid channel straight to full coverage, and a second
lockup over the same texel should *refresh* that mark rather than deepen it, while decay continues
to act on it normally. The physical argument is good — a lockup deposits its mark in one pass, and
two cars locking up in the same braking zone do not lay a mark that is twice as dark or that lasts
twice as long.

**This splits into a cheap half and an expensive half, and only the cheap half is a tuning change.**

### The cheap half: one lockup, one full mark

Today a locked wheel reports `LockedWheelSlipAmount = 1.0` and that goes through
`SlipToExposure(Slip, Scale) = Slip * 0.002 * Scale`, so one contact deposits **0.002 exposure,
which is 0.6 % coverage**. The marks in `c10-view-skid-marks` are only visible at all because that
capture ran at 32x deposition.

Making a lockup saturate is a constant and a branch: the locked case stops going through
`ExposurePerUnitSlip` and takes its own exposure, around 1.0 for 0.95 coverage or 3.0 for 0.9999.
That is the same shape of derivation `SaturationConstant` already has.

### The expensive half: refresh rather than accumulate

This needs `dst = max(dst, value)` on one channel, and **the paint path cannot express that**.
Measured against the engine source rather than assumed:

- `FCanvas`'s `EBlendMode` maps to `ESimpleElementBlendMode` at `Canvas.cpp:1602-1615`, and the
  available set is Opaque, Masked, Additive, Modulate, AlphaComposite, AlphaHoldout and
  AlphaBlend/Translucent. **There is no max blend and no screen blend.**
- `BLEND_Translucent`, which `ESurfacePaintMode::Saturating` already uses, writes all of RGB. A skid
  stroke drawn with mask `(0,1,0)` and alpha 1 sets G to 1 *and multiplies R and B by zero*, so it
  would erase the accumulated rubber and any marbles along the length of every skid mark. That is
  worse than the problem it solves.

So the options, none of them a tuning change:

| Option | Cost |
|---|---|
| A separate single-channel target for skid, written opaque | Reverses B6's one-target decision, adds about 28 MiB at R16F, and the material samples twice |
| A small custom pass for skid using the RHI's `BO_Max` blend op | Real rendering work. C0 rejected a hand-rolled RHI path, but it rejected it *for batching*, where the canvas already did the job correctly. Here the canvas genuinely cannot, so that rejection does not carry over |
| Clamp the skid channel at read time instead | **Rejected.** It makes an over-accumulated mark *look* right while still taking an extra half-life to fade, which is the opposite of refreshing |

### Decided: measure before building the expensive half

The cheap half is worth doing on its own and is visible immediately. Whether the expensive half is
needed at all depends on a number nobody has: **how often a second lockup lands on a texel that
already carries one.** If repeat lockups on the same texel are rare, additive accumulation past full
is invisible and only costs a slightly longer fade on a few texels; if they cluster tightly in the
braking zones, it matters and the max write has to be built.

`RubberAccumDemo.Track.CountedPassesOverATexelAgainstThePrediction` already counts passes over the
busiest texel and is the natural place to count lockups the same way. That measurement comes first.

*Exit: a lockup mark reads as full on the frame it is laid, and the decision on refresh-versus-
accumulate is taken on a counted figure rather than on appearance.*

---

## Standing rules for this backlog

- A phase is not complete until its exit condition has been demonstrated, and what was *not*
  verified is stated.
- Every measurement goes in `docs/MEASUREMENTS.md` with the build configuration it came from.
  Development or Shipping, never editor, because editor frame-rate smoothing hides the spikes.
- Every editor-side change goes in `docs/EDITOR_SETUP.md`.
- Learning log evidence is added as work happens, not in week 8.
