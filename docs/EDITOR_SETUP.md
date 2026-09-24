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

> **Superseded 2026-09-23 by [C8](PLAN.md). The reasoning above is not wrong; it answers a
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
[PLAN.md](PLAN.md) and [MEASUREMENTS.md](MEASUREMENTS.md); the short version is that a valid road
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
Why: [C8](PLAN.md) rescales the circuit to 480 m at 3.125 cm per texel on an 800 cm track. Two of
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
anything. Full reasoning in [PLAN.md](PLAN.md) under C9.

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

One asset changed. Full reasoning in [PLAN.md](PLAN.md) under C10.2.

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
