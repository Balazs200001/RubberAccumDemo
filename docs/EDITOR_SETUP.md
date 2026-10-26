# EDITOR_SETUP

Running record of every editor-side change: asset path, what was created or changed, and why.
This is the reproducible record of the half of the project that does not appear in diffs.

Append entries. Do not rewrite history here.

Mentions of `PLAN.md` below refer to the plan as it stood when the entry was written. Since 2026-10-06 the plan lives in GitHub issues, starting from [Plan overview](https://github.com/Balazs200001/RubberAccumDemo/issues/1); the file itself is in this repo's history up to commit `ba81300`.

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
  in an automation test instead. See [phase B](https://github.com/Balazs200001/RubberAccumDemo/issues/3).
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

> **Superseded 2026-09-23 by [C8](https://github.com/Balazs200001/RubberAccumDemo/issues/14). The reasoning above is not wrong; it answers a
> question that is no longer the one being asked.** It solved for the largest round lap that fits
> the cap *at a fixed density of 9.7656 cm per texel*, and 1500 m is the right answer to that. C8
> removes the fixed density from the question: the texel size is chosen first, for what a corner
> has to look like close up, and the lap follows from it. The lap is now **480 m at 3.125 cm per
> texel**, which keeps the same 15360 along-track texels and the same 1024 of margin under the
> cap. The table above still holds for its own question and is kept for that reason.
>
> The track-width note above also still holds and is now load-bearing rather than an aside. Because
> the cars do not scale, a shorter lap must carry a proportionally wider track, and 800 cm on a
> 480 m lap is **8 car widths** against real racing's roughly 7.5. What that costs is recorded
> under the road-mesh fold in `MEASUREMENTS.md`.

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
Why: [phase R](https://github.com/Balazs200001/RubberAccumDemo/issues/5) replaces `FSurfaceRegion` with `FSurfaceTrackRibbon`, and the ribbon is
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

## 2026-09-23 — a read-time blur on the sampled buffer

### `/RacetrackSurface/Materials/MF_SampleRacetrackSurface`

Asset: `/RacetrackSurface/Materials/MF_SampleRacetrackSurface`
Change: modified
Why: at the production density a texel is 9.765625 cm, and the road is viewed close enough that
individual texels read as blocks. Density cannot be raised to fix it — 2.44 cm per texel would need
about 61440 texels along a 1500 m lap against the 16384 limit that already forced the layout down to
1500 m. So the coarseness is a consequence of the storage ceiling and has to be softened when the
buffer is read, not when it is stored.
Made via: `MaterialTools.add_expression` x4, `ObjectTools.set_properties`, `connect_expressions` x6,
`recompile`, `AssetTools.save_assets`

Nodes added:

| Node | Purpose |
|---|---|
| `MaterialExpressionCustom_0` | `BlurredExposure`, `CMOT_Float3`, 3x3 tent over nine taps |
| `MaterialExpressionTextureProperty_0` | `TMTM_TexelSize`, fed by the `AccumulationTarget` input |
| `MaterialExpressionScalarParameter_0` | `SurfaceBlurTexels`, default 1.0, group `Racetrack Surface` |
| `MaterialExpressionTextureObject_0` | `T_EmptyAccumulation`, into the function input's `Preview` pin |

`MaterialExpressionMultiply_1`'s `A` input moved from `MaterialExpressionTextureSample_0`'s `RGB`
to the custom node. `TextureSample_0` is left in the graph, dangling, for the same reason the
world-position chain was left on 2026-09-23: `delete_expression` halts the editor on a
MaterialFunction.

Three choices in here worth defending:

- **The blur is applied to exposure, not to coverage.** It is spliced in before the
  `x SaturationConstant -> x -1 -> Exp -> OneMinus` chain, so the function still evaluates
  `1 - exp(-k * E)`, now on a locally averaged `E`. Averaging after the curve would be averaging
  saturated values, which is not a quantity that means anything. Averaging exposure is also the
  physically sensible reading: a contact patch spread over slightly more surface.
- **Offsets come from `TextureProperty` set to `TMTM_TexelSize`, not a typed constant.** That
  property returns texel size in UV space, which on a 15360 x 123 ribbon is wildly different per
  axis — and that is exactly right, because texels are square in world space. A single hardcoded
  offset would smear the mark along the track and barely blur it across.
- **The radius is a parameter, defaulting to 1 texel.** `SurfaceBlurTexels = 0` reproduces the
  previous look exactly, so the blur can be switched off to prove it is not hiding anything. Note
  that this puts a sampling-quality knob in the plugin's deliverable function rather than in the
  host's debug material. The alternative — blurring inside `M_SurfaceAccumulationDebug` — would
  require duplicating the saturation curve outside the one place that owns it, which is the failure
  the fail-loud collection default exists to catch.

**A compile warning was fixed on the way, and it is not established whether it was pre-existing.**
After the first recompile, an autosave logged `Failed to compile Material for platform PCD3D_SM6`
against `MF_SampleRacetrackSurface.uasset`. That is the MaterialFunction's own preview material,
not the shipped shader: `M_SurfaceAccumulationDebug` recompiled clean before and after. The cause
is that the `AccumulationTarget` function input's `Preview` pin was unconnected and
`TextureSample_0` carries no default texture of its own (`Texture: None`), so a preview compile had
no texture to sample. Wiring `T_EmptyAccumulation` into that `Preview` pin — the job that asset was
created for — stopped the warning recurring on save. Whether the custom node introduced the failure
or merely exposed a pre-existing one could not be settled: the only logs from before the edit are
`-game` and headless runs, and neither compiles an editor preview material at all.

### What was verified, and what was not

Verified: the wiring, by reading every input back after connecting it; that
`M_SurfaceAccumulationDebug` recompiles without error; that the warning above does not recur on
save; that `is_dirty` returns false rather than trusting the save's return value; and that the
circuit renders with twenty cars in a PIE session on `L_SpaHarness`.

Not verified: that the blur looks better, or that it visibly softens a texel edge. A PIE session
was run for about four minutes and the road was still within a few percent of zero coverage, which
is what the deposition rate predicts — the runtime log states 63 % coverage at roughly 17 laps with
twenty cars. Raising `r.RacetrackSurface.DepositionScale` would have made this visible in seconds,
and no MCP tool can set a console variable, which is the same gap recorded on 2026-09-22. Judging
it needs a human in a play session pressing `]`.

---

## 2026-09-23 — Hazard: running the automation tests with the editor open can write actors into the open map

Asset: `/Game/Maps/L_SpaHarness` (no net change; recorded as a warning, not as a setup step)
Change: none. A change happened and was reverted.
Why this is here: it will happen again to anyone who repeats the workflow.

`FindRenderableWorld()` in `Source/RubberAccumDemo/Tests/RacetrackCircuitTests.cpp` returns the
**editor** world, so every host test spawns its `ARacetrackCircuit` and `ASplineFollowingCar` into
whatever level is currently open. Tests that fail on an early-return path skip their `Destroy()`
calls, and the leaked actors then sit in the open level as ordinary unsaved changes. Closing the
editor and saving writes them into the `.umap`.

This happened during C6. `L_SpaHarness.umap` grew from 737340 to 993171 bytes, and a `-game` run
of the map logged two circuits instead of one:

```
RacetrackCircuit_0  spawned a field of 20 cars   (Spa, 150000 cm, the real one)
RacetrackCircuit_34 spawned a field of 20 cars   (stadium, 30921 cm, leaked from a test)
```

The leaked circuit called `ConfigureTrackRibbon` second, so it reconfigured the shared surface
ribbon from Spa\x27s 150000 cm to the stadium default of 30921 cm. Recovered with
`git checkout -- Content/Maps/L_SpaHarness.umap`, which restores the LFS content, and confirmed by
re-running the map and seeing one circuit and a 150000 cm ribbon.

The early returns in the tests added by C6 now tear down before returning. The older tests in that
file still have the original pattern, so the hazard is reduced and not removed.

**Run the automation tests with the editor closed.** The headless command in `CLAUDE.md` spawns
into a throwaway world and cannot touch a saved map. If the tests must be run in the editor, check
`git status` for a modified `.umap` afterwards and do not save the level on exit.

---

## 2026-09-23 — `L_SpaHarness` repointed at the rescaled circuit, and what it revealed

Asset: `/Game/Maps/L_SpaHarness`
Change: **modified and saved.** 740131 bytes against the 737340 baseline, so no leaked test actors
— see the hazard entry above, whose signature is 993171.
Made via: the user set `AuthoredLayoutLapLength` by hand; `ObjectTools.set_properties` for
`RoadSegmentLength`; `ObjectTools.reset_properties` for `ElevationExaggeration`; `AssetTools.save_assets`

| Property | Value | Note |
|---|---|---|
| `AuthoredLayoutLapLength` | 48000 | Set by the user. **Known wrong, see below** |
| `RoadSegmentLength` | 128 | 400 / 3.125, holds rings-per-corner constant |
| `ElevationExaggeration` | *override cleared* | Raised to 7.8125 and reverted, see below |
| `Layout`, `CarCount` | `Spa`, 20 | Unchanged |

### Two things this only showed once it was on screen

**Setting `RoadSegmentLength` was not optional and had been missed.** The lap was changed without
it, leaving the road mesh at 120 rings where the 1500 m layout had 3750. On corners of 200 to
400 cm radius each corner was drawn as a three-to-five sided polygon. That is what "no detail at
all" turned out to mean, and it is not related to texel density — there is no rubber at all in an
editor viewport, so nothing seen there can be judging the buffer.

**`ElevationExaggeration` cannot be raised to compensate for a shorter layout.** Raised to 7.8125,
which restores the pre-C8 absolute rise, the circuit visibly terraces: the authored Z values are
integers, 35 of the 76 points step by exactly one unit, and at that multiplier one step is a 77 cm
rise over about 620 cm of track — a 12.4 % gradient step against 4.0 % at the default 2.5. The
override is cleared. Figures in [MEASUREMENTS.md](MEASUREMENTS.md).

### The lap length is wrong and the map should not be trusted for captures yet

Seen in a close-up of a slow corner, the road **passes through itself**. That is the offset curve
being undefined where the local radius falls below the half width, and at 480 m on an 800 cm track
the fastest corner has 403 cm of radius against a 400 cm half width. Full derivation in
[C8](https://github.com/Balazs200001/RubberAccumDemo/issues/14#issuecomment-6014436025) and [MEASUREMENTS.md](MEASUREMENTS.md); the short version is that a valid road
needs **954 m minimum at this track width, and 1726 m for the authored kinks to be clean**, and
neither fits under the 16384 along-track cap at 3.125 cm per texel.

### Interim configuration applied, and it is an override of the C++ defaults

Applied the same day so the demo has something valid to run on until folded rows land. **Three
per-instance overrides now diverge from C++**, which is more duplicated state than this map has
carried before and is deliberate, because the C++ defaults belong to C8 and the next phase will
replace them.

| Property | C++ default | Map override | Why |
|---|---|---|---|
| `AuthoredLayoutLapLength` | 48000 | **96000** | 960 m clears the 954 m comfortable fold threshold at 800 cm wide |
| `SurfaceCentimetresPerTexel` | 3.125 | **6.25** | 96000 / 6.25 = 15360 and 800 / 6.25 = 128, both exact |
| `RoadSegmentLength` | 128 | **256** | 400 x 96000 / 150000, holds rings-per-corner at the 1500 m value |
| `TrackWidth` | 800 | not overridden | Unchanged |

```
15360 x 128 texels, 15728640 bytes, exactly 15.00 MiB
a 20 cm contact patch stores as 3.20 texels
```

Against 2.05 texels before C8 and 6.40 at C8's 480 m. Worse than C8 on density, better than
pre-C8, and geometrically valid, which 480 m was not.

**Verified by capture, not by arithmetic.** Top-down close-ups of the chicane and of the tightest
hairpin both show the road turning cleanly with no self-intersection. What remains is mild
faceting on the inside edge from the 256 cm ring spacing, which is the benign failure mode of the
two — a polygonal corner rather than a crumpled one. `L_SpaHarness.umap` saved at 740327 bytes,
well clear of the 993171 that signals leaked test actors.

**Not verified:** no rubber has been seen at 6.25 cm per texel. Every capture above is an empty
buffer, so nothing here judges the surface, only the geometry it is painted onto.

---

## Superseded: the original repointing instruction

Asset: `/Game/Maps/L_SpaHarness`
Change: **superseded by the entry above, which carried it out.**
Why: [C8](https://github.com/Balazs200001/RubberAccumDemo/issues/14) rescales the circuit to 480 m at 3.125 cm per texel on an 800 cm track. Two of
the four properties in the override table above are per-instance overrides on `RacetrackCircuit_0`,
so **changing the C++ default does not change the demo.** The editor was closed and the MCP server
was not listening when the work was done, so this could not be made at the time.

| Property | Was | Set to | Why |
|---|---|---|---|
| `AuthoredLayoutLapLength` | 150000 | **48000** | The rescale. 48000 / 3.125 = 15360 along-track texels exactly |
| `RoadSegmentLength` | 400 | **128** | 400 / 3.125, which holds rings-per-corner constant through the rescale |
| `Layout` | `Spa` | unchanged | Still the authored centreline |
| `CarCount` | 20 | unchanged | Still the density C3 and E7 need |

Either set both to the new values, or clear both overrides so the instance follows the C++ defaults
— they are now the same numbers, and clearing leaves less duplicated state to keep in sync.
`TrackWidth` and `SurfaceCentimetresPerTexel` are **not** overridden and already follow the C++
defaults, so they need no action; `DriverLateralSpread` likewise.

Expected at runtime once done, against 15114240 bytes today:

```
Surface ribbon configured: 48000.0 cm of track at 800.0 cm wide, 3.1250 cm per texel,
                           15360 x 256 texels, closed loop
Accumulation buffer allocated at 15360 x 256, 31457280 bytes
```

**The editor must be restarted rather than Live Coded into this.** C8 adds reflected `UPROPERTY`
members to `ASplineFollowingCar` (`Wheelbase`, `WheelTrack`, `LockupProbability`,
`LockedWheelSlipAmount` and the braking thresholds), and the same rule recorded on 2026-09-22
applies: Live Coding patches code but does not reliably introduce new reflection. Reloading the
level is also needed for the road mesh, since `OnConstruction` does not re-run on a Live Coding
patch alone.

**Three things to look at once it is running, all of them judgements rather than numbers**, and all
three are recorded as unverified in `MEASUREMENTS.md`:

1. Whether a 20 cm mark at 6 texels reads as a tyre mark rather than a block, which is the whole
   purpose of the rescale.
2. Whether the road mesh visibly folds. Measured, the tightest radius over one 128 cm ring is
   89 cm against a 400 cm half-width, a ratio of 0.22 where the 1500 m layout ran 0.46. The fold
   predates C8 and C8 makes it about twice as deep. It should be looked for near 66 % of the lap.
3. Whether a locked wheel reads as one dark mark among four. Press `]` a few times first — the
   deposition rate puts the apex at 63 % coverage in about 28 laps at 1x.

---

## 2026-09-23 — C9: the read path folds the ribbon into rows

Three assets changed together, because the row count has to reach the shader for the fold to mean
anything. Full reasoning in [C9](https://github.com/Balazs200001/RubberAccumDemo/issues/15).

### `/RacetrackSurface/Materials/MPC_RacetrackSurface`

Asset: `/RacetrackSurface/Materials/MPC_RacetrackSurface`
Change: modified — one scalar parameter added
Why: the write side folds in C++ and the read side folds in HLSL, so exactly one number has to
cross the boundary. Everything else the shader needs it reads off the bound texture through
`TMTM_TexelSize`, which cannot disagree with the texture that is actually there.
Made via: `ObjectTools.set_properties` on `ScalarParameters`, `AssetTools.save_assets`

| Parameter | Default | Why that default |
|---|---|---|
| `AccumulationRowCount` | **0** | Matches the fail-loud default already used for `SaturationConstant` |

The shader takes `max(Rows, 1.0)`, so an unpublished row count makes a folded ribbon read as though
it were unfolded — which puts every mark at a quarter of its correct along-track position, four
times over. That is deliberately not subtle. It is the same failure C8 found, where a configuration
path never ran and nothing said so.

`SaturationConstant` and `RegionWorldSize` were read back afterwards and are unchanged.

**`ParameterId` could not be set through the tool** and the call warned about it. That turned out not
to matter: `UMaterialExpressionCollectionParameter::PostEditChangeProperty` derives `ParameterId`
from `ParameterName` whenever the node is edited (`MaterialExpressions.cpp:17169`), so setting the
name is what fixes up the id. The name was set a second time on its own, after `Collection` was
populated, to be sure that path ran. This matters because `PostLoad` goes the other way — it derives
the *name* from the *id* (`MaterialExpressions.cpp:17156`) — so a wrong id would lose the binding
silently on the next load.

### `/RacetrackSurface/Materials/MF_SampleRacetrackSurface`

Asset: `/RacetrackSurface/Materials/MF_SampleRacetrackSurface`
Change: modified — one node added, one input added, the custom node's code replaced
Why: the mesh UV is a ribbon coordinate, the buffer is now a folded atlas, and nothing in between
was doing the conversion.
Made via: `MaterialTools.add_expression` (`MaterialExpressionCollectionParameter`),
`ObjectTools.set_properties` x3, `MaterialTools.recompile`, `AssetTools.save_assets`

| Node | Purpose |
|---|---|
| `MaterialExpressionCollectionParameter_3` | Reads `AccumulationRowCount` from the collection |
| `MaterialExpressionCustom_0` | Gained a fifth input, `Rows`; renamed to `BlurredFoldedExposure`; code replaced |

**Additive and rewire only. No `delete_expression`**, which asserts and halts the editor on a
MaterialFunction — recorded further up this file. The four existing inputs were re-supplied verbatim
alongside the new one, because the tool refuses an array append whose existing elements have also
changed, and all five were read back afterwards to confirm the original wiring survived.

Three things in the new code worth defending:

- **The tent is taken in ribbon space and every tap is folded separately.** A tap that steps past
  the end of a row folds into the start of the next one, which is the surface physically next to it.
  The blur therefore stops being a source of boundary artefacts and becomes correct across them.
- **The shader does its own bilinear along the track.** At a row end the correct neighbour is in the
  next row, which is nowhere near it in the atlas. Folding would otherwise *regress* something that
  is currently right by accident: the render target's `AddressX` defaults to `TA_Wrap`
  (`TextureDefines.h:498`), and on an unfolded ribbon wrapping past the end of the lap is the right
  neighbour, because the lap is a loop. So the two neighbouring whole-lap texels are wrapped around
  the lap, folded into the atlas separately, sampled with their U snapped to the texel centre, and
  blended by hand. A bilinear sampler handed a UV at a texel centre returns that texel exactly, so
  snapping gives an exact sample in X while leaving the sampler's bilinear in Y, which is wanted
  inside a band. The shader therefore depends on neither the target's `Filter` nor its `AddressX`,
  neither of which is set deliberately anywhere and neither of which is visible from the material.
- **`RowWidth` is `round(1.0 / TexelSize.x)`, and the `round` is load bearing.** `TexelSize.x` is
  the exact reciprocal of an integer atlas width, and its reciprocal in float32 comes back as
  14400.00001 or 14399.99999 rather than 14400. Without the `round`, `floor(texel / RowWidth)`
  lands on the wrong row at exactly the boundary, which is a one-texel error at every row seam.
  With it, the row arithmetic is exact integer arithmetic.

Across-track, `v` is clamped into the half-texel inset before folding, which reproduces clamp
addressing inside the band. Without it, `v = 0` and `v = 1` sit exactly on the boundary between two
rows' bands and a bilinear tap blends half of a neighbouring row's track edge.

### `/Game/Materials/M_SurfaceAccumulationDebug`

Asset: `/Game/Materials/M_SurfaceAccumulationDebug`
Change: modified — recompiled and re-saved, graph untouched
Why: it is the only material that calls the changed function, so recompiling it is what proves the
new HLSL compiles at all. Its cached shader map changed as a result.
Made via: `MaterialTools.recompile`, `AssetTools.save_assets`

This material is also what closes the read path in
`RubberAccumDemo.Surface.ReadPathReturnsTheMarkWhereItWasPainted`: a stroke is painted through the
C++ path, the material is drawn into a small render target with
`DrawMaterialToRenderTarget` — which supplies UV 0..1 across it
(`KismetRenderingLibrary.cpp:220`) — and the result is read back. The mark has to come back at the
ribbon coordinate it was put at. **That the material lives in `/Game` and not in the plugin means
the plugin alone cannot test its own read path end to end**, which is worth knowing before anyone
tries to move the test.

### What was verified, and what was not

Verified: that `MPC_RacetrackSurface` still carries its two original scalars alongside the new one;
that all five custom-node inputs are wired to the expressions intended, by reading every one back
after the edit; that `MF_SampleRacetrackSurface` and `M_SurfaceAccumulationDebug` both recompile
without raising; that no shader compile failure appears in the log; and that `is_dirty` returns
false for both plugin assets rather than trusting the save's return value.

Not verified at the time of writing: that the folded read produces the right *picture*. The editor
was running the pre-C9 C++ when these edits were made, so nothing had yet published a row count
other than the collection's fail-loud 0. Nothing here has been looked at on screen.

---

## 2026-09-23 — Hazard: Live Coding a change to the *size* of a static array renders garbage

Change: none. A misleading screenshot happened and was chased for twenty minutes.
Why this is here: it will happen again to anyone who uses the Live Coding MCP tool to avoid a
close-and-reopen cycle.

`RacetrackLayouts::SpaCentreline` went from 77 points to 78 as part of opening up Campus. Rather
than close the editor to build, `LiveCodingToolset.CompileLiveCoding` was used to patch the running
editor, the level was reloaded, and the circuit was captured. The capture looked right at Campus.

**A full top-down view then showed a long needle spike where the bus-stop chicane should be.**

The source was not at fault, and this was established before changing anything:

- All 78 points in the array were read back and compared against the original — only indices 47, 48
  and 49 had been replaced by the four new ones, with nothing dropped or duplicated in the
  reformatting.
- The spline was reconstructed offline with UE's own Catmull-Rom tangent rule
  (`tangent = (P[i+1] - P[i-1]) / 2`) and the maximum bulge of any segment away from its chord was
  measured at **7.0 units on a 47.4 unit chord for the original points and 6.2 on the same chord
  after the change**. A 13–15 % bulge is ordinary. Nothing remotely like a needle exists in either
  point set.

**The cause is that `UE_ARRAY_COUNT(SpaCentreline)` is a compile-time constant baked into
`BuildAuthoredLayoutPoints`.** Live Coding patches code, and the patched function was iterating 78
elements. Whether the data symbol it read was the relocated 78-element array or the original
77-element one cannot be determined after the fact, but a patched loop count against an unrelocated
array walks one element past the end and feeds whatever follows it into the spline as a point. That
is exactly what a single wild spike looks like.

**A clean build with the editor closed produces no spike**, and the same circuit then reports
`57600 x 256 texels folded into 4 rows of 14400` where the Live Coding session reported 57601.

**The rule taken from this, which extends the one already recorded on 2026-09-22.** That entry says
Live Coding does not reliably introduce new *reflection*. This adds: **it does not reliably handle a
change to the size of a static array either, and the failure is silent and looks like a geometry
bug rather than a build problem.** Live Coding is for changing the body of a function. Anything that
changes a compile-time constant — an array extent, a `constexpr`, an enum count — needs the editor
closed and a real build.

**Nothing captured from a Live Coding session should be trusted as evidence.** The Campus capture
taken in that session was discarded for the same reason, even though it happened to be correct.

---

## 2026-09-23 — The `L_SpaHarness` file-size check for leaked actors no longer works

Recorded because the hazard entry above it gives a size signature that is now wrong.

That entry says `L_SpaHarness.umap` is 737340 bytes clean and 993171 with leaked test actors, and
tells you to check the size after running tests with the editor open. **At the C9 configuration the
clean map is 2047445 bytes**, so the old figures no longer separate the two cases.

The reason is not leaked actors. `UProceduralMeshComponent` serialises its sections into the map,
and the road mesh scales with the lap: `RoadSegmentLength` is 128 cm against a 180000 cm lap, which
is **1406 rings against the 375** the interim 960 m configuration carried at 256 cm. The actor list
was checked directly and holds fifteen actors — the template lighting and sky, one
`RacetrackCircuit_0`, and no `ASplineFollowingCar` at all.

**Check the actor list, not the file size.** `SceneTools.find_actors` with an empty name filter
lists every actor in the level in one call, and a second `ARacetrackCircuit` or any
`ASplineFollowingCar` is the real signature of the leak. The file size now tracks the road mesh and
will move whenever the lap length or the segment length does.

---

## 2026-09-24 — C10.2: the debug material masks the sampled coverage by channel

One asset changed. Full reasoning in [C10.2](https://github.com/Balazs200001/RubberAccumDemo/issues/16#issuecomment-6014449380).

### `/Game/Materials/M_SurfaceAccumulationDebug`

Asset: `/Game/Materials/M_SurfaceAccumulationDebug`
Change: modified — three nodes added, one output rewired
Why: `Coverage` is a Vector3 whose R, G and B are buildup, skid and marbles, so the view was already
colour coded but could not show one channel alone. A mask multiplied in before the emissive output
isolates one channel without touching the plugin's function.
Made via: `MaterialTools.add_expression` x3, `ObjectTools.set_properties` x2,
`MaterialTools.connect_expressions` x3, `MaterialTools.connect_to_output`,
`MaterialTools.recompile`, `AssetTools.save_assets`

| Node | Purpose |
|---|---|
| `MaterialExpressionVectorParameter_0` | `DebugChannelMask`, default `(1,1,1,1)`, group `Surface Debug` |
| `MaterialExpressionComponentMask_0` | R, G, B on, A off — the parameter's float4 down to the float3 `Coverage` is |
| `MaterialExpressionMultiply_0` | `A` = `MaterialFunctionCall_0.Coverage`, `B` = the masked parameter |

`MP_EmissiveColor` now reads `MaterialExpressionMultiply_0` instead of the function call's `Coverage`
output directly. The function call node and its four output pins are untouched.

**Additive and rewire only. No `delete_expression`.** The graph went from two nodes to five and
nothing was removed, so the call that asserts and halts the editor on a MaterialFunction was never
reached. Read back after recompiling: `MP_EmissiveColor` takes `Multiply_0`, `Multiply_0.A` takes
`MaterialFunctionCall_0.Coverage`, `Multiply_0.B` takes `ComponentMask_0`, the parameter is named
`DebugChannelMask` with default `(1,1,1,1)`, and the mask is `R,G,B` true and `A` false.

**The default is `(1,1,1,1)` and that is the load-bearing choice.** A multiply by one is the
identity, so an unset or absent parameter means *show everything* rather than *show nothing*, and
the all-channels view is provably the image every capture before C10 was taken through rather than
merely one that ought to match. It is the opposite of the fail-loud default `SaturationConstant` and
`AccumulationRowCount` use, deliberately: those guard a configuration path that must run, whereas
this one has to degrade to the existing behaviour when the host does not drive it.

**Why here and not in `MF_SampleRacetrackSurface`.** The function is the seam a host integrates
against and it already offers `Coverage` plus `Buildup`, `Skid` and `Marbles` as separate outputs.
Which of those a host shows, and whether it cycles between them, is that host's business. A
debug-view selector inside the function would hand a parameter that means nothing outside this demo
to every future integrator. `M_SurfaceAccumulationDebug` lives in `/Game`, is host content, and
exists for exactly this.

**Why a mask and not a branch over the four outputs.** Three added nodes against five, one sample
rather than four, no per-pixel comparison, and the identity property above. An `If` chain would
produce the same image for the three single-channel views but would route the all-channels view
through three comparisons rather than leaving it arithmetically unchanged.

The C++ that drives it, `ARacetrackCircuit::ApplyDebugViewToRoad`, was committed before this asset
existed and worked without it:
`UMaterialInstanceDynamic::SetVectorParameterValue` writes into the instance's own parameter array
and does not validate the name against the parent, so the cycle logged and announced correctly while
the image did not change. That was the intended degradation, not an accident.

---

## 2026-09-24 — G5: marbles render as scattered dots in the debug material

**Asset changed:** `/Game/Materials/M_SurfaceAccumulationDebug`. Nine expressions added, one
existing connection rewired, nothing deleted. `/RacetrackSurface/Materials/MF_SampleRacetrackSurface`
is **not touched**, for the same reason C10.2 recorded and which is restated at the end of this
entry. Saved and verified not dirty.

The material was five nodes before this and is fourteen after.

### What was added

| Node | Class | Settings |
|---|---|---|
| `MaterialExpressionWorldPosition_0` | `WorldPosition` | defaults, absolute |
| `MaterialExpressionScalarParameter_0` | `ScalarParameter` | `MarbleDotCellCentimetres`, default **12.0**, group `Surface Debug` |
| `MaterialExpressionDivide_0` | `Divide` | world position over cell size |
| `MaterialExpressionVectorNoise_0` | `VectorNoise` | `VNF_CellnoiseALU` (the class default), Quality 1 |
| `MaterialExpressionVectorNoise_1` | `VectorNoise` | `VNF_VoronoiALU`, Quality 1 |
| `MaterialExpressionScalarParameter_1` | `ScalarParameter` | `MarbleDotMaxRadius`, default **0.42**, group `Surface Debug` |
| `MaterialExpressionCustom_0` | `Custom` | `MarbleDots`, `CMOT_Float1`, four inputs |
| `MaterialExpressionComponentMask_1` | `ComponentMask` | R and G only |
| `MaterialExpressionAppendVector_0` | `AppendVector` | |

### Wiring

```
WorldPosition ---------\
                        Divide --> VectorNoise(Cellnoise) --> Custom.CellRandom
MarbleDotCellCentremes-/      \--> VectorNoise(Voronoi)   --> Custom.Voronoi
MF_SampleRacetrackSurface.Marbles ----------------------------> Custom.Density
MarbleDotMaxRadius ------------------------------------------> Custom.MaxRadius

MF_SampleRacetrackSurface.Coverage --> ComponentMask(RG) --> AppendVector.A
Custom (dot mask) -----------------------------------------> AppendVector.B
AppendVector --> Multiply.A        (rewired; was Coverage direct)
DebugChannelMask --> ComponentMask(RGB) --> Multiply.B      (unchanged)
Multiply --> MP_EmissiveColor                               (unchanged)
```

### The Custom node's HLSL, recorded verbatim because it exists nowhere else

This text lives only inside a binary `.uasset`. It is not in any `.cpp`, `.usf` or `.ush`, so it
cannot appear in a diff and this entry is the only reviewable copy.

```hlsl
float Radius = saturate(Density * 2.0 - CellRandom.r) * MaxRadius;
float Edge = max(Radius * 0.45, 1e-4);
return Radius <= 1e-4 ? 0.0 : (1.0 - smoothstep(Radius - Edge, Radius, Voronoi.a));
```

Inputs in order: `Density`, `CellRandom`, `Voronoi`, `MaxRadius`. Output `CMOT_Float1`.

**One threshold produces both required behaviours, which is why there is no second comparison.**
The goal asked for dots that get *both denser and individually bigger* toward maximum. Because the
radius is `saturate(Density * 2 - CellRandom.r)`, a cell whose random draw is low crosses zero
first, so at low density only a few cells carry a dot at all; as density rises, more cells cross
zero *and* every radius already past zero grows. Count and size therefore both rise from a single
expression, rather than from a presence test multiplied by a size term.

### The two engine facts this rests on, checked in the 5.8 source rather than assumed

From `Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionVectorNoise.h`:

- `VNF_CellnoiseALU` is documented at lines 15-19 as *"Random color for each unit cell in 3D space.
  RGB output range 0 to 1"*. It is the only one of the five functions that is a normalised per-cell
  random, which is why the presence term reads `CellRandom.r`.
- `VNF_VoronoiALU` is documented at lines 40-46 as *"RGB = position of closest point at center of
  Voronoi cell. A = distance to closest point with range 0 to about 4"*. **The distance is in the
  alpha channel, not in RGB.** RGB is a position, so comparing it against a coverage scalar would
  not give a stable per-cell threshold — an error worth recording because it is the obvious
  misreading of this node and one of the planning passes made it.

`get_expression_output_names` reports a single unnamed output on the Voronoi node, so the whole
float4 is passed into the Custom node and `.a` is taken in HLSL. That the material compiles is the
confirmation that the output really is four components.

### Why world position rather than the ribbon UV

The mesh UV is anisotropic: U spans a quarter of the lap per atlas row, V spans the 800 cm track
width. Feeding it to the noise would make elliptical dots whose aspect changes with lap length, and
would need `CentimetresPerTexel` published to `MPC_RacetrackSurface` to correct. `WorldPosition`
needs nothing published, gives round dots by construction, and is continuous across the fold
boundaries because it never knows about them.

At the 12 cm default cell size, a 800 cm track carries about 66 cells across its width and a dot
reaches about 5 cm across at full density.

### Why this is in the host material and not the plugin's function

Unchanged from C10.2, and worth restating because the temptation is stronger here. A
scattered-blue-dot renderer is a debug read of a density. A shipping host would render marbles with
H1's parallax-occlusion material, not with dots. `MF_SampleRacetrackSurface` keeps returning
`Marbles` as a plain scalar, which is what an integrator wants, and every node above sits in
`/Game`. This also keeps clear of the asset where `MaterialTools.delete_expression` is known to
assert and halt the editor — nothing in this change deletes anything anywhere.

### Not verified

- **Not seen on screen.** The graph compiles and the wiring was read back and confirmed, but no run
  has been looked at, and whether the dot size and cell size read well is a judgement rather than a
  check.
- **`MarbleDotCellCentimetres` at 12.0 and `MarbleDotMaxRadius` at 0.42 are starting values chosen
  by arithmetic, not by eye.** They are instance parameters in the `Surface Debug` group, so they
  are an A/B with a slider rather than a rebuild.
- **The rewire changed what the all-channels view shows.** `Multiply.A` was `Coverage` directly and
  is now `AppendVector(Coverage.rg, MarbleDots)`. Rubber and skid are bit-identical, but the blue
  channel of the all-channels view is now a dot mask rather than a smooth coverage — so captures
  taken before this change are still comparable in R and G and are **not** comparable in B.

---

## 2026-09-24 — G5 corrected: the dot mapping made a real marble field invisible

**Asset changed:** `/Game/Materials/M_SurfaceAccumulationDebug`, the `MarbleDots` Custom node's
`Code` only. No nodes added, removed or rewired. Saved.

**Why, measured rather than judged by eye.**
`RubberAccumDemo.Demo.MarblesReachTheBufferOnTheFoldedLayout` read the marble channel back off the
real 14401 x 1024 four-row atlas and found **0.064880 exposure, 0.1769 coverage** — a genuine
marble field, 31 % of rubber's 0.5777 on the same run. Nothing was visible on screen.

The first mapping was `Radius = saturate(Density * 2 - CellRandom.r) * MaxRadius`. At a density of
0.1769 that lights only cells whose random draw is under 0.354, about a third of them, each with a
radius under 0.15 cell units. That is roughly **2 % of the surface in 1.8 cm specks** — invisible,
while a flat colour at the same density would have read clearly.

The single-threshold form was chosen on 2026-09-24 because it produced both required behaviours
from one expression. It does, but only across the wrong range: it needs a density near 0.5 before
most cells carry a dot, and the measured working range is **0.1 to 0.3**.

### The corrected HLSL, recorded verbatim

```hlsl
float Lit = step(CellRandom.r, saturate(Density * 2.5));
float Radius = MaxRadius * (0.4 + 0.6 * saturate(Density * 1.5)) * Lit;
float Edge = max(Radius * 0.45, 1e-4);
return Radius <= 1e-4 ? 0.0 : (1.0 - smoothstep(Radius - Edge, Radius, Voronoi.a));
```

Inputs and output are unchanged: `Density`, `CellRandom`, `Voronoi`, `MaxRadius`, `CMOT_Float1`.

Presence and size are separated again, which is what the original plan called for before the
single-threshold form replaced it:

- **Presence** is `CellRandom.r < saturate(Density * 2.5)`, so the share of cells carrying a dot
  reaches 100 % at a density of 0.4 rather than 0.5.
- **Size** is `0.4 + 0.6 * saturate(Density * 1.5)`, so a lit cell always gets at least 40 % of the
  maximum radius. A dot that exists is visible; it then grows to full by a density of 0.67.

At the measured 0.1769 that is **44 % of cells at 56 % of maximum radius**, about 5.6 cm across in
a 12 cm cell, against 35 % of cells at 1.8 cm before.

**The lesson is worth keeping separate from the fix.** The first mapping was not wrong in form; it
was calibrated against a density range nobody had measured yet. The measurement existed one commit
later and contradicted it. Presentation curves for a quantity this project generates should be set
after reading the quantity back, not before.

### Not verified

- **Not seen on screen.** The graph compiles and the asset is saved; whether 44 % of cells at 5.6 cm
  reads as marbles rather than as noise is a judgement, and `MarbleDotCellCentimetres` and
  `MarbleDotMaxRadius` remain instance parameters for exactly that reason.

---

## 2026-09-24 — G5 bypassed: marbles render as flat coverage, like rubber and skid

**Asset changed:** `/Game/Materials/M_SurfaceAccumulationDebug`. One connection rewired:
`Multiply.A` is driven by `MF_SampleRacetrackSurface.Coverage` again, as it was before G5. Nothing
deleted. Saved.

Requested directly after two rounds of the dot presentation failing to show anything on screen. The
blue channel is now displayed exactly the way red and green are.

**The nine dot nodes are left in the graph, orphaned.** `WorldPosition`, the two `VectorNoise`
nodes, the `Divide`, the two `ScalarParameter`s, the `Custom` node, the `ComponentMask` and the
`AppendVector` are all still present and still wired to each other; only the final
`AppendVector -> Multiply.A` link is gone. An unconnected expression compiles to nothing, so this
costs no instructions, and restoring the dots later is one connection rather than nine nodes.

**This is also a deliberate bisection.** The marble channel measurably holds 0.1769 coverage in the
buffer on this exact layout, and nothing was visible on screen. Removing the Custom node from the
path leaves the graph in its precise pre-G5 state, so the next run discriminates:

- **Blue appears** — the dot mapping was the whole problem, and the fix is a presentation curve.
- **Blue still absent** — the fault is upstream of anything added on 2026-09-24, in a read path
  whose blue channel has never once carried data and therefore has never been exercised.

**Checked while bisecting, and all sound:**

- The placed circuit in `/Game/Maps/L_SpaHarness` matches the diagnostic test exactly: `Spa`,
  180000 cm, `TrackWidth` 800, 3.125 cm per texel, `CarCount` 20, `CarClass` the C++
  `ASplineFollowingCar` rather than a Blueprint, `RoadMaterial` this material. So the test is not
  measuring a different scene from the one on screen.
- `BlurredFoldedExposure` samples `.rgb` and returns `CMOT_Float3`, so blue is folded and blurred
  identically to red.
- The saturation chain is `Custom -> Multiply -> Exponential -> OneMinus -> Coverage`, all float3.
  No mask drops a component.

---

## 2026-09-24 — F0: `BlurredFoldedExposure`'s HLSL, read back and recorded verbatim

Asset: `/RacetrackSurface/Materials/MF_SampleRacetrackSurface`
Change: **none. This is a read, not an edit.** No property was set, nothing was saved, nothing is
dirty.
Why: phase F writes a second reader of the accumulation buffer in C++, and the only existing
reader is this Custom node, whose source existed nowhere outside the `.uasset`. C9 recorded
"keeping the two in step" as this project's standing risk; you cannot keep in step with something
you cannot read. The G5 marble-dot node was quoted verbatim "because it exists nowhere else" and
this one was not, which was an inconsistency rather than a decision.
Made via: `ObjectTools.get_properties` on `MaterialExpressionCustom_0`, properties `Code`,
`Description`, `OutputType`, `Inputs`.

```hlsl
float RowTotal = max(Rows, 1.0);
float RowWidth = round(1.0 / max(TexelSize.x, 0.000001));
float LapTexels = RowTotal * RowWidth;
float2 RibbonTexel = float2(1.0 / LapTexels, TexelSize.y * RowTotal);
float HalfBand = TexelSize.y * RowTotal * 0.5;
float3 Sum = 0;
float Weight = 0;
[unroll] for (int y = -1; y <= 1; ++y)
{
	[unroll] for (int x = -1; x <= 1; ++x)
	{
		float w = (x == 0 ? 2.0 : 1.0) * (y == 0 ? 2.0 : 1.0);
		float2 Ribbon = UV + float2(x, y) * RibbonTexel * Radius;
		float Along = frac(Ribbon.x);
		float Across = clamp(Ribbon.y, HalfBand, 1.0 - HalfBand);
		float Sampled = Along * LapTexels - 0.5;
		float Lower = floor(Sampled);
		float Blend = Sampled - Lower;
		float FirstTexel = Lower < 0.0 ? LapTexels - 1.0 : Lower;
		float SecondTexel = Lower + 1.0 >= LapTexels ? 0.0 : Lower + 1.0;
		float FirstRow = floor(FirstTexel / RowWidth);
		float SecondRow = floor(SecondTexel / RowWidth);
		float2 FirstUV = float2((FirstTexel - FirstRow * RowWidth + 0.5) / RowWidth, (FirstRow + Across) / RowTotal);
		float2 SecondUV = float2((SecondTexel - SecondRow * RowWidth + 0.5) / RowWidth, (SecondRow + Across) / RowTotal);
		float3 First = Texture2DSampleLevel(Tex, TexSampler, FirstUV, 0).rgb;
		float3 Second = Texture2DSampleLevel(Tex, TexSampler, SecondUV, 0).rgb;
		Sum += lerp(First, Second, Blend) * w;
		Weight += w;
	}
}
return Sum / Weight;
```

`Description` is `BlurredFoldedExposure`, `OutputType` is `CMOT_Float3`, and the five inputs are
wired as C9 recorded them: `Tex` from the `AccumulationTarget` function input, `UV` from
`TextureCoordinate_0`, `TexelSize` from `TextureProperty_0`, `Radius` from the
`SurfaceBlurTexels` scalar parameter, `Rows` from the `AccumulationRowCount` collection parameter.

### Three things the prose record did not say, and one it said slightly wrong

**The kernel is separable and its weights are `(x==0?2:1) * (y==0?2:1)`**, which is
`[1 2 1] ⊗ [1 2 1]`, summing to 16. It is normalised by an accumulated `Weight` rather than by a
literal `/16`, so the node stays correct if the loop bounds ever change.

**`Radius` scales the tap *spacing*, not the weights.** That is structurally the same knob phase F
wants for the contact patch, which makes the relationship between the display read and the query
read much simpler than PLAN.md assumed when it wrote F1: they are *the same kernel at two
radii*. The material's `SurfaceBlurTexels` defaults to 1 texel for looks; the query uses the
contact patch, 6.4 texels at 3.125 cm per texel for a 20 cm patch. **They can therefore be forced
into exact agreement for F5's overlay check by setting `SurfaceBlurTexels` to the patch width**,
which is a far stronger check than comparing two different kernels and arguing they ought to be
close.

**The lap is `RowTotal * RowWidth` texels, not `AlongTrackTexels()`.** On the demo ribbon those
differ: the atlas is 14401 wide over 4 rows, so the shader's lap is 57604 texels, while
`FSurfaceTrackRibbon::AlongTrackTexels()` returns 57601. The three-texel difference is padding
introduced by `TexelsPerRow()`'s `DivideAndRoundUp`. It is **not a bug** — the C++ paint path
normalises through `RibbonToAtlasInRow`, which uses the same `RowCount`-based convention, so the
write and the read already agree with each other, and the effective texel size is 0.005 % off the
configured one, consistently, on both sides. **It is a trap for F1**: a C++ gather that reached for
`AlongTrackTexels()` because it is the obvious-looking accessor would land a fraction of a texel
off, which no test written against itself would catch.

**Corrected from the C9 entry:** it says "the shader therefore depends on neither the target's
`Filter` nor its `AddressX`". That holds for the along-track axis, where the manual two-tap lerp
and the `+0.5` snap make the result exact, and for `AddressX`, which `frac` and the explicit wrap
make irrelevant. It does **not** hold across-track: `Across` is passed through unsnapped and the
blend in Y is the hardware sampler's, which is what the same entry says two sentences earlier
("leaving the sampler's bilinear in Y, which is wanted inside a band"). The summary sentence
overstates the guarantee; the detailed sentence is right. Phase F inherits this and it is
harmless either way, because a canvas gather samples the same texture through the same sampler, so
the two readers move together whatever `Filter` is set to.

### What was verified, and what was not

Verified: that the node read back is the one C9 edited, by its `Description`, its `CMOT_Float3`
output and its five named inputs; and that nothing was written, because only `get_properties` was
called.

Not verified: that this is the code the *cooked* shader contains. It is the code in the asset, and
the asset recompiled clean on 2026-09-23, but no shader map was inspected.

---

## 2026-10-06 — W3.2: the debug material's inverse display curve, and a coverage probe

**Asset changed:** `/Game/Materials/M_SurfaceAccumulationDebug`. One expression added,
`MaterialExpressionCustom_1`, description `InverseDisplayResponse`; one connection rewired. Nothing
deleted. Recompiled clean, saved, verified not dirty.

**Asset created:** `/Game/Materials/M_SurfaceCoverageProbe`. Saved, verified not dirty.

**Checked, not changed:** `/Game/Blueprints/BP_SurfaceRegionDisplay` compiles with warnings treated
as errors and is not dirty afterwards. This is the recompile check owed since W1.1, when the
Blueprint library's write wrappers lost their `Channel` parameter.

**Not touched:** `/RacetrackSurface/Materials/MF_SampleRacetrackSurface` and
`/RacetrackSurface/Materials/MPC_RacetrackSurface`, both only read.

### Why

Decision E: the debug view is linear in coverage, in the encoded pixel value. Decision E3, taken on
2026-10-06 after `MEASUREMENTS.md` "W3.1 again" showed local exposure making a thin mark on black
twice as bright as a wide band of the same coverage: local exposure and bloom are neutralised in C++
by an unbound post-process component the circuit owns, and the material undoes only what then
remains, which acts per pixel: the fixed exposure of 2.4, the filmic tonemapper and the sRGB
encoding.

### The debug material's wiring after the change

```
MF_SampleRacetrackSurface.Coverage --> Custom (InverseDisplayResponse).Coverage     (new)
Custom (InverseDisplayResponse) ------> Multiply.A                                  (rewired; was Coverage direct)
DebugChannelMask --> ComponentMask(RGB) --> Multiply.B                              (unchanged)
Multiply --> MP_EmissiveColor                                                       (unchanged)
```

The nine orphaned marble-dot nodes from G5 are still present and still unconnected.

### The Custom node's HLSL, recorded verbatim because it exists nowhere else

`MaterialExpressionCustom_1`, one input `Coverage` (float3 from the function), output `CMOT_Float3`:

```hlsl
static const float3 EmissiveAtRootCoverage[33] =
{
	float3(0.000000, 0.000000, 0.000000),
	float3(0.018101, 0.015370, 0.017533),
	float3(0.025961, 0.020739, 0.021845),
	float3(0.032875, 0.025915, 0.026600),
	float3(0.038339, 0.030174, 0.031348),
	float3(0.044709, 0.035053, 0.035833),
	float3(0.050053, 0.038466, 0.039363),
	float3(0.055811, 0.042889, 0.044038),
	float3(0.062183, 0.047552, 0.048258),
	float3(0.068876, 0.052230, 0.052734),
	float3(0.076068, 0.057361, 0.057736),
	float3(0.084356, 0.062865, 0.063031),
	float3(0.093051, 0.068732, 0.068653),
	float3(0.101460, 0.075052, 0.074822),
	float3(0.112091, 0.082019, 0.081577),
	float3(0.122967, 0.089945, 0.089115),
	float3(0.133289, 0.097215, 0.096439),
	float3(0.144901, 0.105839, 0.104730),
	float3(0.158257, 0.115520, 0.114064),
	float3(0.171252, 0.125618, 0.123956),
	float3(0.185028, 0.136152, 0.134425),
	float3(0.199983, 0.148243, 0.146199),
	float3(0.217467, 0.161661, 0.159378),
	float3(0.235734, 0.177080, 0.174584),
	float3(0.254742, 0.194723, 0.191696),
	float3(0.276460, 0.215250, 0.211613),
	float3(0.299778, 0.239338, 0.234574),
	float3(0.328947, 0.269552, 0.264214),
	float3(0.360744, 0.304588, 0.298437),
	float3(0.397184, 0.351555, 0.343366),
	float3(0.442319, 0.410605, 0.399539),
	float3(0.495917, 0.493459, 0.479194),
	float3(0.558750, 0.612727, 0.587000)
};

float3 Position = sqrt(saturate(Coverage)) * 32.0;
float3 Emissive = 0.0;

[unroll]
for (int Channel = 0; Channel < 3; ++Channel)
{
	int Lower = min((int)Position[Channel], 31);
	Emissive[Channel] = lerp(EmissiveAtRootCoverage[Lower][Channel], EmissiveAtRootCoverage[Lower + 1][Channel], Position[Channel] - Lower);
}

return Emissive;
```

### How the table was made, so it can be made again

1. **The raw response.** `RubberAccumDemo.Surface.DebugViewResponse` drew the material before this
   change (coverage times the channel mask, nothing else) on a plane filling a capture with a
   persistent view state, with local exposure contrast 1 and bloom 0 on the capture. Coverage was
   held at 0.5 and the channel mask scaled, so emissive ran from 0 to 3 in 48 steps, per primary.
   The levels are in `MEASUREMENTS.md` under W3.2. The uniform field and a stripe on black agreed
   within 0.8 levels at every step.
2. **The ceilings.** A bright pure primary washes toward white: green at emissive 1.0 shows red 60.
   Each channel's ceiling is the level it reaches where the other two channels first exceed 5
   levels above the dither floor of 0.2, linearly interpolated between steps, and red is capped at
   254 so the curve stays below the clip: **red 254.0, green 235.08, blue 210.26**. Coverage 1 shows
   each channel at its ceiling and coverage `c` at `c` times it.
3. **The knots.** For knot `i` of 32, coverage `(i/32)^2`, the emissive is the measured response
   inverted at `ceiling * coverage`, by linear interpolation between the sweep's steps. Knot 0 is 0.
4. **Why square-root spacing.** The filmic toe crushes the bottom: nothing below emissive 0.0175
   lifts a channel off black. Uniform knots in coverage left the simulated screen up to 3.3 levels
   below target near coverage 0.02; knots uniform in the square root of coverage put most of them
   in the toe and bring the simulated worst case to 0.49, 1.13 and 0.62 levels for red, green and
   blue, at coverages 0.75, 0.98 and 0.97.

### The probe: `/Game/Materials/M_SurfaceCoverageProbe`

Unlit, opaque, surface domain, one-sided, as the debug material is.

| Node | Class | Settings |
|---|---|---|
| `MaterialExpressionTextureObjectParameter_0` | `TextureObjectParameter` | `AccumulationTarget`, default `/RacetrackSurface/Materials/T_EmptyAccumulation`, sampler `LinearColor` |
| `MaterialExpressionMaterialFunctionCall_0` | `MaterialFunctionCall` | `/RacetrackSurface/Materials/MF_SampleRacetrackSurface` |
| `MaterialExpressionVectorParameter_0` | `VectorParameter` | `DebugChannelMask`, default `(1,1,1,1)`, group `Surface Debug` |
| `MaterialExpressionComponentMask_0` | `ComponentMask` | R, G, B |
| `MaterialExpressionMultiply_0` | `Multiply` | |

```
AccumulationTarget --> MF_SampleRacetrackSurface.AccumulationTarget
MF_SampleRacetrackSurface.Coverage --> Multiply.A
DebugChannelMask --> ComponentMask(RGB) --> Multiply.B
Multiply --> MP_EmissiveColor
```

**One addition beyond decision E2**, which specified coverage straight to emissive: the channel mask
multiply, copied from the debug material. At its default of 1 the probe is still exactly coverage to
emissive, which is what the two read-path tests now draw. It is there so the response harness can
keep sweeping emissive through the raw chain, which the debug material no longer exposes now that it
carries the curve. `SurfaceBlurTexels` reaches the probe because it is a parameter inside
`MF_SampleRacetrackSurface`, so the gather test's blur radius still applies.

### What was verified, and what was not

Verified: both materials recompile; every connection above was read back with
`get_expression_inputs` and `get_property_input`; both are saved and not dirty. Run inside the editor
before closing it, with local exposure and bloom neutralised on the capture, the debug material
reads within 1.4 levels of `ceiling * coverage` at every coverage from 0.01 to 0.99, in all three
channels, as a uniform field and as a stripe on black.

Verified after the full build that added the circuit's post-process component (`21c6446`): the same
assertions with nothing set on the capture, so with the chain the world gives every view, pass in
the headless suite within the same 1.4 levels.

Not verified: the screen itself, in a running demo; the all-channels view, where the tonemapper does
not act on mixed colours channel by channel; and the cooked shader.

---

## 2026-10-06 — E4: a lit asphalt road material and its three textures

**Assets created**, all saved and verified not dirty:

| Asset | What |
|---|---|
| `/Game/Materials/Asphalt/T_AsphaltFresh_BaseColor` | Base colour, from `T_sfrofg0a_2K_B.png` |
| `/Game/Materials/Asphalt/T_AsphaltFresh_Normal` | Normal, from `T_sfrofg0a_2K_N.png` |
| `/Game/Materials/Asphalt/T_AsphaltFresh_ORM` | AO, roughness and metallic packed, from `T_sfrofg0a_2K_ORM.png` |
| `/Game/Materials/M_SurfaceAsphalt` | The lit road material the circuit switches to on V |

**Not touched, only read:** `/RacetrackSurface/Materials/MF_SampleRacetrackSurface`,
`/RacetrackSurface/Materials/T_EmptyAccumulation` and `/Game/Materials/M_SurfaceAccumulationDebug`.

Made via: `TextureTools.import_file`, `ObjectTools.set_properties` and `get_properties`,
`MaterialTools.create_material`, `add_expression`, `connect_expressions`, `connect_to_output`,
`recompile`, `get_expression_inputs`, `get_property_input`, then `AssetTools.save_assets` with the
four paths given explicitly and `AssetTools.is_dirty` on each.

### Source

Fab Megascans surface *Asphalt Fresh*, id `sfrofg0a`, 2k, the `_ue_mid` package, downloaded by the
user and unzipped to `C:\Users\balu2\Downloads\asphalt_fresh_sfrofg0a_2k_ue_mid\Textures\`. The import
records that absolute path as each texture's source, so a reimport needs the same download in the
same place. The package's `sfrofg0a.json` gives a 2 x 2 m tileable scan with 2.7 cm of relief.

Both GitHub repositories, `RubberAccumDemo` and `RacetrackSurfacePlugin`, are **private**, checked
with `gh repo view --json visibility` on 2026-10-06. The E4 plan section said the plugin repository
was public, and it is not. The textures are in host content because they are demo dressing, which
holds either way.

### What Interchange chose on import, and what was changed

| Texture | Interchange chose | Changed to | Why |
|---|---|---|---|
| BaseColor | `TC_Default`, sRGB on | unchanged | Colour data |
| Normal | `TC_Normalmap`, sRGB off, `TEXTUREGROUP_WorldNormalMap`, green not flipped | `bFlipGreenChannel` true | The map is OpenGL convention, below |
| ORM | `TC_Default`, **sRGB on** | `TC_Masks`, sRGB off | Packed linear data. As imported, roughness and AO would have been decoded as colour |

All three read back after the change. **Interchange does not recognise a packed mask texture** and
imports it as colour. The package's glTF marks the ORM image `"compression": "mask"`, but that hint
only reaches Unreal if the glTF itself is imported, and here the PNG was. Check every packed texture
after import.

**The normal convention was measured, not assumed.** Each normal channel minus 0.5 was correlated
with the central difference of the package's own displacement map: red against the column gradient
-0.77 and green against the row gradient +0.77 on the PNG, -0.83 and +0.83 on the JPG. Positive on
green means green points up the image, the OpenGL convention. Unreal's is DirectX.

**The ORM packing was checked against the separate maps.** Red correlates 0.947 with
`Asphalt_Fresh_sfrofg0a_2K_AO.jpg`, green 0.932 with the roughness JPG, and blue is 0 at every texel.
Green averages 0.80, which is the roughness the rubber response lowers from.

### The material, `/Game/Materials/M_SurfaceAsphalt`

Surface domain, opaque, default lit, one-sided, tangent-space normal. All of these are the new
material defaults, read back rather than set.

| Node | Class | Settings |
|---|---|---|
| `MaterialExpressionTextureObjectParameter_0` | `TextureObjectParameter` | `AccumulationTarget`, default `/RacetrackSurface/Materials/T_EmptyAccumulation`, sampler `LinearColor`, as the debug material |
| `MaterialExpressionMaterialFunctionCall_0` | `MaterialFunctionCall` | `/RacetrackSurface/Materials/MF_SampleRacetrackSurface` |
| `MaterialExpressionTextureCoordinate_0` | `TextureCoordinate` | UV0 |
| `MaterialExpressionScalarParameter_0` | `ScalarParameter` | `AsphaltTilesAlongLap`, default 1, group `Asphalt Tiling` |
| `MaterialExpressionScalarParameter_1` | `ScalarParameter` | `AsphaltTilesAcross`, default 1, group `Asphalt Tiling` |
| `MaterialExpressionAppendVector_0` | `AppendVector` | the two tile counts |
| `MaterialExpressionMultiply_0` | `Multiply` | UV0 times the tile counts |
| `MaterialExpressionTextureSample_0` | `TextureSample` | `T_AsphaltFresh_BaseColor`, sampler `Color` |
| `MaterialExpressionTextureSample_1` | `TextureSample` | `T_AsphaltFresh_Normal`, sampler `Normal` |
| `MaterialExpressionTextureSample_2` | `TextureSample` | `T_AsphaltFresh_ORM`, sampler `Masks` |
| `MaterialExpressionVectorParameter_0` | `VectorParameter` | `RubberColour` |
| `MaterialExpressionVectorParameter_1` | `VectorParameter` | `MarbleColour` |
| `MaterialExpressionScalarParameter_2` to `_8` | `ScalarParameter` | in the parameter table below, in that order |
| `MaterialExpressionMultiply_1` to `_4` | `Multiply` | coverage times strength |
| `MaterialExpressionLinearInterpolate_0` to `_6` | `LinearInterpolate` | |
| `MaterialExpressionConstant3Vector_0` | `Constant3Vector` | (0, 0, 1) |

Wiring, every connection read back with `get_expression_inputs` and `get_property_input`:

```
AccumulationTarget --> MF_SampleRacetrackSurface.AccumulationTarget
UV0 x Append(AsphaltTilesAlongLap, AsphaltTilesAcross) --> UVs of all three texture samples

BaseColor.RGB --> Lerp_0(B RubberColour, Alpha Buildup x BuildupDarkening)
              --> Lerp_1(B RubberColour, Alpha Skid x SkidDarkening)
              --> Lerp_2(B MarbleColour, Alpha Marbles x MarbleDarkening)   --> MP_BaseColor
ORM.G         --> Lerp_3(B BuildupRoughness, Alpha Buildup)
              --> Lerp_4(B SkidRoughness, Alpha Skid)
              --> Lerp_5(B MarbleRoughness, Alpha Marbles)                  --> MP_Roughness
Normal.RGB    --> Lerp_6(B (0,0,1), Alpha Buildup x BuildupNormalFlattening) --> MP_Normal
ORM.R                                                                       --> MP_AmbientOcclusion
```

`MP_Specular`, `MP_Metallic` and `MP_EmissiveColor` are unconnected, read back as `None`. Specular
per the E4 decision, metallic because the ORM's blue is 0 everywhere, emissive because the view is lit.

| Parameter | Default | What it does |
|---|---|---|
| `AsphaltTilesAlongLap` | 1 | The circuit sets lap length over 200 cm, rounded. 1 means unset, and shows one tile stretched over the lap |
| `AsphaltTilesAcross` | 1 | The circuit sets track width over 200 cm |
| `RubberColour` | (0.010, 0.010, 0.011) linear | What laid rubber and skid marks darken toward |
| `MarbleColour` | (0.022, 0.020, 0.018) linear | What marbles dull toward, a little lighter than laid rubber |
| `BuildupDarkening` | 0.85 | How far full buildup goes toward `RubberColour` |
| `SkidDarkening` | 1.0 | How far a full skid mark goes toward it |
| `MarbleDarkening` | 0.8 | How far full marbles go toward `MarbleColour` |
| `BuildupRoughness` | 0.55 | Roughness at full buildup, against the asphalt's own 0.80 |
| `SkidRoughness` | 0.45 | Roughness at a full skid mark |
| `MarbleRoughness` | 0.95 | Roughness at full marbles, loose and matte |
| `BuildupNormalFlattening` | 0.7 | How far full buildup flattens the asphalt's normal, rubber filling the pores |

The defaults are starting points. The scan's average colour is `#24211D`, about 0.015 linear, so the
asphalt is already nearly as dark as rubber on average. What the darkening mostly removes is the
light aggregate, and the roughness drop is expected to be the more visible change. Whether it reads
as laid rubber is the user's judgement.

**Plain nodes rather than a Custom node.** A Custom node's HLSL is stored inside the `.uasset` just
as the graph is, so it would not be any more reviewable in a diff. These are seven lerps of
presentation, and the full graph is recorded above.

### What was verified, and what was not

Verified: the material recompiles without error, which also confirms each sampler type agrees with
its texture's compression; every connection was read back; all four assets are saved and not dirty;
`git status` showed only these four new files, so nothing else was saved alongside them; the
material's thumbnail renders as a lit, textured sphere,
`2627-Y3A-PR-243663/assets/media/e4-asphalt-material-thumbnail.png`.

Not verified at this point: the material on the road with the tile counts set and rubber on it,
under Lumen in a running demo, which needs the C++ built into the editor target; and the cooked
shader.

## 2026-10-08 — C11.2 and C11.8: the Vehicle feature pack, and the circuit set to drive its sports car

[C11.2](https://github.com/Balazs200001/RubberAccumDemo/issues/120) and
[C11.8](https://github.com/Balazs200001/RubberAccumDemo/issues/126).

### The import, done by the user in the editor

MCP has no tool that adds a feature pack, so the user did it by hand:
**Content Browser → + Add → Add Feature or Content Pack… → Blueprint → Vehicle → Add to Project**.
It installs `Engine/../FeaturePacks/TP_VehicleAdvBP.upack` by copying `Templates/TP_VehicleAdvBP/Content`
into the project. It added 152 files, about 44 MB, under:

| Folder | What it holds |
|---|---|
| `/Game/VehicleTemplate/` | The Blueprints (`BP_VehicleAdvPawnBase`, `BP_VehicleAdvSportsCar`, `BP_VehicleAdvOffroadCar`, `BP_VehicleAdvGameMode`, `BP_VehicleAdvPlayerController`, the wheel Blueprints, the torque curves), the input (`IMC_Vehicle_Default`, `IMC_Vehicle_MouseLook`, `IMC_SteeringWheel`, `IA_Throttle`, `IA_Brake`, `IA_Steering`, `IA_Handbrake`, `IA_LookAround`, `IA_ToggleCamera`, `IA_Reset`, `IA_Headlights`), the touch UI, and the two template maps `Lvl_VehicleBasic` and `VehicleBasic` with their materials and meshes |
| `/Game/Vehicles/` | The shared vehicle content: the sports car's and offroad car's meshes, materials, textures and physical materials. About 40 MB of the 44 |
| `/Game/Input/` | A shared input pack: actions and touch |
| `/Game/__ExternalActors__/` and `/Game/__ExternalObjects__/` | `Lvl_VehicleBasic`'s actors, saved one file per actor |

**The import also rewrote `Config/DefaultInput.ini`.** That is
`Engine/Source/Editor/UnrealEd/Private/Factories/PackFactory.cpp:281-348`, which saves the whole input
settings object. It added 22 legacy action and axis mappings (Handbrake, SwitchCamera, ResetVR,
MoveForward, MoveRight, LookUp, LookRight), 3 gesture axis configs and 3 settings lines.

**Reverted with `git checkout -- Config/DefaultInput.ini`, as the user decided.** Before the revert:
- Nothing in `Source/` uses those mapping names.
- `BP_VehicleAdvPawnBase`'s event graph, read back with `read_graph_dsl`, listens only to Enhanced
  Input events (`IA_Throttle`, `IA_Steering`, `IA_Brake`, `IA_Handbrake`, `IA_LookAround`,
  `IA_ToggleCamera`, `IA_Reset`) and touch events, and no legacy axis or action events.

The running editor still holds the added mappings in memory until it restarts, which is harmless. If
anyone saves Project Settings → Input before then, they are written back. Check `git status` after.

### What the car is, read back over MCP

- `BP_VehicleAdvSportsCar`'s parent is `BP_VehicleAdvPawnBase`, whose parent is
  `/Script/ChaosVehicles.WheeledVehiclePawn`. No template C++ is involved.
- `AutoPossessPlayer` is `Disabled` and `AutoPossessAI` is `PlacedInWorld`, so a spawned car is
  neither possessed nor given an AI controller by itself.
- `VehicleMovementComp.WheelSetups`, in index order:

| Index | Bone | Wheel class | Radius | Width | Max brake torque | Handbrake |
|---|---|---|---|---|---|---|
| 0 | `Phys_Wheel_FL` | `SportsCar_WheelsFront` | 39 cm | 35 cm | 4500 | no |
| 1 | `Phys_Wheel_FR` | `SportsCar_WheelsFront` | 39 cm | 35 cm | 4500 | no |
| 2 | `Phys_Wheel_BL` | `SportsCar_WheelsRear` | 40 cm | 40 cm | 1500 | 6000 |
| 3 | `Phys_Wheel_BR` | `SportsCar_WheelsRear` | 40 cm | 40 cm | 1500 | 6000 |

This is the same order as `ASplineFollowingCar`'s `FrontLeft, FrontRight, RearLeft, RearRight`
(`Source/RubberAccumDemo/SplineFollowingCar.h:13-16`), so the overlay's four slots match by index. The
contact width written to the surface is each wheel's own `WheelWidth`, 35 cm front and 40 cm rear.

**`IMC_Vehicle_Default`'s keyboard bindings**, read from `DefaultKeyMappings`: W and Up throttle, S and
Down brake, A, D, Left and Right steering, SpaceBar handbrake, Tab camera toggle, BackSpace reset, and
L headlights. The rest are gamepad and VR. **The drive key is F**, chosen by the user from the keys
free in this context, in the circuit's own keys (`. , ] [ P M N C V`), in the fly pawn's and in the
engine's `DebugExecBindings`. It is bound in host commit `4592d82`.

Mouse look while driving is not wired. That is `IMC_Vehicle_MouseLook`, a second context; the host
adds only `IMC_Vehicle_Default`.

### `L_SpaHarness`: two new overrides on `RacetrackCircuit_0`

| Property | Was | Now |
|---|---|---|
| `DriveVehicleClass` | None | `/Game/VehicleTemplate/Blueprints/SportsCar/BP_VehicleAdvSportsCar.BP_VehicleAdvSportsCar_C` |
| `DriveMappingContext` | None | `/Game/VehicleTemplate/Input/IMC_Vehicle_Default.IMC_Vehicle_Default` |

Both were read back after setting. The level was saved, and `git status` showed only
`Content/Maps/L_SpaHarness.umap` modified.

Read back unchanged at the same time: `Layout` Spa, `AuthoredLayoutLapLength` 180000, `TrackWidth`
800, `RoadSegmentLength` 128, `EdgeWallHeight` 150, `CarCount` 20, `ElevationExaggeration` 2.5. The
drive calibration values were left at their class defaults, which are uncalibrated placeholders.

**Not verified here:** the car in PIE, which is the next step.

### Corrected, 2026-10-08: the import broke the sports car, and four meshes were restored

The first drive failed: the car shook violently, flipped and fell off the world. **The cause was the
editor, not the circuit.** While the pack's files were being copied, the editor's auto-reimport saw
the FBX source files arrive in `Content/Vehicles/SportsCar/` and reimported four assets from them:
`SKM_SportsCar`, `SM_SportsCar`, `SM_SportsCar_Glass` and `SM_SportsCar_Wheel`. The log line is
`AssetReimport: Reimported asset SKM_SportsCar from …/SKM_SportsCar.FBX`. Those reimported versions
are what `199a348` committed.

**How it was found**, all in Simulate with no player and no input:

| Placed | On | After about 3 s |
|---|---|---|
| Sports car | Spa road, spline point 0 | 18 m sideways, 3.6 m up, rolled 83° |
| Sports car | A plain cube floor (`/Engine/BasicShapes/Cube` scaled to 100 m by 100 m), far below the track | Rose 3.7 m, rolled 100°, fell off |
| Offroad car, not reimported | The same floor | Settled flat, roll 0.0° |

So the fault was in the car asset, not in the road, the walls or the C++. A byte comparison against
`Engine/../Templates/TemplateResources/Standard/Vehicles/Content/SportsCar/` showed exactly those
four `.uasset` files differing. `SKM_SportsCar` was 68653 bytes against the template's 40444. Every
other file was identical, including `PA_SportsCar` and the FBX sources.

**Fix.** With the user's explicit permission to work in `Content/` with file tools, and with the
editor closed:
- The four original `.uasset` files were copied back from the template folder, each checked
  byte-identical afterwards.
- The four `.FBX` files were deleted from `Content/Vehicles/SportsCar/`, so nothing can reimport them
  again. They were never in git, because the root `.gitignore` whitelists file types and FBX is not
  on the list.

The commit is `3d1388f`.

**Verified after the fix, in Simulate:** the sports car settles on the cube floor (18.5 cm up, pitch
0.49°, roll −0.10°) and on the Spa road at spline point 0 (19.7 cm above the surface, pitch 1.84°
from the road's own grade, roll −0.53°). The test actors were removed and the level was not saved.

**For a rebuild from scratch:** before adding the pack, untick **Editor Preferences → General →
Loading & Saving → Auto Reimport → Monitor Content Directories** (`bMonitorContentDirectories`,
`Engine/Source/Editor/UnrealEd/Classes/Settings/EditorLoadingSavingSettings.h:128`), or restore the
four meshes as above afterwards.

---

## 2026-10-08 — H1 step 1: the marble parameters on the asphalt retuned

**Asset changed:** `/Game/Materials/M_SurfaceAsphalt`. Two parameter defaults only. No nodes were
added, removed or rewired. Saved, verified not dirty, and `git status` showed only this `.uasset`
changed. Commit `c3ba409`.

Why: [H1](https://github.com/Balazs200001/RubberAccumDemo/issues/73) step 1, chosen by the user on
2026-10-08. The baseline in `MEASUREMENTS.md` (*H1 baseline*) showed full marbles leaving the
asphalt no darker than bare road, because `MarbleColour` was above the scan's 0.015 mean. Editing
this E4 asset was approved by the user.

Made via: `MaterialTools.get_expressions` and `get_expression_inputs` to confirm the graph still
matched the E4 record, `ObjectTools.get_properties` to read each parameter's name and default before
the change, `ObjectTools.set_properties`, `MaterialTools.recompile`, `get_properties` again to read
back, then `AssetTools.save_assets` with the one path and `AssetTools.is_dirty`.

| Node | Parameter | Before | After |
|---|---|---|---|
| `MaterialExpressionVectorParameter_1` | `MarbleColour` | (0.022, 0.020, 0.018) linear | **(0.0045, 0.0041, 0.0037)** linear |
| `MaterialExpressionScalarParameter_4` | `MarbleDarkening` | 0.8 | **1.0** |
| `MaterialExpressionScalarParameter_7` | `MarbleRoughness` | 0.95 | 0.95, unchanged |

Read back after the recompile as (0.0045, 0.0041, 0.0037, 1), 1 and 0.95. The wiring this acts
through was read back too and is unchanged from E4: `Lerp_2(A Lerp_1, B MarbleColour.RGB,
Alpha Multiply_3)`, with `Multiply_3 = MF_SampleRacetrackSurface.Marbles x MarbleDarkening`.

**The values.** `MarbleColour` keeps E4's hue ratio, 1 : 0.91 : 0.82, at about a third of the scan's
mean, so full marbles are clearly darker than bare asphalt and darker than laid rubber's 0.010.
`MarbleDarkening` at 1.0 lets full coverage reach `MarbleColour`. Nothing above 1 is possible here
without a clamp, because the lerp's alpha is `Marbles x MarbleDarkening` and an alpha past 1
extrapolates beyond `MarbleColour`. `MarbleRoughness` stays: the marble band is already matte, and
the change it would make is not the one that is missing.

The circuit does not override these on its dynamic instance. It sets only `AccumulationTarget`,
`AsphaltTilesAlongLap` and `AsphaltTilesAcross` (`RacetrackCircuit.cpp:2083-2087`), so the
material's defaults are what the road shows.

**The editor log** after the recompile and save has no material or shader error. Its one error line
is `LogModelContextProtocol: Call to unknown method "server/discover"`, which is the MCP server and
unrelated.

### What was verified, and what was not

Verified: the values read back; the material recompiles; it is saved and not dirty; the road shows
the change in a running `-game` demo at 32x and at 1x deposition, measured in `MEASUREMENTS.md` under
*H1 step 1*.

Not verified: whether it reads as marbles, which is the user's judgement; the cooked shader.

---

## 2026-10-08 — H1 step 2: marbles as pellets on top of the tarmac, `MF_MarbleLayer`

**Why.** The user's verdict on step 1: *"this doesn't look like marbles at all. The tarmac itself
should not get darker; there should just be rubber pellets on top of the road."* The coverage wash
is gone. The marble channel now decides where pellets are, how many and how big. The tarmac under
and between them is untouched. The decision is recorded on
[#73](https://github.com/Balazs200001/RubberAccumDemo/issues/73).

**Asset created:** `/Game/Materials/MF_MarbleLayer`, commit `e2e9c52`.
**Asset changed:** `/Game/Materials/M_SurfaceAsphalt`, commit `d823698`. Two nodes added, three
connections changed, one parameter default changed. Nothing deleted.
Both are saved and verified not dirty, and `git status` showed only these two files.

Made via: `MaterialTools.create_function`, `add_expression`, `connect_expressions`,
`connect_to_output`, `disconnect_expressions`, `recompile`, `get_expression_inputs`,
`get_expression_input_names` and `get_expression_output_names`; `ObjectTools.set_properties` and
`get_properties`; `AssetTools.save_assets` with the paths given and `is_dirty`;
`LogsToolset.GetLogEntries` after each recompile.

### `MF_MarbleLayer`

| Node | Class | Settings |
|---|---|---|
| `MaterialExpressionFunctionInput_0` | `FunctionInput` | `Marbles`, `FunctionInput_Scalar`, sort priority 0 |
| `MaterialExpressionWorldPosition_0` | `WorldPosition` | `WPT_ExcludeAllShaderOffsets`, as in `MF_SampleRacetrackSurface`, so a later POM or WPO cannot move the pellets |
| `MaterialExpressionVertexNormalWS_0` | `VertexNormalWS` | |
| `MaterialExpressionScalarParameter_0` | `ScalarParameter` | `MarblePelletCellCentimetres`, default **4.0**, group `Marble Pellets` |
| `MaterialExpressionScalarParameter_1` | `ScalarParameter` | `MarblePelletMaxRadius`, default **0.45** of a cell, group `Marble Pellets` |
| `MaterialExpressionScalarParameter_2` | `ScalarParameter` | `MarblePresenceGain`, default **2.0**, group `Marble Pellets` |
| `MaterialExpressionScalarParameter_3` | `ScalarParameter` | `MarbleMaxPresence`, default **0.6**, group `Marble Pellets` |
| `MaterialExpressionCustom_0` | `Custom` | `MarblePellets`, `CMOT_Float4`, inputs below |
| `MaterialExpressionComponentMask_0` | `ComponentMask` | R, G, B |
| `MaterialExpressionComponentMask_1` | `ComponentMask` | A |
| `MaterialExpressionTransform_0` | `Transform` | `TRANSFORMSOURCE_World` to `TRANSFORM_Tangent` |
| `MaterialExpressionFunctionOutput_1` | `FunctionOutput` | `PelletMask`, sort priority 0 |
| `MaterialExpressionFunctionOutput_0` | `FunctionOutput` | `PelletNormal`, sort priority 1 |

```
WorldPosition.XYZ            --> Custom.Position
VertexNormalWS               --> Custom.SurfaceNormal
FunctionInput Marbles        --> Custom.Coverage
MarblePelletCellCentimetres  --> Custom.CellSize
MarblePelletMaxRadius        --> Custom.MaxRadius
MarblePresenceGain           --> Custom.PresenceGain
MarbleMaxPresence            --> Custom.MaxPresence
Custom --> ComponentMask(RGB) --> Transform(World to Tangent) --> PelletNormal
Custom --> ComponentMask(A)   ----------------------------------> PelletMask
```

### The Custom node's HLSL, recorded verbatim because it exists nowhere else

```hlsl
float2 Scaled = Position.xy / CellSize;
float2 Cell = floor(Scaled);
float2 Local = Scaled - Cell;
float Fill = saturate(Coverage * PresenceGain);
float Presence = Fill * MaxPresence;
float Growth = 0.5 + 0.5 * Fill;
float NearestEdge = 1.0;
float BestHeight = 0.0;
float2 BestOffset = 0.0;
float BestRadius = 1.0;
for (int Row = -1; Row <= 1; ++Row)
{
    for (int Column = -1; Column <= 1; ++Column)
    {
        float2 Neighbour = float2(Column, Row);
        float4 Draw = Rand32ToFloat(Rand4DPCG32(int4(int2(Cell + Neighbour), 0, 0)));
        if (Draw.z >= Presence)
        {
            continue;
        }
        float Radius = MaxRadius * Growth * (0.6 + 0.4 * Draw.w);
        float2 Offset = Local - (Neighbour + Draw.xy);
        float Distance = length(Offset);
        NearestEdge = min(NearestEdge, Distance - Radius);
        float Height = Radius * Radius - Distance * Distance;
        if (Height > BestHeight)
        {
            BestHeight = Height;
            BestOffset = Offset;
            BestRadius = Radius;
        }
    }
}
float EdgeWidth = max(abs(DDX(NearestEdge)) + abs(DDY(NearestEdge)), 1e-4);
float Mask = 1.0 - smoothstep(-EdgeWidth, EdgeWidth, NearestEdge);
float2 Tilt = BestOffset / BestRadius;
float3 TiltWorld = float3(Tilt, 0.0);
TiltWorld -= dot(TiltWorld, SurfaceNormal) * SurfaceNormal;
float Rise = sqrt(saturate(1.0 - dot(Tilt, Tilt)));
float3 PelletNormal = normalize(TiltWorld + SurfaceNormal * Rise);
return float4(PelletNormal, Mask);
```

Inputs in order: `Position`, `SurfaceNormal`, `Coverage`, `CellSize`, `MaxRadius`, `PresenceGain`,
`MaxPresence`. Output `CMOT_Float4`: xyz the pellet's world-space normal, w the mask.

**What it does, line by line in prose, since the source carries no comments:**
- **Cells.** World XY is cut into square cells of `CellSize` cm. Each cell owns at most one pellet.
- **Per-cell random draw.** `Rand4DPCG32`, keyed on the cell's integer coordinates, gives four
  uniforms: `xy` is the pellet's centre anywhere in its cell, `z` decides whether the pellet exists,
  `w` varies its size between 60 % and 100 %.
- **Fill.** This is `saturate(Coverage x PresenceGain)`.
- **Presence.** This is `Fill x MaxPresence`, so the share of cells carrying a pellet scales with
  fill and reaches `MaxPresence` at full fill. It is a scale, not a clamp. With the gain at 2, fill
  itself saturates at 0.5 coverage, so above 0.5 coverage the density no longer rises.
- **Growth.** This is `0.5 + 0.5 x Fill`, from the *uncapped* fill. The cap reduces how many pellets
  there are, not how big they grow.
- **Search.** All 3x3 neighbouring cells are searched. A pellet's radius is under one cell and its
  centre lies inside its own cell, so no pellet outside that neighbourhood can reach the pixel.
  Pellets may overlap; the highest dome wins, so overlaps read as a clump rather than a cut.
- **Edge.** `NearestEdge` is the signed distance to the nearest pellet edge. The mask is a
  one-pixel smoothstep around it, sized by the engine's `DDX` and `DDY`, which return 0 in shaders
  without hardware derivatives (`Common.ush:828`).
- **Normal.** Each pellet is a hemisphere resting on the surface. Its normal tilts in the surface
  plane by `offset / radius` and rises by `sqrt(1 - tilt^2)`.

**Engine facts this rests on, each read in the 5.8 source:**
- `Rand4DPCG32(int4)` and `Rand32ToFloat(uint4)` are in `Engine/Shaders/Private/RandomPCG.ush`
  lines 80 and 137. `RandomPCG.ush` is included by `Random.ush:11`, which `MaterialTemplate.ush:26`
  includes, so a Custom node can call them.
- An LWC input to a Custom node arrives as a demoted `float3` of the input's name, plus a
  `FWSVector3` named with `LWC` appended (`HLSLMaterialTranslator.cpp`, `CustomExpression`, the
  `MCT_LWCVector3` case). The demoted value is about 0.004 cm precise at this circuit's 500 m
  extent.
- The road's tangent basis is real: `GenerateRoadMesh` gives every vertex a tangent along the lap
  and its normal (`RacetrackCircuit.cpp:2182`), so World to Tangent is valid.

**Why not the engine's `VoronoiNoise3D_ALU`** (`Random.ush:517`):
- It returns only the single nearest seed, by 3D distance.
- At qualities 1 and 2, its seeds sit exactly 0.2588 from each lattice corner (`Random.ush:482`).
  Sliced across a flat road, that is a nearly regular square grid of pellets.
- Pellets need a union of irregular discs, each with its own attributes, which it does not compute.

**Why world XY rather than the mesh UV.** The same reason as G5. The ribbon UV is anisotropic and
folded, while world XY gives round pellets and is continuous across the folds. On Spa's slopes a
pellet stretches by 1/cos of the grade, a few percent.

### `M_SurfaceAsphalt` after the change

| Node | Class | Settings |
|---|---|---|
| `MaterialExpressionMaterialFunctionCall_1` | `MaterialFunctionCall` | `/Game/Materials/MF_MarbleLayer` |
| `MaterialExpressionLinearInterpolate_7` | `LinearInterpolate` | |

```
MF_SampleRacetrackSurface.Marbles --> MF_MarbleLayer.Marbles                           (new)
MF_MarbleLayer.PelletMask --> Multiply_3.A        (rewired; was MF_SampleRacetrackSurface.Marbles)
MF_MarbleLayer.PelletMask --> Lerp_5.Alpha        (rewired; was MF_SampleRacetrackSurface.Marbles)
Lerp_6 --> Lerp_7.A,  MF_MarbleLayer.PelletNormal --> Lerp_7.B,  PelletMask --> Lerp_7.Alpha (new)
Lerp_7 --> MP_Normal                              (rewired; was Lerp_6)
```

So:
- `Lerp_2` paints `MarbleColour` inside a pellet, through `Multiply_3 = PelletMask x MarbleDarkening`.
  `MarbleDarkening` is now the pellet's opacity, left at 1.0.
- `Lerp_5` paints `MarbleRoughness` (0.95, unchanged) inside a pellet.
- `Lerp_7` puts the pellet's dome normal over the asphalt's normal.
- Outside pellets, every output is exactly what E4 built.

| Parameter | Before | After | Why |
|---|---|---|---|
| `MarbleColour` | (0.0045, 0.0041, 0.0037), from step 1 | **(0.010, 0.010, 0.011)** | The value of `RubberColour`: marbles are the same rubber that is laid on the line |

### A tool defect found while reading the wiring back

`MaterialTools.get_expression_inputs` **reports the wrong output pin when two inputs of one node
are fed from different outputs of the same source node.** With `Lerp_7.B` on `PelletNormal` and
`Lerp_7.Alpha` on `PelletMask`, both read back as `PelletNormal`, through three reconnects. With
`B` disconnected, `Alpha` read back as `PelletMask`. With `Alpha` disconnected, `B` read back as
`PelletNormal`. So each connection was verified on its own, and the final `Alpha` connection was
made last, as a single call on the input verified a moment before. The final state with both inputs
connected **cannot be read back correctly** through this tool. Any future read-back of a node with
two inputs from one multi-output source has to be done one input at a time.

### What was verified, and what was not

Verified: both assets compile, with no shader or material error in the editor log after each
recompile; every connection was read back, with the defect above handled; both are saved and not
dirty; pellets render in a running `-game` demo, at 32x as dense dark pellets in the marble band
with the tarmac between them unchanged. The numbers are in `MEASUREMENTS.md` under *H1 step 2*.

**Not verified, and open:** at the demo's 1x deposition, the pellets are sparser and smaller than
the marble coverage the debug view shows in the same pixels should give. The cause is not found.
See `MEASUREMENTS.md`. Also not verified: whether it reads as marbles, which is the user's
judgement; the frame cost of the 3x3 search; shimmer at a distance, where a pellet is under a
pixel; the cooked shader.

---

## 2026-10-08 — H1 step 2 amended: presence gain 1.0, pellets 22 % bigger

**Asset changed:** `/Game/Materials/MF_MarbleLayer`. Two parameter defaults only. No nodes or
connections changed. Recompiled with no material error in the log, saved, verified not dirty, and
`M_SurfaceAsphalt` not dirty either. `git status` showed only this `.uasset`. Commit `59f4150`.

Made via: `ObjectTools.get_properties` and `set_properties`, `MaterialTools.recompile`,
`AssetTools.save_assets` with the one path, `AssetTools.is_dirty` on both materials, and
`LogsToolset.GetLogEntries`.

| Node | Parameter | Before | After | Asked for |
|---|---|---|---|---|
| `MaterialExpressionScalarParameter_2` | `MarblePresenceGain` | 2.0 | **1.0** | *"Make the gain 1.0"*. Fill now rises linearly with coverage up to 1.0 instead of saturating at 0.5, so the band's density keeps following its coverage |
| `MaterialExpressionScalarParameter_1` | `MarblePelletMaxRadius` | 0.45 | **0.55** of a cell | *"the pellets could be a little bigger so they are easier to see"* |

**Bigger by radius, not by cell size, and why.** A larger `MarblePelletCellCentimetres` would also
make pellets bigger, but it spaces them further apart: at 5 cm instead of 4 there would be 36 %
fewer per square metre. A larger radius keeps the count and grows each pellet. At 0.55 of a 4 cm
cell, a full-size pellet is 4.4 cm across, against 3.6 before. The 3x3 search stays exact for any
radius under one cell, because a pellet's centre lies in its own cell, so 0.55 is safe. More
neighbours overlap, which reads as clumping.

**Edits were refused while PIE ran.** The first `AssetTools.is_dirty` calls failed with *"Asset
does not exist"*. The editor log said why: `LogUtils: Error: The Editor is currently in a play
mode`. The user had started PIE. Nothing was changed until the user stopped it. Asset tools over
MCP need PIE stopped, and they report it as a missing asset rather than as play mode.

---

## 2026-10-08 — H2: the pellets fade to their prefiltered tone by pixel footprint

**Why.** The pellets shimmered where a 4 cm cell is smaller than about 2 pixels: 5.6 to 14.6 times
bare tarmac under a quarter-pixel pan from 15 m out (`MEASUREMENTS.md`, *H2: the marble pellets
shimmer with distance*). The user chose option B and kept anti-aliasing off. The decision and the
rejected options are on [#74](https://github.com/Balazs200001/RubberAccumDemo/issues/74).

**Asset changed:** `/Game/Materials/MF_MarbleLayer`. Two nodes added, two Custom inputs added,
two connections made, the Custom node's code replaced. Nothing deleted.

**Also saved:** `/Game/Materials/M_SurfaceAsphalt`. Its graph is unchanged. It was recompiled
explicitly to check the new HLSL compiles inside it, which marked it dirty, and it was saved so no
editor state is left unsaved. `git status` showed it modified on disk afterwards. That is the
material's cached data refreshed from the changed function. No node, connection or parameter
default of it was touched.

Made via: `MaterialTools.get_expressions`, `add_expression`, `connect_expressions`, `recompile`
(the function, then the material), `get_expression_inputs`; `ObjectTools.get_properties` and
`set_properties`; `AssetTools.is_dirty` and `save_assets` with the two paths;
`EditorAppToolset.IsPIERunning`, false before the edit; `LogsToolset.GetLogEntries`, no material or
shader error after either recompile.

### Nodes added to `MF_MarbleLayer`

| Node | Class | Settings |
|---|---|---|
| `MaterialExpressionScalarParameter_4` | `ScalarParameter` | `MarblePelletFullDetailCellPixels`, default **4.0**, group `Marble Pellets` |
| `MaterialExpressionScalarParameter_5` | `ScalarParameter` | `MarblePelletToneOnlyCellPixels`, default **2.0**, group `Marble Pellets` |

```
MarblePelletFullDetailCellPixels --> Custom.FullDetailCellPixels   (new input, 8th)
MarblePelletToneOnlyCellPixels   --> Custom.ToneOnlyCellPixels     (new input, 9th)
```

The seven existing inputs were passed back exactly as read and the two appended, then each of the
nine read back with `get_expression_inputs`. Every input comes from a different node, so the
read-back defect recorded under H1 step 2 does not apply.

| Parameter | Default | What it does |
|---|---|---|
| `MarblePelletFullDetailCellPixels` | 4 | A cell at least this many pixels across, along its most compressed screen axis, draws the pellets exactly as before |
| `MarblePelletToneOnlyCellPixels` | 2 | A cell this many pixels across or fewer draws only the pellets' average tone, and skips the 3x3 search |

The defaults come from the measurement. At 4.8 px a cell the band flickered 1.8 times bare
tarmac, which is the close range the user judged convincing in H1. At 1.8 px it flickered 5.6
times.

### The Custom node's HLSL, recorded verbatim because it exists nowhere else

```hlsl
const float SmallestSizeShare = 0.6;
const float SizeShareSpread = 0.4;
float2 Scaled = Position.xy / CellSize;
float2 Cell = floor(Scaled);
float2 Local = Scaled - Cell;
float Fill = saturate(Coverage * PresenceGain);
float Presence = Fill * MaxPresence;
float Growth = 0.5 + 0.5 * Fill;
float LargestRadius = MaxRadius * Growth;
float MeanSquaredSizeShare = SmallestSizeShare * SmallestSizeShare + SmallestSizeShare * SizeShareSpread + SizeShareSpread * SizeShareSpread / 3.0;
float Tone = saturate(Presence * PI * LargestRadius * LargestRadius * MeanSquaredSizeShare);
float CellsPerPixel = max(length(DDX(Scaled)), length(DDY(Scaled)));
float CellPixels = 1.0 / max(CellsPerPixel, 1e-6);
float Resolved = smoothstep(ToneOnlyCellPixels, FullDetailCellPixels, CellPixels);
float NearestEdge = 1.0;
float BestHeight = 0.0;
float2 BestOffset = 0.0;
float BestRadius = 1.0;
[branch]
if (Resolved > 0.0)
{
    for (int Row = -1; Row <= 1; ++Row)
    {
        for (int Column = -1; Column <= 1; ++Column)
        {
            float2 Neighbour = float2(Column, Row);
            float4 Draw = Rand32ToFloat(Rand4DPCG32(int4(int2(Cell + Neighbour), 0, 0)));
            if (Draw.z >= Presence)
            {
                continue;
            }
            float Radius = LargestRadius * (SmallestSizeShare + SizeShareSpread * Draw.w);
            float2 Offset = Local - (Neighbour + Draw.xy);
            float Distance = length(Offset);
            NearestEdge = min(NearestEdge, Distance - Radius);
            float Height = Radius * Radius - Distance * Distance;
            if (Height > BestHeight)
            {
                BestHeight = Height;
                BestOffset = Offset;
                BestRadius = Radius;
            }
        }
    }
}
float EdgeWidth = max(abs(DDX(NearestEdge)) + abs(DDY(NearestEdge)), 1e-4);
float Mask = 1.0 - smoothstep(-EdgeWidth, EdgeWidth, NearestEdge);
float2 Tilt = BestOffset / BestRadius;
float3 TiltWorld = float3(Tilt, 0.0);
TiltWorld -= dot(TiltWorld, SurfaceNormal) * SurfaceNormal;
float Rise = sqrt(saturate(1.0 - dot(Tilt, Tilt)));
float3 PelletNormal = normalize(TiltWorld + SurfaceNormal * Rise);
float3 BlendedNormal = normalize(lerp(SurfaceNormal, PelletNormal, Resolved));
return float4(BlendedNormal, lerp(Tone, Mask, Resolved));
```

Inputs in order: `Position`, `SurfaceNormal`, `Coverage`, `CellSize`, `MaxRadius`, `PresenceGain`,
`MaxPresence`, `FullDetailCellPixels`, `ToneOnlyCellPixels`. Output unchanged, `CMOT_Float4`: xyz
the world-space normal, w the mask.

**What changed from H1's version, in prose, since the source carries no comments:**
- **The size spread is named.** The pellet's radius was `MaxRadius x Growth x (0.6 + 0.4 x Draw.w)`.
  It is the same expression with `0.6` and `0.4` as `SmallestSizeShare` and `SizeShareSpread`, so
  the tone can use the same two numbers.
- **The footprint.** `Scaled` is the position in cells. Its screen derivatives give how many cells
  one pixel steps across, along each screen axis. The larger of the two lengths is the axis along
  which a cell is most compressed, and `CellPixels` is its inverse: how many pixels one cell spans
  along that axis. This is the quantity texture mip selection uses. It accounts for grazing
  angles, field of view and the render resolution without retuning.
- **Resolved.** This is 1 where a cell spans `FullDetailCellPixels` or more, 0 where it spans
  `ToneOnlyCellPixels` or fewer, and a smoothstep between.
- **The tone.** This is the share of road the pellets cover on average:
  `Presence x pi x (MaxRadius x Growth)^2 x E[(0.6 + 0.4u)^2]`.
  - The expectation is `0.6^2 + 0.6 x 0.4 + 0.4^2 / 3 = 0.6533`.
  - A NumPy model of this HLSL, sampled at 24 x 24 points per cell over 192 x 192 cells, measured
    the true covered share. The formula overstates it by at most 4 % up to 0.7 coverage, and by 9 %
    at full coverage, because it ignores overlaps.
  - The colour, roughness and normal lerps in `M_SurfaceAsphalt` all take this mask as their
    alpha. So where only the tone remains, they give the average of what the pellets would have
    drawn, as a mipmap does for a texture.
- **The search is skipped** with `[branch]` where `Resolved` is 0. The loop holds no derivative
  instruction. `DDX(NearestEdge)` is taken after the branch, where every lane is active again.
- **The output blends.** The mask is `lerp(Tone, Mask, Resolved)`, and the normal blends from the
  surface normal to the dome normal by the same `Resolved`. Up close, `Resolved` is 1 and the output
  is exactly H1's.
- **Where derivatives do not exist.** `DDX` and `DDY` return 0 (`Common.ush:828`), so
  `CellsPerPixel` is 0, `CellPixels` is a million and `Resolved` is 1, which is H1's behaviour.

**Engine facts this rests on, each read in the 5.8 source:**
- `PI` is `const static MaterialFloat PI` at `Common.ush:133`, in scope for Custom nodes.
- The `float2` overloads of `DDX` and `DDY` are at `Common.ush:828` to `898`.
- `AntialiasedTextureMask`, the engine's own derivative-based mask edge, carries the note "ddx/ddy
  only for every 2x2 block (bad for distant stuff)" at `Common.ush:1756`. That is the 2x2 speckle
  this fade replaces with the tone at distance.

**Not done:** C's per-pixel edge width (`ddx_fine`) inside the transition, which was not chosen.
The edge is still sized by `DDX(NearestEdge)`.

### What was verified, and what was not

Verified:
- `MF_MarbleLayer` and `M_SurfaceAsphalt` recompiled, with no material or shader error in the
  editor log. The material recompile raises if its shader fails to compile, and it did not.
- Every one of the nine Custom inputs was read back.
- Both assets are saved and not dirty. `git status` showed only these two files.
- In a running `-game` demo, the pellets render up close as before and fade with distance. Under
  the H2 pan the band's flicker is now 0.82 to 0.91 times bare tarmac at 15, 30 and 60 m, against
  5.6 to 14.6 times before. The numbers are in `MEASUREMENTS.md`, *H2 exit check*.

Not verified:
- Whether the faded band reads right, which is the user's call. It reads close to bare tarmac,
  probably because pellets look dark mostly through shading, not albedo.
- The pellet layer's frame cost, before or after.
- The cooked shader.

---

## 2026-10-12 — H2 amended: pellets drawn out to 1 px a cell with area-kept analytic edges, a tone gain

**Why.** The user drove the 4-to-2 fade and judged it: *"I only see marbles in the [nearest few
metres] but I want to see it all in the [band out past the car] too."* At the defaults above, the
chase camera showed pellets only within about 3 m of the camera. The decision is on
[#74](https://github.com/Balazs200001/RubberAccumDemo/issues/74).

To draw pellets further out without bringing H1's shimmer back, each pellet's edge now comes from
the pixel's footprint analytically, and keeps the pellet's area. That is the part of option C that
was not chosen on 2026-10-08.

**Asset changed:** `/Game/Materials/MF_MarbleLayer`. One node added, one Custom input added, one
connection made, two parameter defaults changed, the Custom node's code replaced. Nothing deleted.
**Also saved:** `/Game/Materials/M_SurfaceAsphalt`, for the same reason as in the entry above: it
was recompiled explicitly and re-saved. Its graph is untouched. Host commit `6e9c9e8`.

Made via the same tools as the entry above. `IsPIERunning` returned false, and both assets were not
dirty and matched their committed state before the edit. There was no material or shader error in
the log after either recompile. All ten Custom inputs were read back.

| Node | Parameter | Before | After |
|---|---|---|---|
| `MaterialExpressionScalarParameter_4` | `MarblePelletFullDetailCellPixels` | 4 | **1** |
| `MaterialExpressionScalarParameter_5` | `MarblePelletToneOnlyCellPixels` | 2 | **0.5** |
| `MaterialExpressionScalarParameter_6`, new | `MarblePelletToneGain`, group `Marble Pellets` | | **1** |

```
MarblePelletToneGain --> Custom.ToneGain   (new input, 10th)
```

### The Custom node's HLSL, recorded verbatim because it exists nowhere else

```hlsl
const float SmallestSizeShare = 0.6;
const float SizeShareSpread = 0.4;
const float WideEdgeAreaExcess = 0.2;
const float NarrowEdgeAreaConstant = 0.1;
const float NarrowEdgeAreaLinear = 0.3;
const float NarrowEdgeAreaQuadratic = 0.8;
float2 Scaled = Position.xy / CellSize;
float2 Cell = floor(Scaled);
float2 Local = Scaled - Cell;
float2 FootprintX = DDX(Scaled);
float2 FootprintY = DDY(Scaled);
float Fill = saturate(Coverage * PresenceGain);
float Presence = Fill * MaxPresence;
float Growth = 0.5 + 0.5 * Fill;
float LargestRadius = MaxRadius * Growth;
float MeanSquaredSizeShare = SmallestSizeShare * SmallestSizeShare + SmallestSizeShare * SizeShareSpread + SizeShareSpread * SizeShareSpread / 3.0;
float Tone = saturate(Presence * PI * LargestRadius * LargestRadius * MeanSquaredSizeShare * ToneGain);
float CellsPerPixel = max(length(FootprintX), length(FootprintY));
float CellPixels = 1.0 / max(CellsPerPixel, 1e-6);
float Resolved = smoothstep(ToneOnlyCellPixels, FullDetailCellPixels, CellPixels);
float Uncovered = 1.0;
float BestHeight = 0.0;
float2 BestOffset = 0.0;
float BestRadius = 1.0;
[branch]
if (Resolved > 0.0)
{
    for (int Row = -1; Row <= 1; ++Row)
    {
        for (int Column = -1; Column <= 1; ++Column)
        {
            float2 Neighbour = float2(Column, Row);
            float4 Draw = Rand32ToFloat(Rand4DPCG32(int4(int2(Cell + Neighbour), 0, 0)));
            if (Draw.z >= Presence)
            {
                continue;
            }
            float Radius = LargestRadius * (SmallestSizeShare + SizeShareSpread * Draw.w);
            float2 Offset = Local - (Neighbour + Draw.xy);
            float Distance = max(length(Offset), 1e-6);
            float2 Direction = Offset / Distance;
            float EdgeWidth = max(abs(dot(Direction, FootprintX)) + abs(dot(Direction, FootprintY)), 1e-4);
            float SizeOverEdge = Radius / EdgeWidth;
            float FilteredArea = SizeOverEdge >= 1.0
                ? SizeOverEdge * SizeOverEdge + WideEdgeAreaExcess
                : NarrowEdgeAreaConstant + SizeOverEdge * (NarrowEdgeAreaLinear + NarrowEdgeAreaQuadratic * SizeOverEdge);
            float PelletCoverage = (1.0 - smoothstep(-EdgeWidth, EdgeWidth, Distance - Radius)) * SizeOverEdge * SizeOverEdge / FilteredArea;
            Uncovered *= 1.0 - PelletCoverage;
            float Height = Radius * Radius - Distance * Distance;
            if (Height > BestHeight)
            {
                BestHeight = Height;
                BestOffset = Offset;
                BestRadius = Radius;
            }
        }
    }
}
float Mask = 1.0 - Uncovered;
float2 Tilt = BestOffset / BestRadius;
float3 TiltWorld = float3(Tilt, 0.0);
TiltWorld -= dot(TiltWorld, SurfaceNormal) * SurfaceNormal;
float Rise = sqrt(saturate(1.0 - dot(Tilt, Tilt)));
float3 PelletNormal = normalize(TiltWorld + SurfaceNormal * Rise);
float3 BlendedNormal = normalize(lerp(SurfaceNormal, PelletNormal, Resolved));
return float4(BlendedNormal, lerp(Tone, Mask, Resolved));
```

Inputs in order: `Position`, `SurfaceNormal`, `Coverage`, `CellSize`, `MaxRadius`, `PresenceGain`,
`MaxPresence`, `FullDetailCellPixels`, `ToneOnlyCellPixels`, `ToneGain`.

**What changed from the 2026-10-08 version, in prose:**

- **The edge is analytic, per pellet.**
  - The screen derivatives of the cell coordinate, `FootprintX` and `FootprintY`, are taken once
    before the loop.
  - For each pellet, the edge width is the pixel's footprint along the pellet's radial direction:
    `abs(dot(Direction, FootprintX)) + abs(dot(Direction, FootprintY))`. That is exactly what
    `DDX(Distance)` and `DDY(Distance)` would be for that one disc, but per pixel rather than per
    2x2 quad, and without jumping between pellets.
  - Up close it gives the same edge as before. `DDX(NearestEdge)` is gone, and so are the 2x2
    blocks.
- **Each pellet keeps its area.**
  - A disc of radius `r` with the edge `1 - smoothstep(-W, W, rho - r)` covers `pi (r^2 + W^2/5)`
    when `r >= W`. This is exact: `W^2/5` is the variance of a smoothstep's derivative over `-W`
    to `W`.
  - Below that, the profile is cut at the centre, and `pi W^2 (0.1 + 0.3 t + 0.8 t^2)`, with
    `t = r / W`, fits the numerically integrated area within 3.5 % (`edge_model.py`).
  - Each pellet's coverage is scaled by `pi r^2` over that area. So as a pellet shrinks below its
    filter it becomes a fainter, wider dot of the same area, instead of the darker halo that made
    H1's far band 1.3 to 1.5 times too dark.
- **Pellets combine as independent coverages.** The mask is `1 - product of (1 - PelletCoverage)`
  over the nine neighbours, not the nearest edge. With the nearest edge, overlapping filtered tails
  were lost: modelled at 0.6 coverage, the band fell to 0.66 of its true share at 1 px a cell. With
  the product it stays at 0.93 to 1.00 down to 1 px, and falls to 0.86 to 0.94 by 0.75 px, where
  tails start to reach past the 3x3 search (`edge_union.py`). Hence the tone takes over below
  1 px.
- **The thresholds** are now 1 and 0.5 px a cell, on the most compressed axis as before.
- **`ToneGain`** scales the tone's alpha. It is a lever for making the far band read darker than its
  true average. At 1 it matches the filtered pellets: in the 1 to 2 px band, run 4 drew tone at gain
  1 and run 5 drew resolved pellets on the same frozen field, and they differ by 0.3 levels
  (`MEASUREMENTS.md`, *H2 amended*).
- **Not changed:** the dome normal is still point-sampled at the pixel centre and weighted by the
  mask in `M_SurfaceAsphalt`.

### What was verified, and what was not

Verified:
- Both assets compile, are saved and are not dirty, and `git status` showed only these two.
- In a running `-game` demo under the H2 pan, band flicker is 1.62x bare tarmac at 15 m, 1.63x at
  30 m and 0.83x at 60 m. That is within #74's 2x exit, though above the 4-to-2 fade's 0.8 to 0.9x,
  because pellets are now drawn further out.
- The chase view shows pellets well beyond where the car sits.

Not verified:
- whether it now shows what the user wants, which is the user's call;
- the frame cost, now that the 3x3 search runs on more pixels and does more work per neighbour;
- the cooked shader.

---

## 2026-10-12 — H2: the tone gain raised to 4, so the unresolved band reads as a darker patch

**Why.** The user drove the analytic-edge version and judged it: *"I still want to see it be more
visible from further. When I am in the car view I expect to see all the marbles, or at least a
darker road patch. The [near band, with resolved pellets] is good and looks fine."* The decision is
on [#74](https://github.com/Balazs200001/RubberAccumDemo/issues/74).

**Asset changed:** `/Game/Materials/MF_MarbleLayer`. One parameter default. No nodes or connections
changed. Host commit `8372c53`. `M_SurfaceAsphalt` was not recompiled and is not dirty.

| Node | Parameter | Before | After |
|---|---|---|---|
| `MaterialExpressionScalarParameter_6` | `MarblePelletToneGain` | 1 | **4** |

**A temporary change, saved and reverted, never committed.** `MarbleMaxPresence`
(`MaterialExpressionScalarParameter_3`) was set to 0 and saved, for one `-game` run that gives the
road without marbles on the same pixels. It was then set back to 0.6 and read back as 0.6 before
the gain was changed. Committed history holds 0.6 throughout.

The first attempt to save the 0 failed, and nothing reached disk. The user's own `-game` window held
`MF_MarbleLayer.uasset` open: `MoveFile` error 32, a sharing violation, retried ten times, then
*"failed to save"*. **A running `-game` process locks the packages it loaded, so MCP saves fail
until it exits.** The value was put back to 0.6 in memory at once, and the reference run waited
until the user closed the game.

**Why 4.** This is a deliberate exaggeration, chosen because the user asked for a visible patch.
It is not a physical match. `MEASUREMENTS.md`, *H2: the tone gain*, has the numbers:
- Measured against the marble-free road, resolved pellets darken the band by only 3.5 to 5 %.
- At gain 1, the tone darkened it by 2.5 to 3.4 %.
- A gain matching the resolved pellets per unit coverage would be about 1.5 to 1.8. That is still
  under 5 %, and does not read at a distance.
- Each unit of tone alpha darkens the road by about 18 %, which is also the ceiling with the current
  pellet colour and roughness. At 4, the band core reaches about 0.6 alpha and 10 to 14 %.
- The gain multiplies only the tone: the output is `lerp(Tone, Mask, Resolved)`, with `ToneGain`
  inside `Tone`. So where pellets are resolved, a cell spanning 1 px or more, nothing changes, and
  the tarmac between near pellets stays untouched.

---

## 2026-10-12 — H2: a smooth footprint against the road's facets, pellets to 0.5 px a cell, the gain ramped in

**Why.** The user drove the gain-4 version with the new chase camera and judged it: *"There is a
pretty obvious problem when there is elevation change with your marble implementation at this
spot, but I also think the [band beyond the resolved pellets] again should be marbles. Let me see
if the flickering is even noticeable, and if it should be something I should worry about."* Their
screenshot showed horizontal stripes across the marble band over a crest. The decision is on
[#74](https://github.com/Balazs200001/RubberAccumDemo/issues/74).

**The cause of the stripes, from the code.** It is not separately measured.
- The road mesh is flat 128 cm segments (`RoadSegmentLength`), so over a crest each segment tilts
  slightly differently.
- `DDX` and `DDY` of the world position are constant across one flat segment and jump at its edge.
- At grazing angles, a one-degree change of tilt changes the footprint by 10 to 20 %. So
  `Resolved`, and with it the choice between pellets and the much darker gain-4 tone, changed
  segment by segment.

**Asset changed:** `/Game/Materials/MF_MarbleLayer`. One node added, one Custom input added, one
connection made, two parameter defaults changed, the Custom node's code replaced.
**Also saved:** `/Game/Materials/M_SurfaceAsphalt`, recompiled and re-saved as before, with its
graph untouched. Host commit `50d92fb`.

Before the edit: no `-game` process was running, `IsPIERunning` was false, and git was clean. After
it: no material or shader error after either recompile, all eleven Custom inputs read back, both
assets saved and not dirty, and no shader error in the `-game` log of the run that followed.

| Node | Parameter | Before | After |
|---|---|---|---|
| `MaterialExpressionScalarParameter_4` | `MarblePelletFullDetailCellPixels` | 1 | **0.5** |
| `MaterialExpressionScalarParameter_5` | `MarblePelletToneOnlyCellPixels` | 0.5 | **0.25** |
| `MaterialExpressionScalarParameter_7`, new | `MarblePelletUnityGainCellPixels`, group `Marble Pellets` | | **2** |

`MarblePelletToneGain` stays at 4. Its meaning changes, as described below.

```
MarblePelletUnityGainCellPixels --> Custom.UnityGainCellPixels   (new input, 11th)
```

### The Custom node's HLSL, recorded verbatim because it exists nowhere else

```hlsl
const float SmallestSizeShare = 0.6;
const float SizeShareSpread = 0.4;
const float WideEdgeAreaExcess = 0.2;
const float NarrowEdgeAreaConstant = 0.1;
const float NarrowEdgeAreaLinear = 0.3;
const float NarrowEdgeAreaQuadratic = 0.8;
const float GrazingFacingLimit = 0.01;
float2 Scaled = Position.xy / CellSize;
float2 Cell = floor(Scaled);
float2 Local = Scaled - Cell;
float3 ViewRay = normalize(Parameters.WorldPosition_CamRelative);
float3 Up = normalize(SurfaceNormal);
float Facing = max(abs(dot(ViewRay, Up)), GrazingFacingLimit);
float PixelAngle = 2.0 / (ResolvedView.ViewToClip[0][0] * ResolvedView.ViewSizeAndInvSize.x);
float AcrossCellsPerPixel = GetPixelDepth(Parameters) * PixelAngle / CellSize;
float AlongCellsPerPixel = AcrossCellsPerPixel / Facing;
float2 AlongView = ViewRay.xy / max(length(ViewRay.xy), 1e-4);
float2 AcrossView = float2(-AlongView.y, AlongView.x);
float Fill = saturate(Coverage * PresenceGain);
float Presence = Fill * MaxPresence;
float Growth = 0.5 + 0.5 * Fill;
float LargestRadius = MaxRadius * Growth;
float MeanSquaredSizeShare = SmallestSizeShare * SmallestSizeShare + SmallestSizeShare * SizeShareSpread + SizeShareSpread * SizeShareSpread / 3.0;
float Tone = saturate(Presence * PI * LargestRadius * LargestRadius * MeanSquaredSizeShare);
float CellPixels = 1.0 / max(AlongCellsPerPixel, 1e-6);
float Resolved = smoothstep(ToneOnlyCellPixels, FullDetailCellPixels, CellPixels);
float Gain = lerp(ToneGain, 1.0, smoothstep(FullDetailCellPixels, UnityGainCellPixels, CellPixels));
float Uncovered = 1.0;
float BestHeight = 0.0;
float2 BestOffset = 0.0;
float BestRadius = 1.0;
[branch]
if (Resolved > 0.0)
{
    for (int Row = -1; Row <= 1; ++Row)
    {
        for (int Column = -1; Column <= 1; ++Column)
        {
            float2 Neighbour = float2(Column, Row);
            float4 Draw = Rand32ToFloat(Rand4DPCG32(int4(int2(Cell + Neighbour), 0, 0)));
            if (Draw.z >= Presence)
            {
                continue;
            }
            float Radius = LargestRadius * (SmallestSizeShare + SizeShareSpread * Draw.w);
            float2 Offset = Local - (Neighbour + Draw.xy);
            float Distance = max(length(Offset), 1e-6);
            float2 Direction = Offset / Distance;
            float EdgeWidth = max(abs(dot(Direction, AlongView)) * AlongCellsPerPixel + abs(dot(Direction, AcrossView)) * AcrossCellsPerPixel, 1e-4);
            float SizeOverEdge = Radius / EdgeWidth;
            float FilteredArea = SizeOverEdge >= 1.0
                ? SizeOverEdge * SizeOverEdge + WideEdgeAreaExcess
                : NarrowEdgeAreaConstant + SizeOverEdge * (NarrowEdgeAreaLinear + NarrowEdgeAreaQuadratic * SizeOverEdge);
            float PelletCoverage = (1.0 - smoothstep(-EdgeWidth, EdgeWidth, Distance - Radius)) * SizeOverEdge * SizeOverEdge / FilteredArea;
            Uncovered *= 1.0 - PelletCoverage;
            float Height = Radius * Radius - Distance * Distance;
            if (Height > BestHeight)
            {
                BestHeight = Height;
                BestOffset = Offset;
                BestRadius = Radius;
            }
        }
    }
}
float Mask = 1.0 - Uncovered;
float2 Tilt = BestOffset / BestRadius;
float3 TiltWorld = float3(Tilt, 0.0);
TiltWorld -= dot(TiltWorld, SurfaceNormal) * SurfaceNormal;
float Rise = sqrt(saturate(1.0 - dot(Tilt, Tilt)));
float3 PelletNormal = normalize(TiltWorld + SurfaceNormal * Rise);
float3 BlendedNormal = normalize(lerp(SurfaceNormal, PelletNormal, Resolved));
return float4(BlendedNormal, saturate(lerp(Tone, Mask, Resolved) * Gain));
```

Inputs in order: `Position`, `SurfaceNormal`, `Coverage`, `CellSize`, `MaxRadius`, `PresenceGain`,
`MaxPresence`, `FullDetailCellPixels`, `ToneOnlyCellPixels`, `ToneGain`, `UnityGainCellPixels`.

**What changed, in prose:**

- **The footprint no longer uses screen derivatives.**
  - A pixel spans `PixelAngle = 2 / (ViewToClip[0][0] x ViewSizeX)` radians. `ViewToClip[0][0]`
    is the cotangent of half the horizontal field of view, and `ViewSizeX` is the render width, so
    1336 here.
  - At scene depth `GetPixelDepth(Parameters)`, a pixel covers depth times that across the view.
  - Along the view it covers that divided by `Facing`, the cosine between the view ray
    (`Parameters.WorldPosition_CamRelative`) and the **interpolated** vertex normal. The floor of
    0.01 sends grazing angles to the tone.
  - Depth and the interpolated normal are continuous across the road's facets, so the fade, the
    gain and every pellet's edge width are continuous too.
  - On a flat surface near the centre of the screen this equals what the derivatives gave. Away
    from the centre it overstates the footprint slightly, by about `1 / cos` of the angle off the
    view axis.
  - `DDX` and `DDY` are no longer called.
- **Each pellet's edge** is now this footprint along the pellet's radial direction:
  `AlongCellsPerPixel` along the view's direction in XY, `AcrossCellsPerPixel` across it.
- **The darkness gain is ramped, and applied after the blend.**
  - `Gain` is 1 where a cell spans `UnityGainCellPixels` (2) or more. It rises to `ToneGain` (4) at
    `FullDetailCellPixels` (0.5) and below.
  - It multiplies `lerp(Tone, Mask, Resolved)`, so it darkens sub-pixel pellets as well as the
    tone. Resolved pellets at 2 px and more are unchanged.
  - Before this, the gain sat inside the tone only, so the filtered pellets between 1 and 4 px read
    lighter than both the near dots and the far patch.
- **Pellets are drawn down to 0.5 px a cell**, and are pure tone below 0.25. Below about 0.75 px the
  3x3 search starts to miss filtered tails, which the gain more than covers.
- `ToneGain` therefore now means *the darkness gain where pellets are smaller than half a pixel*.
  `PI` and `GetPixelDepth` are as recorded above (`Common.ush:133`, `MaterialTemplate.ush:1120`).
  `ResolvedView.ViewToClip` and `ResolvedView.ViewSizeAndInvSize` are read by `MaterialTemplate.ush`'s
  own helpers, at `:2895` and `:1232`.

### What was verified, and what was not

Verified:
- The materials compile, and the shader compiles in `-game`, with no error logged.
- Under the H2 pan:
  - the band's darkening rises continuously with distance;
  - flicker is 3.1x bare tarmac at 15 m and 4.0x at 30 m, against #74's 2x exit;
  - it is 1.05x at 60 m and 1.8x in the chase view.

The numbers are in `MEASUREMENTS.md`, *H2: the smooth footprint*.

Not verified:
- **that the stripes over the crest are gone.** None of the measured views is on a crest. That is
  the user's to see where they saw it;
- whether the flicker is noticeable while driving, which is the user's call;
- the frame cost;
- the cooked shader.

---

## 2026-10-12 — H2: the pellets decided by camera distance alone

**Why.** The user judged the smooth-footprint version: *"It definitely feels better while driving
now. I like all the distance settings, but for some reason there is a problem still with elevation
changes, where sometimes even when the road I'm looking at is close and right behind the car the
marbles don't show up at all. If you can't pinpoint the bug, maybe it's better to use a simpler
system which just measures distance to the camera of the pixel to decide to draw the marbles or
not."* The decision is on [#74](https://github.com/Balazs200001/RubberAccumDemo/issues/74).

**What was found first** (`MEASUREMENTS.md`, *H2: on the slopes*):
- **Straights get no marbles by design.** `AppendMarbleFan` only throws a fan where the
  curvature exceeds `MinimumMarbleCurvature` (`SurfaceAccumulationSubsystem.cpp:983`). A straight
  climb or descent receives none, however steep, so most of the missing pellets there were correct.
- **Where marbles do exist on a slope, the facing term made them faint.** The chase camera does not
  pitch with the car, so on a descent it sees the road at a grazing angle. The footprint along the
  view was the across footprint divided by `Facing`, so the fade came closer and each near pellet
  was smeared along the view, keeping its area but too faint to read.
- **Ruled out by reading the source:** the paint-versus-sample mapping on slopes (both use the 3D
  arc length), the road's winding and normals (the spline's up vector tilts with the slope), and
  mipmapping of the accumulation target (it has no mips).

**Asset changed:** `/Game/Materials/MF_MarbleLayer`. Three parameters renamed and their defaults
changed, three Custom inputs renamed, the Custom node's code replaced. No node added or deleted.
**Also saved:** `/Game/Materials/M_SurfaceAsphalt`, recompiled and re-saved as before, with its
graph untouched. Host commit `f07843f`.

Before the edit there was no `-game` process, PIE was not running and git was clean. After it:
no error after either recompile, all eleven inputs read back under their new names, both assets
saved, and no shader error in the `-game` log of the run that followed.

| Node | Before | After |
|---|---|---|
| `MaterialExpressionScalarParameter_4` | `MarblePelletFullDetailCellPixels`, 0.5 | **`MarblePelletFullDetailDistance`, 1000 cm** |
| `MaterialExpressionScalarParameter_5` | `MarblePelletToneOnlyCellPixels`, 0.25 | **`MarblePelletToneOnlyDistance`, 1400 cm** |
| `MaterialExpressionScalarParameter_7` | `MarblePelletUnityGainCellPixels`, 2 | **`MarblePelletUnityGainDistance`, 500 cm** |
| `MaterialExpressionScalarParameter_6` | `MarblePelletToneGain`, 4 | unchanged |

Custom inputs renamed to match: `FullDetailDistance`, `ToneOnlyDistance` and `UnityGainDistance`.

**The distances keep the settings the user liked.** On flat road in the chase view, the
pixels-per-cell version put a cell at 2 px at about 4.7 m from the camera, at 0.5 px at about
9.5 m and at 0.25 px at about 13.4 m. That uses `4h / (r^2 x pixel angle)`, with the camera
167.5 cm above the road and a pixel angle of 2 / 1336. These are rounded to 5, 10 and 14 m.

### The Custom node's HLSL, recorded verbatim because it exists nowhere else

```hlsl
const float SmallestSizeShare = 0.6;
const float SizeShareSpread = 0.4;
const float WideEdgeAreaExcess = 0.2;
const float NarrowEdgeAreaConstant = 0.1;
const float NarrowEdgeAreaLinear = 0.3;
const float NarrowEdgeAreaQuadratic = 0.8;
float2 Scaled = Position.xy / CellSize;
float2 Cell = floor(Scaled);
float2 Local = Scaled - Cell;
float CameraDistance = length(Parameters.WorldPosition_CamRelative);
float PixelAngle = 2.0 / (ResolvedView.ViewToClip[0][0] * ResolvedView.ViewSizeAndInvSize.x);
float EdgeWidth = max(CameraDistance * PixelAngle / CellSize, 1e-4);
float Fill = saturate(Coverage * PresenceGain);
float Presence = Fill * MaxPresence;
float Growth = 0.5 + 0.5 * Fill;
float LargestRadius = MaxRadius * Growth;
float MeanSquaredSizeShare = SmallestSizeShare * SmallestSizeShare + SmallestSizeShare * SizeShareSpread + SizeShareSpread * SizeShareSpread / 3.0;
float Tone = saturate(Presence * PI * LargestRadius * LargestRadius * MeanSquaredSizeShare);
float Resolved = 1.0 - smoothstep(FullDetailDistance, ToneOnlyDistance, CameraDistance);
float Gain = lerp(1.0, ToneGain, smoothstep(UnityGainDistance, FullDetailDistance, CameraDistance));
float Uncovered = 1.0;
float BestHeight = 0.0;
float2 BestOffset = 0.0;
float BestRadius = 1.0;
[branch]
if (Resolved > 0.0)
{
    for (int Row = -1; Row <= 1; ++Row)
    {
        for (int Column = -1; Column <= 1; ++Column)
        {
            float2 Neighbour = float2(Column, Row);
            float4 Draw = Rand32ToFloat(Rand4DPCG32(int4(int2(Cell + Neighbour), 0, 0)));
            if (Draw.z >= Presence)
            {
                continue;
            }
            float Radius = LargestRadius * (SmallestSizeShare + SizeShareSpread * Draw.w);
            float2 Offset = Local - (Neighbour + Draw.xy);
            float Distance = length(Offset);
            float SizeOverEdge = Radius / EdgeWidth;
            float FilteredArea = SizeOverEdge >= 1.0
                ? SizeOverEdge * SizeOverEdge + WideEdgeAreaExcess
                : NarrowEdgeAreaConstant + SizeOverEdge * (NarrowEdgeAreaLinear + NarrowEdgeAreaQuadratic * SizeOverEdge);
            float PelletCoverage = (1.0 - smoothstep(-EdgeWidth, EdgeWidth, Distance - Radius)) * SizeOverEdge * SizeOverEdge / FilteredArea;
            Uncovered *= 1.0 - PelletCoverage;
            float Height = Radius * Radius - Distance * Distance;
            if (Height > BestHeight)
            {
                BestHeight = Height;
                BestOffset = Offset;
                BestRadius = Radius;
            }
        }
    }
}
float Mask = 1.0 - Uncovered;
float2 Tilt = BestOffset / BestRadius;
float3 TiltWorld = float3(Tilt, 0.0);
TiltWorld -= dot(TiltWorld, SurfaceNormal) * SurfaceNormal;
float Rise = sqrt(saturate(1.0 - dot(Tilt, Tilt)));
float3 PelletNormal = normalize(TiltWorld + SurfaceNormal * Rise);
float3 BlendedNormal = normalize(lerp(SurfaceNormal, PelletNormal, Resolved));
return float4(BlendedNormal, saturate(lerp(Tone, Mask, Resolved) * Gain));
```

Inputs in order: `Position`, `SurfaceNormal`, `Coverage`, `CellSize`, `MaxRadius`, `PresenceGain`,
`MaxPresence`, `FullDetailDistance`, `ToneOnlyDistance`, `ToneGain`, `UnityGainDistance`.

**What changed, in prose:**
- **One quantity decides everything: the pixel's distance to the camera**,
  `length(Parameters.WorldPosition_CamRelative)`, in cm.
  - Pellets are fully drawn within `FullDetailDistance`, and only the tone remains beyond
    `ToneOnlyDistance`, with a smoothstep between.
  - The darkness gain is 1 within `UnityGainDistance` and rises to `ToneGain` by
    `FullDetailDistance`.
  - The viewing angle and the road normal take no part, so a slope, a crest or the camera's fixed
    pitch cannot move the fade.
- **Each pellet's edge width is the size of one pixel at that distance**: distance times
  `PixelAngle`, in cells, the same in every direction. The area-keeping normalisation is unchanged.
- **What this gives up.** Where the road runs nearly parallel to the view, a cell is compressed
  along the view by `1 / facing`, and the edge no longer widens to match. Pellets there stay visible
  as pellets but can alias along the view. Measured in the chase view, band flicker went from 1.8x
  to 2.1x bare tarmac.
- `SurfaceNormal` is now used only for the dome normal.

### What was verified, and what was not

Verified:
- The materials compile, and so does the shader in `-game`, with no error logged.
- Under the H2 pan, #74's exit is met again: 0.72x bare tarmac at 30 m and 1.14x at 60 m.
- Pellets render on the crest and climb captures wherever the marble channel has coverage.

Not verified:
- the spot the user drove, which is the user's to see;
- the frame cost;
- the cooked shader.

---

## 2026-10-12 — H2: the dark-patch-only distance moved to 20 m

**Why.** The user asked: *"I think you could maybe increase the max distance to camera where the
dark patch only happens to about 20, and decide the appropriate levels for the others yourself."*

**Asset changed:** `/Game/Materials/MF_MarbleLayer`. One parameter default; no nodes, connections or
code changed. `M_SurfaceAsphalt` was not recompiled and is not dirty. Host commit `f79e224`.

| Node | Parameter | Before | After |
|---|---|---|---|
| `MaterialExpressionScalarParameter_5` | `MarblePelletToneOnlyDistance` | 1400 cm | **2000 cm** |

**The other two are deliberately unchanged, and why:**
- **`MarblePelletUnityGainDistance` stays at 500 cm.** That is where a cell still spans about 2 px
  in the chase view, the band the user called good. Moving it out would leave sub-pixel pellets
  drawn at their natural, faint darkness, a lighter ring before the gain takes over.
- **`MarblePelletFullDetailDistance` stays at 1000 cm.** That is where a cell shrinks to about
  0.5 px along the view in the chase view. Beyond it, the 3x3 search loses filtered area and flicker
  climbs.

So the crossfade from pellets to the dark patch now runs from 10 to 20 m instead of 10 to 14 m.

**Verified:** saved, read back as 2000, and no shader error in the `-game` run that measured it.
`MEASUREMENTS.md`, *H2: the dark patch from 20 m*, has the numbers. **Not verified:** how it looks
while driving, which is the user's call; frame cost.

---

## 2026-10-12 — H3: the marble layer on the engine's material quality switch

**Why.** H3 (#75), *"Scalability tiers wired to the standard system"*. The user chose option A: a
`QualitySwitch` in `MF_MarbleLayer`, driven by `sg.EffectsQuality` through `r.MaterialQualityLevel`.
The upscaling is left alone. The decision and the engine facts are on
[#75](https://github.com/Balazs200001/RubberAccumDemo/issues/75).

**Asset changed:** `/Game/Materials/MF_MarbleLayer`. Five nodes added, one Custom input added, eleven
connections made, two connections moved, the pellet node's code changed in two lines.
**Also saved:** `/Game/Materials/M_SurfaceAsphalt`, recompiled and re-saved as before, with its
graph untouched. Host commit `f791db2`.

Before the edit there was no `-game` process, PIE was not running and the host was clean.

### Nodes added

| Node | Class | Settings |
|---|---|---|
| `MaterialExpressionQualitySwitch_0` | `QualitySwitch` | Chooses the layer's output |
| `MaterialExpressionCustom_1` | `Custom` | `MarbleTone`, `CMOT_Float4`, HLSL below |
| `MaterialExpressionQualitySwitch_1` | `QualitySwitch` | Chooses the distance scale |
| `MaterialExpressionScalarParameter_8` | `ScalarParameter` | `MarblePelletMediumDistanceScale`, **0.5**, group `Marble Pellets` |
| `MaterialExpressionConstant_0` | `Constant` | 1.0 |

### Wiring

```
Constant_0 (1.0)                  --> QualitySwitch_1.Default
MarblePelletMediumDistanceScale   --> QualitySwitch_1.Medium
QualitySwitch_1                   --> Custom_0.DistanceScale            (new input, 12th)

VertexNormalWS_0                  --> Custom_1.SurfaceNormal
FunctionInput_0 (Marbles)         --> Custom_1.Coverage
MarblePelletMaxRadius             --> Custom_1.MaxRadius
MarblePresenceGain                --> Custom_1.PresenceGain
MarbleMaxPresence                 --> Custom_1.MaxPresence
MarblePelletToneGain              --> Custom_1.ToneGain

Custom_0 (MarblePellets)          --> QualitySwitch_0.Default
Custom_1 (MarbleTone)             --> QualitySwitch_0.Low
QualitySwitch_0 --> ComponentMask_0 (RGB, PelletNormal) and ComponentMask_1 (A, PelletMask)
                                   (both moved; they read Custom_0 before)
```

Every input was read back with `get_expression_inputs`: both switches, `MarbleTone`'s six inputs,
the pellet node's twelve inputs and both masks.

**What each quality level gets:**

| `r.MaterialQualityLevel` | Reached by | Output |
|---|---|---|
| Low | `sg.EffectsQuality 0`, `scalability 0` | `MarbleTone`: the dark patch everywhere. The pellet node is not in this variant at all |
| Medium | `sg.EffectsQuality 1` | `MarblePellets` with `DistanceScale` 0.5: 2.5, 5 and 10 m |
| High, Epic | `sg.EffectsQuality 2` and up | `MarblePellets` with `DistanceScale` 1: 5, 10 and 20 m |

A `QualitySwitch` input left unconnected falls back to `Default`
(`MaterialExpressions.cpp:9067-9113`), so High and Epic need no wire.

**Setting a new Custom node's inputs.** A new Custom node starts with one empty input. Setting all
six named, connected inputs in one `set_properties` call was refused:
*"ArrayAdd: elements changed alongside the size change; insertion points are ambiguous."*
It worked in three steps:
1. rename the existing element, with the same size;
2. pass it back unchanged with five names appended;
3. connect each input with `connect_expressions`.

### `MarbleTone`'s HLSL, recorded verbatim

```hlsl
const float SmallestSizeShare = 0.6;
const float SizeShareSpread = 0.4;
float Fill = saturate(Coverage * PresenceGain);
float Presence = Fill * MaxPresence;
float LargestRadius = MaxRadius * (0.5 + 0.5 * Fill);
float MeanSquaredSizeShare = SmallestSizeShare * SmallestSizeShare + SmallestSizeShare * SizeShareSpread + SizeShareSpread * SizeShareSpread / 3.0;
float Tone = saturate(Presence * PI * LargestRadius * LargestRadius * MeanSquaredSizeShare);
return float4(normalize(SurfaceNormal), saturate(Tone * ToneGain));
```

Inputs: `SurfaceNormal`, `Coverage`, `MaxRadius`, `PresenceGain`, `MaxPresence`, `ToneGain`. It is the
same expected cover as the pellet node's `Tone`, with the full gain, and the flat surface normal.
**The formula is duplicated across the two nodes**, so a change to the pellets' size distribution
has to be made in both.

### The pellet node's HLSL, changed in two lines

The full code is in the entry *H2: the pellets decided by camera distance alone*, with these two
lines replaced:

```hlsl
float Resolved = 1.0 - smoothstep(FullDetailDistance * DistanceScale, ToneOnlyDistance * DistanceScale, CameraDistance);
float Gain = lerp(1.0, ToneGain, smoothstep(UnityGainDistance * DistanceScale, FullDetailDistance * DistanceScale, CameraDistance));
```

Inputs now in order: `Position`, `SurfaceNormal`, `Coverage`, `CellSize`, `MaxRadius`,
`PresenceGain`, `MaxPresence`, `FullDetailDistance`, `ToneOnlyDistance`, `ToneGain`,
`UnityGainDistance`, `DistanceScale`.

### What was verified, and what was not

Verified:
- Both materials recompiled in the editor with no error, and both are saved.
- In a `-game` run, `sg.EffectsQuality` was switched 3 → 1 → 0 → 3 by console command:
  - the three looks appear as designed;
  - no shader error was logged, so the Low and Medium variants compiled. The editor compiles only
    the active level;
  - `RacetrackDemo.SurfaceReport` gave identical peaks at every tier.

The numbers are in `MEASUREMENTS.md`, *H3: the quality tiers switched at runtime*.

Not verified:
- each tier's frame cost beyond single `ProfileGPU` frames, which is H4's;
- the `scalability <n>` console command itself. It sets `sg.EffectsQuality` among the other groups
  (`Scalability.cpp:767-879`) but was not run;
- the cooked shaders per quality level.

## 2026-10-12 — What the sports car is drawn with, read back over MCP (no change)

**No asset was changed.** No Play-In-Editor session was started, and the level was neither edited nor
saved. Every value here was read with `SkeletalMeshTools`, `StaticMeshTools` and
`ObjectTools.get_properties` while the editor was open for H5 (#77).

### `SKM_SportsCar` is not the visible car

| Read | Value |
|---|---|
| `SkeletalMeshTools.get_lod_count` | 1 |
| `get_vertex_count`, LOD 0 | 4 |
| `get_bounds` | origin (0, 0, 5.59), box extent (1.26, 1.26, 0) |
| `get_material_slots` | `M_SportsCarChasis_003` |

It is a flat quad 2.5 cm across, 5.6 cm up. The Vehicle template uses it as the skeleton and physics
carrier, `VehicleMesh`, with the wheel bones `Phys_Wheel_FL/FR/BL/BR`.

### `BP_VehicleAdvSportsCar`'s visible components

Read from the component templates `<Name>_GEN_VARIABLE` in
`/Game/VehicleTemplate/Blueprints/SportsCar/BP_VehicleAdvSportsCar.BP_VehicleAdvSportsCar_C`:

| Component | `StaticMesh` | Relative rotation | Relative scale |
|---|---|---|---|
| `Chasis_Main` | `/Game/Vehicles/SportsCar/SM_SportsCar` | 0 | 1 |
| `Chasis_Glass` | `/Game/Vehicles/SportsCar/SM_SportsCar_Glass` | 0 | 1 |
| `FL_Wheel_0` | `/Game/Vehicles/SportsCar/SM_SportsCar_Wheel` | yaw −90 | X −1 |
| `FR_Wheel_0` | `/Game/Vehicles/SportsCar/SM_SportsCar_Wheel` | yaw +90 | 1 |
| `BL_Wheel_0` | `/Game/Vehicles/SportsCar/SM_SportsCar_Wheel` | yaw −90 | X −1 |
| `BR_Wheel_0` | `/Game/Vehicles/SportsCar/SM_SportsCar_Wheel` | yaw +90 | 1 |

- Every relative location is 0, and no component has `OverrideMaterials`.
- The Blueprint also has a `Mesh Material` dynamic material instance variable, which was not read.
- **Not read:** each component's attach parent and socket. They live on the Blueprint's construction
  nodes, which `get_properties` cannot reach on the template (`AttachParent` and `AttachSocketName`
  could not be read). The wheels are taken to be attached at their wheel bones. The `-game` stills
  of `59db3b0` agree: the rims face out on both sides and the tyres sit on the road.

### The static meshes

| Mesh | `get_bounds` min | max | Notes |
|---|---|---|---|
| `SM_SportsCar` | (−223.3, −114.1, −2.4) | (247.5, 114.1, 120.9) | Nanite on, 11,093 triangles, LOD 0 |
| `SM_SportsCar_Glass` | (−216.3, −94.7, 7.8) | (218.7, 94.7, 73.9) | |
| `SM_SportsCar_Wheel` | (−39.3, −7.8, −39.3) | (39.3, 27.1, 39.3) | radius 39.3 cm, axle along Y |

### What uses this

`ASplineFollowingCar` (`59db3b0`) builds the AI cars the same way, in C++:
- `CarBody` is `SM_SportsCar` and `CarGlass` is `SM_SportsCar_Glass`.
- The four wheels are `SM_SportsCar_Wheel`, placed at the wheel bones' reference pose read from
  `SKM_SportsCar`, with the same per-side yaw and mirror.
- The body is scaled to the car's 140 cm wheelbase and grounded at the tyres: the lowest wheel
  centre minus the wheel's Z extent.

There is no editor-side change to rebuild.

## 2026-10-13 — E4 corrected: the asphalt sampled in world XY

**Why.** The user reported the tarmac looking stretched, with each section of road visible
separately. The cause is in the C++ record on
[#53](https://github.com/Balazs200001/RubberAccumDemo/issues/53#issuecomment-6141027625):
- The asphalt tiled UV0 900 times along the lap.
- `UProceduralMeshComponent` stores UVs as float16.
- From halfway round the lap, each 128 cm segment showed between 0.73x and 1.46x of its texture.

Editing this E4 asset was approved by the user.

**Asset changed:** `/Game/Materials/M_SurfaceAsphalt`.
- 10 nodes added, 4 inputs rewired, nothing deleted.
- Saved and verified not dirty. `git status` showed only this `.uasset` among the content.
- Host commit `06d0bfd`. The C++ that stopped setting the tile counts is `3f8e6e1`.

Made via: `MaterialTools.get_expressions`, `get_expression_inputs`, `get_expression_input_names`
and `get_expression_output_names` to confirm the graph matched the E4 record first; `add_expression`;
`ObjectTools.set_properties` and `get_properties`; `connect_expressions`; `recompile`;
`LogsToolset.GetLogEntries`; `AssetTools.save_assets` with the one path; `AssetTools.is_dirty`.

### Nodes added

| Node | Class | Settings |
|---|---|---|
| `MaterialExpressionWorldPosition_0` | `WorldPosition` | `WPT_ExcludeAllShaderOffsets`, as in `MF_MarbleLayer`, so the tarmac and the pellets share one space |
| `MaterialExpressionComponentMask_0` | `ComponentMask` | R, G |
| `MaterialExpressionScalarParameter_9` | `ScalarParameter` | `AsphaltScanCentimetres`, default **200**, group `Asphalt Tiling` |
| `MaterialExpressionDivide_0` | `Divide` | |
| `MaterialExpressionConstant3Vector_1` | `Constant3Vector` | (1, 1, 0) |
| `MaterialExpressionMultiply_5` | `Multiply` | |
| `MaterialExpressionTransform_0` | `Transform` | `TRANSFORMSOURCE_World` to `TRANSFORM_Tangent` |
| `MaterialExpressionComponentMask_1` | `ComponentMask` | R, G |
| `MaterialExpressionComponentMask_2` | `ComponentMask` | B |
| `MaterialExpressionAppendVector_1` | `AppendVector` | |

### Wiring

```
WorldPosition.XYZ --> Mask(R, G) --> Divide.A,  AsphaltScanCentimetres --> Divide.B
Divide --> UVs of TextureSample_0 (base colour), _1 (normal), _2 (ORM)       (all three rewired; were Multiply_0)

TextureSample_1.RGB x (1, 1, 0) --> Transform(World to Tangent) --> Mask(R, G) --> Append.A
TextureSample_1.RGB --> Mask(B) ------------------------------------------------> Append.B
Append --> Lerp_6.A                                         (rewired; was TextureSample_1.RGB)
```

Every input listed was read back with `get_expression_inputs`, and every property with
`get_properties`.

- **The normal map's rotation.** Sampled at world XY, the map's red and green lie along world X and
  Y. The material's normal is in the road's tangent frame, whose X runs along the lap (`GenerateRoadMesh`).
  So red and green, as a world vector `(R, G, 0)`, go through World to Tangent. Only the result's X
  and Y are kept, and the map's own blue stays as Z.
  - On flat road this is an exact rotation by the road's heading.
  - On a slope it projects world X and Y onto the road, which is how a top-down projection lays the
    texture on it. The road's own normal is kept, so the slope's shading is not lost.
- **The sign convention is the engine's.** `/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal`
  was read over MCP. Its top-down path does three things:
  1. samples at world position over a negated size (`Multiply_0`, `ConstB` −1);
  2. multiplies the sample by (−1, −1, 1) (`Multiply_2`, `Constant3Vector_3`);
  3. uses the result as a world normal.

  So its red and green run along the UV's own axes. Sampled at +XY over the size, as here, they are
  world +X and +Y.
- **Why not that engine function.** It is a triplanar function. Its `Z Texture` output transforms a
  world-space normal (`Transform_5`) built by `WorldAlignedNormals_HighQuality`. That is about 150
  nodes across the two assets, not traced far enough to defend on a slope. The road needs one
  top-down projection, which is the ten nodes above.
- **Two masks the graph does not strictly need.** `WorldPosition` has its own `XY` pin, and the
  normal sample has its own `B` pin. Each mask was already added before that was seen, and it is
  wired in rather than deleted, because of the point below.

### Left in the graph, disconnected

`TextureCoordinate_0`, `ScalarParameter_0` (`AsphaltTilesAlongLap`), `ScalarParameter_1`
(`AsphaltTilesAcross`), `AppendVector_0` and `Multiply_0` now reach no output.
- They are not compiled into the shader.
- The two parameters may still be listed on instances of the material. That was not checked.
- **They were not deleted over MCP.** `UMaterialEditingLibrary::DeleteMaterialExpression` ends in
  `Expression->MarkAsGarbage()` (`MaterialEditingLibrary.cpp:611`), the same call as the
  `DeleteMaterialExpressionInFunction` path that asserted `!IsRooted()` and halted the editor on
  2026-09-23 (line 1362).
- Selecting them and pressing Delete in the material editor removes them safely.

### What was verified, and what was not

Verified:
- The material recompiles with no material or shader error in the editor log.
- Every rewired input and every new property reads back as set.
- The asset is saved and not dirty.
- In a `-game` run, under the camera poses that showed sharp-edged bands across the road at 55 % and
  80 % of the lap before the change, the bands are gone. The stills are in `MEASUREMENTS.md`,
  *E4 corrected: the asphalt's stripes, before and after*.

Not verified:
- whether the normal map's lighting reads right, which is the user's judgement;
- whether the 2 m repeat of the scan, now regular in world space, shows from a distance;
- the asphalt on the steepest slope;
- the cooked shader.

## 2026-10-13 — R5 corrected: what the power-of-two ring count means for `L_SpaHarness` (no asset change)

**No asset was changed.** This records what a C++ change does to the editor-side setup. The change
and its reasons are on [#42](https://github.com/Balazs200001/RubberAccumDemo/issues/42) and in
`MEASUREMENTS.md`, *R5 corrected*.

- **`RoadSegmentLength` means something new.** It is still 128 on `RacetrackCircuit_0` in
  `L_SpaHarness`. It is now the *longest* a segment may be, not the length of each segment.
  `GetRoadSegmentCount()` rounds `ceil(lap / RoadSegmentLength)` up to a power of two, at most
  2048, so every ring's UV survives float16. On the 180000 cm lap that is 2048 rings of 87.9 cm, not
  1406 of 128.02 cm.
- **The map still holds the old road.** `UProceduralMeshComponent` serialises its sections into the
  map, and `L_SpaHarness` was not re-saved, so the file still holds the 1406-ring road.
  - The editor re-runs construction when it loads the level.
  - `-game` from the editor binary re-runs it on uncooked data (`World.cpp:5985-5988`).
  - So neither shows the saved mesh. A cooked build was not checked.
- **When the level is next saved,** the file grows with the bigger road, as recorded under C9. That
  is expected and not a leak. Check the actor list, not the file size.

## 2026-10-14 — Phase U: the demo's keys removed, the settings window drawn from code (no asset change)

**No asset was changed.** This records what Phase U does to the setup described above. The work and
its reasons are on [#129](https://github.com/Balazs200001/RubberAccumDemo/issues/129).

- **The keys recorded in earlier entries no longer exist.** That covers the circuit's
  `. , ] [ P M N C V L F K O`, including the drive key F chosen under C11. Host commit `61987fd`
  removed them.
  - Every setting is now in the "Demo settings" window. Every console command is unchanged.
  - `IMC_Vehicle_Default` is unchanged, and it is still the only mapping context added while driving.
- **The window is Slate in the host module**: `SDemoWindow` and `SRacetrackDemoPanel`, added over the game
  viewport by `RacetrackCircuit_0` at `BeginPlay`. No Widget Blueprint, material or level property is
  involved.
- **The input mode is set in code, not in `Config/DefaultInput.ini`.**
  - At `BeginPlay` the circuit sets `FInputModeGameAndUI` and shows the cursor. It ignores move and
    look input until the left mouse button is held on the scene.
  - The ini still has `bCaptureMouseOnLaunch=True` and
    `DefaultViewportMouseCaptureMode=CapturePermanently_IncludingInitialMouseDown`. The input mode
    releases the launch-time capture and switches the viewport to `CaptureDuringMouseDown`. The
    `-game` log shows both changes.
  - `L_SpaHarness`'s World Settings were not read. They are not needed, because the fly pawn is the
    default `ADefaultPawn`.
- **Captures with on-screen announcements now show the window.** Add `RacetrackDemo.Panel 0` to
  `-ExecCmds` to hide it. `DisableAllScreenMessages` hides it along with the announcements.

## 2026-10-26 — C11.9: a PS5 DualSense drives the car through RawInput (no asset change)

**No asset was changed.** The work and its reasons are on
[#138](https://github.com/Balazs200001/RubberAccumDemo/issues/138).

- **`RubberAccumDemo.uproject` enables the engine's `RawInput` plugin** (host `640260d`).
  - The plugin is marked deprecated in 5.8. The log prints "The Plugin RawInput has been marked
    deprecated for 5.8 and will be removed soon".
  - `GameInput` was enabled first (`5ce9551`) and swapped out in `640260d`. It needs Microsoft's
    GameInput redistributable installed on every machine that runs the demo, and the user decided
    against that.
  - `XInputDevice` is unchanged.
- **`Config/DefaultInput.ini` gains a `[/Script/RawInput.RawInputSettings]` section** (host
  `c487587`). It holds one device configuration, for vendor `0x054C` and product `0x0CE6`, the
  DualSense. Its six axis entries follow the pad's HID value-caps order, which is measured in
  `MEASUREMENTS.md` under the same date:

| Index | HID axis | Pad control | Emits |
|---|---|---|---|
| 0 | Ry | R2 | `Gamepad_RightTriggerAxis` |
| 1 | Rx | L2 | `Gamepad_LeftTriggerAxis` |
| 2 | Rz | right stick Y | off |
| 3 | Z | right stick X | off |
| 4 | Y | left stick Y | off |
| 5 | X | left stick X | `Gamepad_LeftX`, as a stick, −1 to 1 |

  - No buttons are configured. The RawInput debug view lists 96 `GenericUSBController_ButtonN`
    keys for the pad, and nothing binds them.
- **`IMC_Vehicle_Default`'s gamepad rows**, read back with `showdebug EnhancedInput` in a `-game`
  run after `RacetrackDemo.Drive`. MCP was not reachable, so this is not an MCP readback. Capture:
  `2627-Y3A-PR-243663/assets/media/c11-9-imc-vehicle-default-bindings-while-driving.png`.

| Action | Gamepad keys |
|---|---|
| `IA_Throttle` | Gamepad Right Trigger Axis, Gamepad Face Button Left |
| `IA_Brake` | Gamepad Left Trigger Axis, Gamepad Face Button Bottom |
| `IA_Steering` | Gamepad Left Thumbstick X-Axis |
| `IA_Handbrake` | Gamepad Left Shoulder |
| `IA_LookAround` | Gamepad Right Thumbstick X-Axis |
| `IA_Reset` | Gamepad Special Right |
| `IA_ToggleCamera` | Gamepad Special Left |
| `IA_Headlights` | none |

  The other rows are keyboard and VR controllers.
- **To use it, plug the pad in before starting the game or the editor.**
  - RawInput binds the devices present when it starts (`RawInputWindows.cpp:95-111`).
  - Its default registration asks for HID joysticks (usage 4) first, and for gamepads (usage 5) only
    when no joystick is connected. A DualSense is a gamepad, so a joystick-class device plugged in
    alongside it, such as a wheel or a flight stick, stops it being picked up.
  - It registers without `RIDEV_INPUTSINK` (`RawInputWindows.cpp:100`, `154`), so the pad only
    reaches the game while its window is in front.
  - `showdebug RawInput` shows each mapped axis's value.
- **Not verified:** driving with the pad. Both `-game` runs were started from a background shell.
  Windows kept the window out of the foreground in both: sampled once a second with
  `GetForegroundWindow`, it was in front for one sample, before the map loaded. No pad report
  reached RawInput, and the three axes read `--`.

## 2026-10-26 — C11.9 corrected: `IA_Brake`'s Pressed and Released triggers removed

**Asset changed:** `/Game/VehicleTemplate/Input/Actions/IA_Brake`, saved. No other asset changed.
`git status` afterwards showed only `Content/VehicleTemplate/Input/Actions/IA_Brake.uasset` under
`Content/`.

**Why.** The user drove with the DualSense and reported that after the first L2 press the brake
stayed slightly on. Pressing S on the keyboard cleared it, and the next L2 press brought it back.

- **The pad was not the cause.** Read from Windows over 5 s with nothing pressed, L2 and R2 both
  stayed at raw 0. RawInput re-sends every axis on every pad report (`RawInputWindows.cpp:710-740`).
- **The cause is `IA_Brake` together with the car's Blueprint**, read over MCP:
  - `IA_Brake` had two triggers on the action: `InputTriggerPressed_0` and `InputTriggerReleased_0`,
    both with `ActuationThreshold` 0.5. `IA_Throttle` and `IA_Steering` have none.
  - `BP_VehicleAdvPawnBase`'s EventGraph wires `IA_Brake` Triggered → `SetBrakeInput(ActionValue)`,
    Started → `BrakeLights(true)`, and Completed → `SetBrakeInput(ActionValue)` →
    `BrakeLights(false)`. The throttle and steering events are wired the same way, with their own
    setters.
  - Pressed fires on the frame the value reaches 0.5. Released fires on the first frame it drops
    below 0.5 (`InputTriggers.cpp:160-180`). Each carries that frame's value. A key goes 1 → 0 in one
    frame, so the release set the brake to 0. An analogue trigger passes through values under 0.5
    on its way down, so the release left the brake just under 0.5. Nothing changed it until the
    next brake event.
  - `bReverseAsBrake` then turns any brake above 0 into full brake while the car moves forward
    without throttle (`ChaosVehicleMovementComponent.cpp:1144-1152`).
- **Change.** `Triggers` was set to an empty array. Read back: `ValueType` `Axis1D`, `Triggers` `[]`,
  `Modifiers` `[]`, `bConsumeInput` true.
  - With no triggers an action is Triggered while its value is non-zero and None at zero
    (`EnhancedPlayerInput.cpp:241`, `689`). The brake now follows L2 every frame and Completed sets
    it to 0, the way `IA_Throttle` already behaved.
  - The S key's brake is unchanged: 1 while held, 0 on release.
  - The brake lights still switch on at Started and off at Completed.
- **Also uses `IA_Brake`:** `IMC_SteeringWheel`, which the host never adds, and `IMC_Vehicle_Default`.
  Neither was changed.
- **Not verified:** the drive with the pad after the change. Nobody had pressed L2 since.

**Verified later the same day, by the user driving in PIE with the DualSense on USB.** Throttle,
brake and steering all worked from the pad, and the brake let go fully after L2. Bluetooth was not
tried.

## 2026-10-26 — S2 and S3: the trackside meshes and materials, in `/Game/Trackside/`

**Assets created, all new and all saved.** No existing asset was changed. Afterwards `git status`
showed only `Content/Trackside/` under `Content/`.

| Asset | What it is |
|---|---|
| `SM_GrandstandOpen` | An open stand: 7 tiers and blue seat blocks. 328 triangles, slots `Concrete` and `OpenStandSeats` |
| `SM_GrandstandCovered` | The same stand with a back wall up to a roof, and red seat blocks. 340 triangles, slots `Concrete`, `CoveredStandSeats` and `Roof` |
| `SM_FencePost` | A catch-fence post, 2.6 m, with its top 40 cm leaning 20 cm toward the track. 24 triangles, slot `FenceSteel` |
| `SM_ArmcoPost` | An Armco post from 25 cm below the ground to 62 cm above it. 12 triangles, slot `ArmcoSteel` |
| `M_Trackside` | Opaque. Base colour = `VertexColor` × `Colour`, default white. Roughness = `Roughness`, default 0.85 |
| `M_TracksideFence` | Masked and two-sided. `Colour` (0.03, 0.035, 0.035) to base colour, and `Roughness` 0.6. Its opacity mask is described below |
| `MI_TracksideConcrete` | `M_Trackside`, `Colour` (0.32, 0.32, 0.31) |
| `MI_TracksideOpenStandSeats` | `M_Trackside`, `Colour` (0.015, 0.07, 0.30) |
| `MI_TracksideCoveredStandSeats` | `M_Trackside`, `Colour` (0.45, 0.015, 0.02) |
| `MI_TracksideRoof` | `M_Trackside`, `Colour` (0.55, 0.56, 0.57) |
| `MI_TracksideFenceSteel` | `M_Trackside`, `Colour` (0.06, 0.07, 0.065) |
| `MI_TracksideArmcoSteel` | `M_Trackside`, `Colour` (0.22, 0.23, 0.24), `Roughness` 0.45 |

The colours are linear. Each mesh slot has the instance of the same name assigned
(`StaticMeshTools.set_material`).

**Why.** Phase S (#139) dresses `L_SpaHarness` as Spa. Everything that follows the track is generated
in C++. These are the separate objects that are placed many times, and the materials everything uses.

**The meshes come from a script, not from hand modelling.** `RawContent/Trackside/MakeTracksideMeshes.py`
builds all four with `bmesh` in Blender 4.3.2, in a scene of its own called `TracksideMeshes`. It
exports each one to FBX next to itself. To rebuild them, run it inside Blender, for example
`exec(compile(open(path).read(), path, "exec"), {"__file__": path, "__name__": "__main__"})`, then
reimport the four FBX files.
- **Units and axes carry through unchanged.** Blender is in metres and the importer converts to
  centimetres. Blender's +X is the engine's +X, read back with `StaticMeshTools.get_bounds`.
  - `SM_GrandstandOpen` runs from X 0 to 705, Y ±600 and Z −400 to 525.
  - `SM_GrandstandCovered` runs from X −60 to 705, because its roof overhangs the front, and up to
    Z 678.
  - `SM_FencePost` runs from X −24 to 4, leaning toward −X.
- **So every mesh faces the track along −X.** A stand's front is at X = 0, and it rises away from
  the track toward +X. The C++ turns each instance so that its +X points away from the track.
- **Each stand has a plinth down to 4 m below its origin**, so it does not float where the ground
  falls away behind a barrier.

**Imported over MCP** with `StaticMeshTools.import_file`, without materials or textures, and with
combined meshes. Then:
- `remove_collisions` on all four, because the legacy FBX import's `bAutoGenerateCollision`
  defaults to true. The scenery has no collision.
- `is_nanite_enabled` read false on all four, and left that way, since they are a few hundred
  triangles each.

**Why slots and instances rather than vertex colours.**
- **The MCP import goes through the legacy `FbxFactory`.** It refuses `.glb`, with *"FbxFactory does
  not support ".glb" files. Allowed: ['fbx', 'obj']"*.
- **The legacy FBX path ignores vertex colours by default**, with
  `VertexColorImportOption=EVertexColorImportOption::Ignore`
  (`Engine/Config/BaseEditorPerProjectUserSettings.ini:677`). `import_file` does not expose the
  option.
- **So each part of a mesh is its own material slot**, and each slot gets an instance of
  `M_Trackside` with its colour. That is one draw per slot, seven over the four meshes.
- **The procedural meshes still use vertex colours.** `M_Trackside` multiplies `VertexColor` by
  `Colour`, and each path leaves the other at white:
  - An imported mesh without colours has white vertex colours.
  - The procedural meshes keep `Colour` at its white default.
- **The vertex colours are linear.** The procedural mesh writes them without conversion when
  `CreateMeshSection_LinearColor`'s `bSRGBConversion` is false (`ProceduralMeshComponent.cpp:541`).
  `M_Trackside` reads `VertexColor` directly.

**`M_TracksideFence`'s opacity mask** is a wire grid, cut from UV0:
- `TexCoord` × `CellsPerMetre` (8) → `Frac` → its R and G, each through `ComponentMask`.
- Each goes to `Step`, with Y the component and X `WireWidth` (0.14).
- `Max` of the two → Opacity Mask.

`Step` compiles to `step(Y, X)`, which is 1 where X ≥ Y (`MaterialExpressions.cpp:4796-4803`). So the
mask is 1 within 14 % of a cell edge in either direction. The C++ gives the fence panel UVs in metres,
so a cell is 12.5 cm. `BlendMode` `BLEND_Masked`, `TwoSided` true and `OpacityMaskClipValue` 0.3333
were read back.

**Shown:** `CaptureAssetImage` of `SM_GrandstandCovered` drew the concrete stand, the red seat rows
and the roof.

**Not verified:** any of it in the level, which comes with S4 to S8. Also not verified is whether
the colours look right, which is the user's call.

## 2026-10-26 — S8: the trackside scenery placed in `L_SpaHarness`

**Asset changed:** `/Game/Maps/L_SpaHarness`, saved, host `41c5a44`. `git status` showed only
`Content/Maps/L_SpaHarness.umap` under `Content/`.

**Change.** One actor was added: `TracksideScenery_1`, labelled `TracksideScenery`, of class
`ATracksideScenery`, at the origin. Its `Circuit` is `RacetrackCircuit_0`. Every other property is at
its class default, so the materials and meshes are the `/Game/Trackside/` assets found by
`ConstructorHelpers`.

**Why.** Phase S (#139) dresses the map as Spa.

**Made via:** `SceneTools.add_to_scene_from_class`, then `ObjectTools.set_properties` for `Circuit`,
then `AssetTools.save_assets`. `AssetTools.is_dirty` read false afterwards.

**The scenery's geometry is not in the map.**
- **The first save stored it**, and the map came out at **127,923,179 bytes**, against the committed
  5,620,382. The procedural mesh keeps its vertices in `ProcMeshSections`, a tagged `UPROPERTY` array
  (`ProceduralMeshComponent.h:333-334`). At about 190,000 vertices that came to roughly 640 bytes
  each.
- **Since host `8ae6190`, the six generated components are transient** default subobjects,
  `CreateDefaultSubobject(..., true)`, behind `Transient` pointers. `SceneryRoot` is not transient, so
  the actor's transform is still saved.
- **The actor now adds 1,713 bytes.** The map saved without it is 8,062,424 bytes, and with it
  8,064,137.
- **The other 2.44 MB over the committed map is not the scenery**, and it was not identified. The map
  had last been saved on 10-08 (`fa4ee03`). The growth fits the circuit's road and walls being saved
  as the current code builds them, but that was not checked.

**So the scenery is rebuilt at every load.**
- **Construction scripts rerun when a level loads in an uncooked world:**
  - in the editor (`World.cpp:2764`, then `Level.cpp:1909`);
  - in `-game` from the editor binary (`World.cpp:5987`).
- **PIE duplicates the editor world without rerunning them**, so `BeginPlay` rebuilds whenever the
  ground has no sections.
- **The cost.** A full rebuild logged `TracksideScenery_1 rebuilt the scenery in 187.3 ms` in the
  editor.

**To rebuild it by hand** after changing the circuit, select `TracksideScenery` and press `Rebuild`
under *Trackside Scenery* in its details. Changing any of its properties also rebuilds it.

**Not verified:**
- the scenery in PIE or in a cooked build;
- the scenery in `-game`, until S8's captures (#147).
