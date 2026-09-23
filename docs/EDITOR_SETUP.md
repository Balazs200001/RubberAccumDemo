# EDITOR_SETUP

Running record of every editor-side change: asset path, what was created or changed, and why.
This is the reproducible record of the half of the project that does not appear in diffs.

Append entries. Do not rewrite history here.

## Format

```
### <date> — <short title>
Asset: <content path>
Change: created | modified | deleted
Why: <reason>
Made via: MCP tool name, or by hand
```

---

## 2026-09-21 — Baseline

No editor-side changes yet. The `ModelContextProtocol` plugin is enabled in
`RubberAccumDemo.uproject`, and `ModelingToolsEditorMode` remains enabled from project creation.

Nothing has been authored in the content browser for this project. The first entries here will
be the phase B receiving material function and the brush material.

## 2026-09-22 — MCP editor toolsets enabled

Asset: `RubberAccumDemo.uproject`
Change: modified
Why: `ModelContextProtocol` alone only publishes the AgentSkill toolset, which cannot touch
assets, actors, materials, the viewport or automation tests. The capability lives in separate
per-area plugins under `Engine/Plugins/Experimental/Toolsets/`, none of which are on by default.
Without them phase B could not be driven or verified through MCP at all.
Made via: by hand, editing the `.uproject`

Enabled, all four already carrying prebuilt Win64 binaries so no engine rebuild was needed:

| Plugin | What it provides |
|---|---|
| `EditorToolset` | viewport and editor capture, level camera, actor and asset selection, CVar lookup, PIE start/stop, and the Python toolsets below |
| `AutomationTestToolset` | automation test discovery and execution against the running editor |
| `LiveCodingToolset` | Live Coding compile triggered from the running editor |
| `ConfigSettingsToolset` | config section read and write |

`EditorToolset` also ships a second family of toolsets as Python under
`EditorToolset/Content/Python/editor_toolset/`, which is where material, asset, scene, blueprint,
static mesh and texture tooling actually comes from. Enabling that one plugin brings all of it.
To reproduce this setup, those four `.uproject` entries are the whole change.

`MCPClientToolset` was enabled first and then removed. Despite the name it is an adapter letting
Unreal connect outward to other MCP servers, and it publishes no callable tools.

Two capabilities confirmed absent after enabling, which shaped the phase B method:

- No console-command or CVar-set tool exists in any toolset, so `stat GPU`, `CsvProfile` and
  `FreezeRendering` cannot be driven over MCP. B3 was re-specified around RHI timestamp queries
  in an automation test instead. See [PLAN.md](PLAN.md).
- No render-target asset tool exists. The accumulation target is created in C++ at runtime, sized
  from the region config, which is where it belongs for a plugin anyway.

## 2026-09-22 — B2, the receiving material function and its parameter collection

Four assets created. Three are the deliverable and live in plugin content; one is host-project
verification dressing and lives in `/Game`.

### `/RacetrackSurface/Materials/MPC_RacetrackSurface`

Change: created
Why: the B5 decision stores unbounded additive exposure and applies `1 - exp(-k * E)` at sample
time. That constant has to be identical in the material and in the phase F query path, so it
cannot be typed into the graph. The single source of truth is
`SurfaceAccumulationMapping.h` in the runtime module; `USurfaceAccumulationSubsystem` pushes the
value into this collection, and the material function reads it back as a collection parameter.
The region is pushed the same way so the material can convert world position to region UV without
the region being duplicated anywhere.
Made via: `MaterialTools.create_parameter_collection`, then `ObjectTools.set_properties`

| Parameter | Kind | Asset default | Set at runtime from |
|---|---|---|---|
| `SaturationConstant` | Scalar | **0.0** | `RacetrackSurfaceMapping::SaturationConstant` |
| `RegionWorldSize` | Scalar | 1.0 | `FSurfaceRegion::WorldSize` |
| `RegionCentre` | Vector | (0,0,0,0) | `FSurfaceRegion::WorldCentre` in RG |

**The 0.0 default for `SaturationConstant` is deliberate and is the failure mode of the design.**
If the C++ push never happens, `1 - exp(0)` is 0 and every surface reads as no coverage at all,
which is immediately visible. The alternative, seeding the asset with 3.0, would leave a second
copy of the constant that silently keeps working when the push is broken. `RegionWorldSize`
defaults to 1.0 rather than 0.0 only to keep the UV divide away from a division by zero; the
saturation gate makes the resulting UV irrelevant.

