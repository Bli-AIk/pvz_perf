# Offline atlas search cache

The current records include validated 4096-bound layouts and prior geometry checkpoints.
They contain rectangle layouts and solver metadata, not image pixels or runtime
textures. Production uses them only after checking the geometry, padding,
maximum side, model, OR-Tools version, record digest and completed-search state.

The matching solver is pinned to OR-Tools **9.15.6755** by the separate
`PvzDevTools` C++ toolchain. Current production configuration
allows a 60-second search per atlas. Records that exhausted that budget without
proof remain explicitly unproven; their status, lower bound and gap are retained.
Strict builds continue solving those records instead of treating them as proofs.

The normal resource pipeline seeds only missing local cache entries. Existing
local searches are preserved. Changed geometry, model or solver versions cause
the affected source cache entries to be skipped.

This directory is part of the source checkout and is not included in
`main.pak64`. It is distributed with the Git repository and GitHub's source
archives, under `assets/definitions/atlas-cache/`. It is not a runtime asset.

The configured width/height bound is now 4096. Wider-domain records are never
used if their geometry exceeds this bound. A retained proof over a larger domain
is still valid for a fitting layout; a completed bounded search remains unproven
and records its source cache key and original search domain. New resource builds
include the current-bound keys, retaining their actual status, bound and gap.
