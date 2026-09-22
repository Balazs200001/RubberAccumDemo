# PLAN

Source of truth for scope. Supersedes the milestone dates in `project-proposal.md` section 6.
The proposal's *content* still stands; only its calendar does not.

Revised 2026-09-21 (start of week 4).

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

**Written and built, not yet demonstrated:** C1 and C2. The C++ compiles against all three build
paths and three new headless tests pass, but neither task's actual exit condition — a closed
circuit and cars lapping *in a level* — has been shown, because that needs the editor open and
this session had it closed, then found `unreal-mcp` unreachable when the plan called for opening
it. That is a real gap, not a formality: this project's own history already has one instance of a
task sitting "written, tested by readback, and never seen" for a stretch, for the same reason
(B1/B2). It is the second time an editor-dependent step has been the slow part while the C++ and
measurement work moved fast.

**Untouched:** C3, C4, C5, and everything in phases D through I.

**Straight answer on the cut list: not invoked, but on narrower grounds than "velocity is fine."**
The rate of closing tasks is not in question — B, C0, and C1/C2's code all closed inside about a
day. What is still unproven is the part of phase C that is not C++: getting the editor and MCP
into a state where the exit conditions can actually be shown, and after that, phase F's game/render
-thread split, which PLAN.md already named on 2026-09-21 as the least familiar part of the engine
and which has not been started. Neither of those is a speed problem the last day of work is
evidence against. If Wednesday's real checkpoint finds C1 and C2 still undemonstrated for the same
tooling reason, that is the signal to treat editor/MCP availability itself as a project risk, not
only a scope one — and the cut list's first candidates on a scope reason would be D5 (region-
follows-track for the 1500 m circuit) and the marble phases G, which are the least connected to the
project's central claim about the emergent racing line.

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
as the editor target. Twelve automation tests pass headlessly. Not verified: A7 has not been
exercised from an actual Blueprint graph, only compiled, because no map exists in `Content/` yet.
Carried into phase C.

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
| B4 | 2048 square RGBA16f over a 200 m region, 32 MiB, 9.77 cm per texel. 4096 square stays configurable at 128 MiB |
| B5 | Additive write of an unbounded exposure value, saturation applied at sample time. Rejects both hardware-blend saturation and the double buffer |
| B6 | One RGBA16f target: R buildup, G skid, B marbles, A unused and not writable through the canvas path |

Two findings that change later phases:

- **Paint cost is per contact, not per texel**, at roughly 0.075 ms per contact and flat across a
  64-fold range of buffer sizes. Eighty contacts costs 5.0 to 6.0 ms. **Batching moves from phase H
  to a prerequisite of phase C**: unbatched, the harness supports about six cars, not the twenty
  that C3 and E7 need. Added as C0 below.
- **A 1500 m circuit does not fit in one region at any resolution measured.** 4096 square still
  gives 36.62 cm per texel over 1500 m. The single-region design is a 200 m instrument, and the
  circuit case needs a following region or tiling. Folded into D5.

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

**Status 2026-09-22: C1 and C2 code complete, exit conditions not yet demonstrated.** Both live in
`Source/RubberAccumDemo/`, the host module, not the plugin. `ARacetrackCircuit` and
`ASplineFollowingCar` build against all three targets (`BuildPlugin`, the editor target, the game
target) and three new headless automation tests pass alongside the existing 24:
`RubberAccumDemo.Track.IsClosedLoopWithPositiveLength`,
`.WrapsDistanceBeyondTrackLength`, `.LateralOffsetMovesAcrossTrackWidth`. Run with
`-ExecCmds="Automation RunTests RacetrackSurface+RubberAccumDemo"` to cover both prefixes in one
pass.

**Not verified — needs the editor.** C1's exit is "closed circuit in the level" and C2's is "cars
lap unattended". Neither has been placed in a level; nothing has rendered or ticked in a live
world. The headless tests prove the geometry and distance math are correct in isolation, not that
the exit conditions are met. The editor was closed for this pass and the `unreal-mcp` server did
not connect (`ConnectionRefused`), so step 3 — placing the track and cars in a map and confirming
they lap and paint rubber — is blocked until the editor is open and the MCP port is reachable from
this session's working directory.

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

`ASurfaceVerificationRig` is not yet deleted. PLAN.md says to delete it once the cars work; that is
an editor-verified fact, not a headless one, so it waits for step 3.
| C3 | Per-position speed and slip curves, lateral offset per car | Traffic spreads across the line |
| C4 | Time compression with lap counter | 50 laps in about two minutes |
| C5 | Fixed-camera observation mode | Repeatable identical load per run |

*Exit: press play, walk away, come back to a marked track.*

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

## Phase D — Skid marks at product quality

| # | Task | Exit |
|---|---|---|
| D1 | Velocity-oriented rotated brush quad, narrow | Marks align with travel direction |
| D2 | Immediate write: darken, drop roughness | Reads as a lockup mark |
| D3 | Defined behaviour at the region edge | Defined, not accidental |
| D4 | Save, reload, reset | State survives a round trip |
| D5 | Behaviour under World Partition streaming | Correct across a streaming boundary |
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

## Standing rules for this backlog

- A phase is not complete until its exit condition has been demonstrated, and what was *not*
  verified is stated.
- Every measurement goes in `docs/MEASUREMENTS.md` with the build configuration it came from.
  Development or Shipping, never editor, because editor frame-rate smoothing hides the spikes.
- Every editor-side change goes in `docs/EDITOR_SETUP.md`.
- Learning log evidence is added as work happens, not in week 8.
