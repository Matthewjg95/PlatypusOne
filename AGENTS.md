# Repository agent entrypoint

Before changing PlatypusOne:

1. Read `STATUS.md`.
2. Read `docs/ARCHITECTURE.md` and the architecture document relevant to the task.
3. Inspect open issues and pull requests before creating overlapping work.
4. Treat verified code/tests and preserved physical evidence as stronger than plans.
5. Keep **observed / derived / inferred / unresolved** information separate.
6. Never invent a physical test result, silently promote a vendor specification
   to bench evidence, or tune a measurement algorithm without preserved evidence.
7. Current post-Dream-Lab work:
   - issue #32 — stabilization/evidence baseline;
   - issue #33 — Rev A perception carrier gate (PCB status: `hardware/pcb/README.md`);
   - issue #37 — Sketch Intent Resolver.
8. The canonical Tab5/M024/Pololu VL53L8CX experiment now lives in
   `Matthewjg95/platypus-lab`, issue #3. Platypus Lab owns its raw evidence;
   PlatypusOne consumes that evidence through issue #33.
9. ShadowScan Mobile (`Matthewjg95/shadowscan-mobile`, issue #1) is the
   active Pixel 4 planar-RGB evidence path. Reuse compatible provenance,
   geometry and constraint concepts with issue #37 rather than forking them.
10. Prefer deterministic, replayable engineering logic before opaque AI.
11. Leave a durable handoff in the relevant issue/doc so another agent can
    continue without chat history.