Setting the parameter arrays through reflection was the one risk here, because
`FCollectionParameterBase` generates its `Id` GUID in its constructor and a reflection write could
have left them null or duplicated. Read back after writing: three distinct GUIDs, so the tool
default-constructs each element before applying the JSON.

### `/RacetrackSurface/Materials/MF_SampleRacetrackSurface`

Change: created
Why: the B2 receiving function. Samples the accumulation target by world position and returns
saturated coverage.
Made via: `MaterialTools.create_function`, `add_expression`, `connect_expressions`, `recompile`

Interface, confirmed by reading the compiled function back:

| Direction | Name | Type |
|---|---|---|
| In | `AccumulationTarget` | Texture2D |
| Out | `Coverage` | Vector3, R buildup, G skid, B marbles |
| Out | `Buildup` | Scalar |
| Out | `Skid` | Scalar |
| Out | `Marbles` | Scalar |

Graph, in order: `WorldPosition` masked to RG, minus `RegionCentre` masked to RG, divided by
`RegionWorldSize`, plus 0.5, into the texture sample UVs. The sampled RGB is multiplied by
`SaturationConstant`, multiplied by -1, through `Exp`, through `OneMinus`. That is
`1 - exp(-k * E)` evaluated on all three channels in one `Exp` rather than three.

Three choices in here worth defending:

- **`WorldPositionShaderOffset` is `WPT_ExcludeAllShaderOffsets`, not the default.** The sample
  position must be the geometric surface position. With the default, a surface using world
  position offset or displacement would sample the buffer at its *displaced* position, so the mark
  would slide across the surface as the displacement changed. H1's parallax occlusion mapping makes
  that a real case rather than a hypothetical one.
- **`SamplerType` is `SAMPLERTYPE_LinearColor`.** The buffer is RGBA16f holding linear exposure,
  and B6 already measured the canvas write path as linear. Declaring `Color` would have compiled,
  because the hardware sRGB decode comes from the texture's own format rather than from the
  material, but it would have documented the wrong thing.
- **`SamplerSource` is `SSM_Clamp_WorldGroupSettings`.** A shared clamped sampler, so the address
  mode is a property of the material rather than of whatever texture is bound, and a sample taken
  outside the region clamps to the edge instead of tiling the region across the whole world. This
  is not a considered answer to the region edge; D3 owns that. It is the least wrong default, and
  it stops the B2 screenshot being misleading.

Not included: an inside-or-outside-the-region output. D3 owns region edge behaviour and the
function should not pre-empt the answer.

### `/RacetrackSurface/Materials/T_EmptyAccumulation`

Change: created
Why: `UMaterialExpressionTextureObjectParameter::Compile` calls `TextureIsValid` and errors when
the default texture is null, so the receiving material needs a valid default. Reading
`MaterialExpressionUtils::VerifySamplerType` shows it returns true early for a null texture, so the
sampler type itself is not the constraint; the parameter simply needs *a* texture. A 1x1 linear
black texture is the honest default because it means exactly what it looks like: nothing
accumulated yet.
Made via: 1x1 PNG written to the scratchpad, `TextureTools.import_file`, then
`ObjectTools.set_properties` to set `SRGB false`, `TMGS_NoMipmaps`, `TC_VectorDisplacementmap`

### `/Game/Materials/M_SurfaceAccumulationDebug`

Change: created
Why: a surface to see the buffer on. `TextureObjectParameter` named `AccumulationTarget` feeds the
function; the function's `Coverage` output drives emissive. Shading model is `MSM_Unlit`
deliberately, so the screenshot shows the buffer contents rather than a lit interpretation of
them. E4 is where the material response becomes a real lit surface.
Made via: `MaterialTools.create_material`, `add_expression`, `connect_to_output`, `recompile`

Placed in `/Game` rather than plugin content because it is verification dressing for the host
project, not part of what the plugin ships. The function and the collection are the deliverable.

### `/Game/Maps/L_SurfaceVerification`

Change: created
Why: `Content/` had no `.umap` at all, so there was no surface to verify B1 or B2 against. The
automation tests had been running against the Engine OpenWorld template, which is not in the
project.
Made via: `AssetTools.duplicate` of `/Engine/Maps/Templates/Template_Default`, then
`SceneTools.add_to_scene_from_asset`

