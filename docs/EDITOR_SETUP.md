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
