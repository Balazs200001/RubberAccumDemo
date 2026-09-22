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