Chosen over the World Partition OpenWorld template that was open unsaved in the editor at the
start of the session. That one carries a landscape and would have written an `__ExternalActors__`
tree of roughly ninety files into the repo, its landscape layer material is a worse target for the
receiving function than a flat plane, and it would have front-loaded the streaming work that D5
owns deliberately.

Contains one added actor, `SurfaceRegionPlane`: `/Engine/BasicShapes/Plane` at the origin, scale
200. Measured bounds after placement are -10000 to +10000 on both axes, which is 20000 cm and
matches the `FSurfaceRegion` defaults exactly, so region and surface align one to one. The
template's own floor tops out at Z -0.5, so the plane sits at Z 0 without z-fighting.

`SurfaceRegionPlane` was **removed later the same session** once `ASurfaceVerificationRig` could be
loaded. The rig owns its own plane sized from its region, so the standalone one was redundant and
would have z-fought with it. The level as committed contains one added actor,
`SurfaceVerificationRig`, at the origin with its Z at 5.

### Live Coding cannot introduce a new cross-module export

`LiveCodingToolset.CompileLiveCoding` failed twice, reproducibly, and the second failure is a
limitation rather than a mistake:

```
SurfaceVerificationRig.cpp.obj : error LNK2019: unresolved external symbol
  USurfaceAccumulationSubsystem::GetOrCreateAccumulationTarget(void)
UnrealEditor-RubberAccumDemo.patch_0.exe : fatal error LNK1120: 1 unresolved externals
```

Live Coding links each module's patch separately and in order. It linked the host module's patch
first, against the plugin's *already loaded* import library, which predates
`GetOrCreateAccumulationTarget`. It then linked the plugin's patch, successfully, but too late. The
same two calls a second time produced `patch_1` with the identical error, so the ordering is not a
race.

The same code compiles and links cleanly on the `RubberAccumDemo` game target with the editor still
open, which is what proves the code is correct rather than the limitation being a real compile
error. The editor target needs a full build with the editor closed before the rig class exists in
the editor.

Worth recording separately: **the game target builds fine while the editor is open.** Only the
editor target is locked by Live Coding. That makes the game target the fast way to check that
host-module and plugin code compiles during an editor-open session, and unlike `BuildPlugin` it is
a Development build, so it compiles the automation test bodies too.

### The limitation is the new export, not the new class — confirmed

After the editor was closed, the editor target rebuilt and the editor reopened, the rig class
registered as `/Script/RubberAccumDemo.SurfaceVerificationRig` and placed normally.

A second change was then needed, and it isolated the cause. `ASurfaceVerificationRig` gained a new
`USceneComponent` root and a new `UPROPERTY` — a change to the layout of an existing reflected
class, which is the thing Live Coding is usually said to refuse — but **no new cross-module
export**. `CompileLiveCoding` returned:

```
Result: Success
Live coding succeeded, data type changes may cause packaging to fail if
assets reference the new or updated data types
```

So the rule is confirmed by both a failure and a success: Live Coding handles new source files,
`Build.cs` changes and new `UPROPERTY` members on existing classes. What it cannot do is introduce
a symbol newly exported by one module and consumed by another, because it links the consumer
against the producer's old import library. Only that case costs an editor restart.

## 2026-09-22 — C1/C2 harness map, and the rig retired

### `/Game/Maps/L_TrackHarness`

Change: created
Why: the phase C harness map. `ARacetrackCircuit` plus five `ASplineFollowingCar` on a 200 m
surface, which is what C1 and C2's exit conditions are demonstrated in.
Made via: `AssetTools.duplicate` of `/Engine/Maps/Templates/Template_Default`, then
`SceneTools.add_to_scene_from_class` / `add_to_scene_from_asset` and `ObjectTools.set_properties`

| Actor | Class | Placement | Properties set |
|---|---|---|---|
| `SurfaceRegionDisplay` | `BP_SurfaceRegionDisplay` | origin, scale 200 | — |
| `RacetrackCircuit` | `ARacetrackCircuit` | (0, 0, 40) | defaults |
| `Car1` | `ASplineFollowingCar` | (-3000, 3000, 40) | `StartingDistanceAlongTrack` 0, `LateralOffsetAlpha` -0.4 |
| `Car2` | `ASplineFollowingCar` | (3000, 3000, 40) | 6184, -0.2 |
| `Car3` | `ASplineFollowingCar` | (6000, 0, 40) | 12368, 0.0 |
| `Car4` | `ASplineFollowingCar` | (3000, -3000, 40) | 18553, 0.2 |
| `Car5` | `ASplineFollowingCar` | (-3000, -3000, 40) | 24737, 0.4 |

