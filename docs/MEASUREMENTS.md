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

---

## Status

**No measurements taken yet.**

The week 2 feasibility gate has not run. Every performance figure in `project-proposal.md`
section 3 is an estimate carried over from the proposal, including:

- the 4096 by 4096 RGBA8 target being 64 MB
- roughly 5 cm per texel over a 200 m square, 37 cm over a 1.5 km circuit
- eighty wheels being 320 bytes of readback
- readback arriving two or three frames late
- the 0.5 ms GPU budget for four contact points at 60 Hz

None of these have been confirmed on this machine. They are arithmetic and reasonable
expectations, not results. Phase B in [PLAN.md](PLAN.md) exists to replace them with real
figures, and this file stays empty until it runs.
