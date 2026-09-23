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

| # | Task | Exit |
|---|---|---|
| G1 | Cellular-noise clumping, threshold onset rather than linear | Clumps, not a gradient |
| G2 | Placement offset from travel path, density driven by buildup in adjacent cells | Band on the outside of corners, no authored line |
| G3 | Clearing when cars drive through | Repeated passes open a path |
| G4 | Channel wired into the query path | Per-wheel readout changes as a car crosses the band |

*Exit: correct spatial distribution as flat texture variation, plus a working readout.*

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

1. **Collapse the baseline spread.** `DriverLateralSpread` shrinks so the field runs close to a
   single line on the straights. It is not removed — see the emergence note below.
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
   it. The result is clamped so the deviated line stays inside the ribbon with margin.

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

**The saturation timescale changes shape.** `ReportLapsToSaturation` assumes twenty cars cross one
apex texel per lap, which stays roughly true. But the straights, currently fanned across many
texels, now concentrate on a few — so the straights will saturate far faster than they do today and
the 17-and-50-lap figures will describe the whole track rather than just the apex. Re-read the
log line after the change rather than assuming it carries over.

### Tests

- **Determinism**: two runs with the same seed produce identical lateral offsets at the same
  distances. Without this the rest of the harness is no longer reproducible.
- **Distribution**: over many corner entries, the deviations have a mode near zero and a spread
  inside the clamp. Asserting the *shape*, not a specific sequence.
- **Held through the corner**: the deviation does not change between corner entry and exit.
- The existing `DriversTakeDifferentLinesThroughACorner` will need revisiting — it asserts a
  committed driver crosses to the inside from a wide baseline, and the baselines are about to stop
  being wide.

*Exit: twenty cars run visibly nose-to-tail on the straights and fan into distinguishable arcs
through a corner, the same seed reproduces the same run, and the accumulated buffer shows one
dominant line with separable arcs around it at a corner rather than a single smear.*

---

## Standing rules for this backlog

- A phase is not complete until its exit condition has been demonstrated, and what was *not*
  verified is stated.
- Every measurement goes in `docs/MEASUREMENTS.md` with the build configuration it came from.
  Development or Shipping, never editor, because editor frame-rate smoothing hides the spikes.
- Every editor-side change goes in `docs/EDITOR_SETUP.md`.
- Learning log evidence is added as work happens, not in week 8.