The car placements are cosmetic only — `BeginPlay` puts each car at its
`StartingDistanceAlongTrack`, so the editor-time transform never matters. Distances are the lap
length (30921 cm, read back from the generated spline's reparam table) divided five ways. The
five distinct `LateralOffsetAlpha` values were chosen so each car leaves a separately identifiable
band, which is what makes "rubber appears where they drive" checkable rather than a single line
that any one car could have produced.

None of the cars have `Track` set. They find the circuit through the `TActorIterator` fallback in
`BeginPlay`, and no car logged the "found no ARacetrackCircuit" warning, so that path is exercised
rather than merely written.

### `/Game/Blueprints/BP_SurfaceRegionDisplay`

Change: created
Why: replaces the display half of `ASurfaceVerificationRig` — a surface to see the buffer on, and
the runtime binding of the accumulation render target into the debug material. It also closes the
A7 debt, because these are the plugin's Blueprint wrappers doing a real job rather than a
graph written only to prove they compile.
Made via: `BlueprintTools.create` (parent `StaticMeshActor`), `add_struct_variable`,
`set_variable_instance_editable`, `write_graph_dsl`, `compile_blueprint`, then
`ObjectTools.set_properties` on the CDO's `StaticMeshComponent0`

Parented to `StaticMeshActor` rather than `Actor` deliberately: `BlueprintTools` has no
add-component tool and `PrimitiveTools` only operates on placed actors, so inheriting
`StaticMeshComponent` from the parent class is the only way to give a Blueprint a mesh over MCP.

Defaults on the inherited component: `/Engine/BasicShapes/Plane`,
`M_SurfaceAccumulationDebug`, scale 200. The scale is **also** passed explicitly at placement,
because the mesh component is the actor root and the spawn transform overwrites the CDO scale —
the same trap recorded against the rig further up this file.

`EventBeginPlay`, read back from the compiled graph:

```
(event EventBeginPlay
  (RacetrackSurface|ConfigureSurfaceRegion (Variables|Default|GetRegion))
  (RacetrackSurface|ResetSurface)
  (bind _returnvalue (RacetrackSurface|GetAccumulationTarget))
  (bind _returnvalue_1 (Rendering|Material|CreateDynamicMaterialInstance
                         (Variables|StaticMeshActor|GetStaticMeshComponent)))
  (Rendering|Material|SetTextureParameterValue _returnvalue_1
                         "AccumulationTarget" _returnvalue))
```

`Region` is an instance-editable `FSurfaceRegion` member, so the region is configured from the map
rather than from code. `ResetSurface` is there so every play session starts from a clean buffer.

### `/Game/Maps/L_SurfaceVerification`

Change: deleted
Why: the rig owned the plane in it, so removing `ASurfaceVerificationRig` left the map with
nothing in it. `L_TrackHarness` supersedes it on every axis. The B1/B2 evidence it produced stays
in `MEASUREMENTS.md` and ILO 1, and the build recipe is recorded further up this file.
Made via: `AssetTools.delete`, then `git rm` — see below

**`AssetTools.delete` returns `true` without deleting a `.umap`.** Called twice on
`/Game/Maps/L_SurfaceVerification`, both times returning `true`, and both times `AssetTools.exists`
still returned `true` and the file was still on disk. `get_referencers` returned an empty array, so
nothing was holding it. The file was removed with `git rm` instead. Worth knowing before trusting
that return value for any future asset deletion: **verify deletes with `exists`, do not trust the
boolean.**

### The paint timing CVar cannot be set over MCP

`r.RacetrackSurface.TimePaint` gates the new timing instrument, and there is no way to set it from
any toolset. `EditorAppToolset.SearchCVars` is lookup-only, and `ConfigSettingsToolset` edits
Project Settings sections backed by a `UObject`, not arbitrary console variables. This is the same
gap recorded against `stat GPU` and `CsvProfile` on 2026-09-22.

The measurement run was taken by temporarily changing `bEnabled` in `FScopedPaintTimer`'s
constructor to ignore the CVar, triggering `LiveCodingToolset.CompileLiveCoding`, running the
session, then reverting and recompiling. Deliberately a one-line change inside a function body
rather than flipping the CVar's *default*: Live Coding patches code but does not re-run the static
constructor that registers a `TAutoConsoleVariable`, so a changed default would likely not have
taken effect and the run would have silently measured nothing.

If this instrument gets used often, the gate should become a `UDeveloperSettings` property, which
`ConfigSettingsToolset` *can* set. Not done now because D6 is where profiling becomes routine.

## 2026-09-23 — The paint timing measurement no longer needs the editor at all

Change: none to any asset. Recorded here because it supersedes the recipe directly above.

The Live Coding workaround above exists because MCP cannot set a console variable **in a running
editor**. That is still true and is still worth knowing. But the measurement never needed the
running editor in the first place.

`r.RacetrackSurface.TimePaint` is declared `ECVF_Default` and read with `GetValueOnGameThread`, so
an ordinary `-ExecCmds` sets it at startup. The editor *binary* can boot an uncooked map directly
in game mode, which is what makes this work where the standalone game target cannot:

```
UnrealEditor.exe "<abs path>/RubberAccumDemo.uproject" /Game/Maps/L_TrackHarness -game \
  -ExecCmds="r.RacetrackSurface.TimePaint 1" \
  -windowed -resx=1280 -resy=720 -nosplash -nosound -abslog="C:/RSBuild/r6.log"
```

Better than the old recipe on four counts, which is why R6 was taken this way:

- **No source edit**, so nothing can be left un-reverted. The old recipe's revert step was a manual
  action with no check on it.
- **No Live Coding**, so no editor restart risk and no patch DLLs.
- **It obeys this project's own measurement rule.** `MEASUREMENTS.md` says Development or Shipping
  and never editor, because editor frame-rate smoothing hides spikes. Simulate-In-Editor broke that
  rule out of necessity. A standalone `-game` run does not.
- **Reproducible from one command line** that goes in the measurement entry, rather than a
  four-step procedure recorded in prose.

**The trap, which cost a run.** Git Bash rewrites a leading-slash argument that looks like a POSIX
path, so `/Game/Maps/L_TrackHarness` arrived as `C:/Program Files/Git/Game/Maps/L_TrackHarness` and
the map silently failed to load — the engine logged `LoadPackage can't find package` and carried on
into an empty world. Prefix the command with `MSYS_NO_PATHCONV=1`, or run it from PowerShell.

Still not reachable this way: `stat unit` and `stat GPU` write to the screen rather than the log, so
frame rate remains uncaptured. That gap is unchanged.

## 2026-09-23 — R5: the receiving function samples by mesh UV

### `/RacetrackSurface/Materials/MF_SampleRacetrackSurface`

Asset: `/RacetrackSurface/Materials/MF_SampleRacetrackSurface`
Change: modified
Why: phase R stores surface state as a track-space ribbon, and the function still sampled by world
position, which against a ribbon reads meaningless texels. The road mesh carries the ribbon
coordinate in UV0 by construction, so a plain texture coordinate *is* the track-space lookup.
Made via: `MaterialTools.add_expression` (`MaterialExpressionTextureCoordinate`),
`connect_expressions` into `MaterialExpressionTextureSample_0`'s `UVs`, `recompile`,
`AssetTools.save_assets`

One node added, one wire moved. **The dangling world-position chain was left in the graph** —
`WorldPosition_0`, `ComponentMask_0`, `ComponentMask_1`, `CollectionParameter_0`,
`CollectionParameter_1`, `Subtract_0`, `Divide_0`, `Constant_0`, `Add_0`. Replacing the *consumer*
takes them out of the compiled shader, and `delete_expression` asserts and halts the editor on a
MaterialFunction, which is recorded further up this file. Verified saved with `is_dirty` returning
false rather than trusting the save's boolean.

**This edit was made and lost once before**, in the session where `delete_expression` halted the
editor before anything was saved. Redone here and saved first.

### `/Game/Maps/L_TrackHarness`

Change: modified
Why: the circuit spawns its own field now, so the five hand-placed `ASplineFollowingCar` actors
were a duplicate population — and their hand-set `LateralOffsetAlpha` values were dead anyway,
because C3 renamed that property to `BaselineLateralOffsetAlpha`. `BP_SurfaceRegionDisplay` went
with them: its flat 200 m plane cannot show a ribbon, and the binding job it did is now done by the
circuit's road.
Made via: `SceneTools.remove_from_scene` six times, `AssetTools.save_assets`

The map now holds the circuit, the template lighting and sky, and nothing else of ours.

### The road mesh was wound inside out, and nobody had looked

Not an editor change, but it was found here and it is the reason R5 appeared to fail at first.

With the material fixed, the road still rendered nothing. The component reported
`bVisible true`, the right override material, identity transform and correct bounds — so it existed
and was not hidden. `M_SurfaceAccumulationDebug` has `TwoSided` false, which made backface culling
the suspect.

Confirmed against the engine's own reference rather than by guessing. `GenerateBoxMesh`'s top face
(`KismetProceduralMeshLibrary.cpp`) has normal `(0,0,1)` and is therefore visible from above, and
its winding gives `(v1-v0) x (v2-v0)` pointing **down**. `GenerateRoadMesh` wound the other way, so
every triangle faced the ground. Swapped, and the road appeared immediately.

**The mesh had been wrong since it was written** and was committed, built and measured against
without anyone seeing it, because `PLAN.md` had been carrying "nothing has been seen on screen
since the restructure" as a known gap. It cost nothing in the measurements — the paint path never
touches the mesh — but it is the clearest argument in this project so far for looking at the thing.

## 2026-09-23 — `/Game/Maps/L_SpaHarness`, a Spa-shaped circuit with twenty cars

Asset: `/Game/Maps/L_SpaHarness`
Change: created
Why: the harness had only ever run the stadium oval. Spa is the circuit the phase R memory
argument is built around, and nothing had ever driven its shape.
Made via: `AssetTools.duplicate` of `/Game/Maps/L_TrackHarness`, then
`ObjectTools.set_properties` on `RacetrackCircuit_0` and two `SceneTools.remove_from_scene`

| Property | Value | Why |
|---|---|---|
| `Layout` | `Spa` | Selects the authored centreline over the generated stadium |
| `AuthoredLayoutLapLength` | 150000 cm | See the scale note below |
| `CarCount` | 20 | The density C3 and E7 need |
| `RoadSegmentLength` | 400 cm | 1500 m of road at 200 cm would be 750 segments for no visible gain |

`Floor_0` and `StaticMeshActor_0`, both from the `Template_Default` duplicate, were removed. They
are a 80 m square sitting in the middle of a 425 by 266 m circuit, which reads as debris from above.

### Why 1500 m and not "Spa, halved"

The unfolded ribbon cannot exceed 16384 texels along the track, and folding is not implemented.

| Scale | Lap | Along-track texels at 9.7656 cm | Fits |
|---|---|---|---|
| Full Spa | 7004 m | 71721 | no |
| Halved | 3502 m | 35860 | no |
| Quarter | 1751 m | 17929 | no, just over |
| **21.4 %** | **1500 m** | **15361** | **yes** |

So "halved" was not available. 1500 m is the largest round figure that fits at the production texel
density, and it is the case `PLAN.md` already tabulated. Confirmed at runtime:

```
Surface ribbon configured: 150000.0 cm of track at 1200.0 cm wide, 9.7656 cm per texel,
                           15361 x 123 texels, closed loop
Accumulation buffer allocated at 15361 x 123, 15115224 bytes
RacetrackCircuit_0 spawned a field of 20 cars spread across 0.80 of the track width
```

Track width stays at 1200 cm rather than scaling with the layout. Scaled properly it would be
3.2 m, which is narrower than two of the 2.2 m cars side by side, so there would be no lateral
spread to observe. This is stated as the harness convenience it is: the circuit is Spa's *shape* at
21 % with a proportionally wide track, not a scale model of Spa.

### The centreline is authored, and how

`RacetrackLayouts::SpaCentreline` in `RacetrackCircuit.cpp` is 76 points read off the circuit's plan
view, stored in arbitrary units. `GenerateTrackSpline` builds the spline from them, measures the
resulting length, then rescales about the centroid so the lap lands on `AuthoredLayoutLapLength` —
two passes, because the length of a Catmull-Rom spline through given points is not known before
building it. Measured 150000.0 cm against 150000 requested.

The points are mapped `(-Y, X)` rather than `(X, -Y)` so the circuit sits the same way up as the
plan it was read from. The first attempt used the latter and produced a Spa rotated 90 degrees,
which is only obvious once you look at it from directly above.

### Later the same day: elevation, a tighter chicane, and demo controls

Asset change: none beyond the property values already tabulated. Recorded here because the map's
behaviour changed substantially and a rebuild would otherwise not reproduce it.

`SpaCentreline` gained a third component, so the circuit is no longer flat. The authored profile
keeps Spa's real proportions — Eau Rouge the low point, Les Combes the high point, roughly 10 %
average gradient up Raidillon, which matches the real climb — and `ElevationExaggeration`,
default 2.5, scales it for legibility. At that setting the lap rises **3805 cm over 1500 m, 2.54 %**,
against real Spa's 1.23 %. The exaggeration is deliberately a separate named factor rather than
baked into the data, so the data stays honest and the demo choice is visible.

The Bus Stop was rebuilt as two near-90 degree turns over about a 45-unit radius rather than the
smooth sweep the first pass produced. At the layout's scale that is a corner radius comparable to
the track width, which is what a real chicane looks like.

The circuit now also does three things at `BeginPlay` that exist purely to make the harness
watchable, and none of which touch the surface system:

| Behaviour | Why |
|---|---|
| Packs the field `GridSpacingCentimetres` apart | Cars spread evenly around the lap read as scattered traffic, not a race |
| Places the observer from the track's own bounds | Spawning at the PlayerStart left you on the ground at one corner of a 425 m circuit |
| Binds `.` and `,` to global time dilation | Buildup takes tens of laps at the derived rate |

**The observer distance is computed, not guessed.** It comes from the camera's actual FOV and the
viewport aspect against the track's projected extent — width across the screen, and the along-view
extent foreshortened by the observer pitch plus the elevation. `ObserverFramingScale` is then a
genuine margin, 1.08. The first version multiplied the track's half-extent by 2.4, which is roughly
a bounding-sphere fit and put the camera **twice as far out as it needed to be** for an elongated
track viewed along its short axis.

### The road mesh is regenerated by a level reload, not by Live Coding alone

Worth recording because it cost a confusing few minutes. Live Coding patches the code but does not
re-run `OnConstruction`, so a change to `GenerateRoadMesh` or to the layout data has no visible
effect until the actor reconstructs. Reloading the level does it; so does touching any property on
the actor. `SceneTools.load_level` refuses while the level is dirty, so save first.

### Live Coding confirmed again on the plugin module

Three `CompileLiveCoding` calls succeeded in this session, all patching
`UnrealEditor-RacetrackSurface` (`patch_0` through `patch_2`), for changes that added a CVar, a
class and a log call inside the plugin's own module. Consistent with the rule established on
2026-09-22: Live Coding handles anything short of a symbol newly exported by one module and
consumed by another.

## 2026-09-22 — R3, the region display stops configuring the region

### `/Game/Blueprints/BP_SurfaceRegionDisplay`

Change: modified
Why: [phase R](PLAN.md) replaces `FSurfaceRegion` with `FSurfaceTrackRibbon`, and the ribbon is
sized from the centreline rather than typed into a map. This Blueprint held an instance-editable
`FSurfaceRegion` member and called `ConfigureSurfaceRegion` from `EventBeginPlay`; both are gone.
Configuration moved to `ARacetrackCircuit::PostInitializeComponents`, which is the actor that
actually knows the track's length, width and closure.
Made via: `BlueprintTools.write_graph_dsl`, `remove_variable`, `compile_blueprint`, then
`AssetTools.save_assets`

`EventBeginPlay` now reads back as:

```
(event EventBeginPlay
  (RacetrackSurface|ResetSurface)
  (bind _returnvalue (RacetrackSurface|GetAccumulationTarget))
  (bind _returnvalue_1 (Rendering|Material|CreateDynamicMaterialInstance
                         (Variables|StaticMeshActor|GetStaticMeshComponent)))
  (Rendering|Material|SetTextureParameterValue _returnvalue_1
                         "AccumulationTarget" _returnvalue))
```

`list_variables` returns an empty array, so the `FSurfaceRegion` member is genuinely gone rather
than orphaned. Verified saved with `is_dirty`, which returns false — the `AssetTools.delete`
lesson recorded further up this file is that a boolean return is not evidence.

**Ordering mattered and is the reason this was done before the C++ change, not after.** Deleting
`FSurfaceRegion` while a Blueprint still holds one as a member breaks that Blueprint on load. The
editor was still running the pre-phase-R C++ when this edit was made, so the variable and the node
both still existed and could be removed cleanly.

**`PostInitializeComponents`, not `BeginPlay`, is where the circuit configures the ribbon.** All
actors get `PostInitializeComponents` during `InitializeActorsForPlay` before any actor gets
`BeginPlay`, so the ribbon is configured before this Blueprint's `GetAccumulationTarget` allocates
the buffer. With configuration in `BeginPlay` the order would be undefined, and a buffer allocated
at the default ribbon size would be bound into the material and then thrown away when the real
ribbon arrived.

### `ReadRenderTargetRawPixelArea` takes a width and a height, not a max

Recorded because the parameter names say otherwise.
`UKismetRenderingLibrary::ReadRenderTargetRawPixelArea(WorldContextObject, Target, MinX, MinY,
MaxX, MaxY, bNormalize)` passes its last two coordinate arguments to the file-local
`ReadRenderTargetHelper` as `Width` and `Height`, which builds
`FIntRect SampleRect(X, Y, X + Width, Y + Height)` at `KismetRenderingLibrary.cpp:326`. So
`MaxX`/`MaxY` are counts, not coordinates, and passing a real maximum reads a rect twice as far as
intended.

`bNormalize = false` is the one that matters for a float target: it selects `RCM_MinMax`, which
the RHI documents as leaving values unchanged. With the default `true` the read goes through
`RCM_UNorm` and the accumulator's unbounded exposure values would be rescaled.

### `MaterialTools.delete_expression` asserts and halts the editor on a MaterialFunction

Hit 2026-09-23 while reworking `MF_SampleRacetrackSurface` for phase R. One call to
`MaterialTools.delete_expression`, removing a now-dangling `MaterialExpressionAdd` from the
function graph, stopped the editor in the debugger:

```
Script Stack (2 frames):
  /Script/MaterialEditor.MaterialEditingLibrary.DeleteMaterialExpressionInFunction
  /EditorToolset/Python/editor_toolset/toolsets/material_PY.MaterialTools.delete_expression
Assertion failed: !IsRooted()  [UObjectBaseUtility.h] [Line: 209]
A breakpoint instruction (__debugbreak()) was executed in UnrealEditor.exe.
```

The MCP call never returned; the client gave up after its 300 s idle timeout. The editor had to be
closed from the debugger.

**Nothing was lost.** The two edits made before the crash — adding a `TextureCoordinate` and wiring
it to the texture sample's `UVs` — had not been recompiled or saved, so the asset on disk was still
the last saved version and the edits were simply redone afterwards.

**The rule taken from this: add and rewire, do not delete, on a MaterialFunction graph.** Replacing
the world-position chain's *consumer* is enough to take it out of the compiled shader — an
expression with no path to an output contributes nothing. The dead nodes are left in the graph
deliberately rather than risking the editor again for tidiness. Anything that genuinely must be
deleted from a MaterialFunction is a by-hand job in the material editor.

This sits alongside the two capability gaps already recorded on 2026-09-22 (no console-command
tool, no render-target asset tool) and the `AssetTools.delete` lesson above: **three of the four
destructive or mutating MCP operations tried in this project have misbehaved.** Prefer additive
operations, and verify rather than trust a return value.

### Why the rig owns its plane rather than the level

First placement of the rig produced a plane 1 m across instead of 200 m. `OnConstruction` sets the
plane's scale from `Region.WorldSize`, but the plane was the actor's **root** component, so its
scale *is* the actor's scale, and the spawn applied its own transform afterwards and overwrote it.
Setting the actor transform again did not fix it either: `OnConstruction` re-runs on an editor drag
via `PostEditMove`, not on a programmatic `SetActorTransform`.

Fixed structurally rather than by working around it. The rig now has a plain `USceneComponent` as
its root and the plane is a child, so the actor's transform and the region-derived scale no longer
compete for the same slot. Measured bounds after the fix are -10000 to +10000 on both axes, which
is the region size derived rather than typed. That is the point of the rig owning the surface at
all: the plane cannot silently disagree with the region it is displaying.
