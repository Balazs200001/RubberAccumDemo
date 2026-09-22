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
