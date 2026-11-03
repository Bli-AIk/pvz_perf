# Selection layer and slider settings

2026-11-04, Windows Vulkan production Release objects, LTO; PVZ_FRAME_DIAGNOSTICS=OFF.

- Actual bottom/top UI passes with an intervening opaque occluder: selected bank card and shovel frames remain occluded at widths 800 and 1400.
- Full touch-input runtime suite passed, including field-card atlas crop references, shared selection, cannons, swept pickups, and garden tools.
- Production settings reader/writer: out-of-range values clamp to 0..100; all 101 integer slider percentages round-trip exactly. SFX gain is slider percent / 100 * 0.65.
- No serialized fields changed. No extra render-list item or full-pool selection scan added.
- Tests used isolated temporary configuration; player saves were not loaded. Current Release sfxVolume was changed from legacy gain value 65 to slider value 100 to preserve its loudness.

`touch-runtime.txt` retains the complete regression output; `settings-runtime.txt` is the settings integration result. These are correctness checks, not performance measurements.
