# Static definitions, CPU particles and NRSC4 resources

This page describes the current runtime resource contract and the retained offline interchange formats. Reanimation and projectile behavior is generated as static C++ data; particles load complete PTC1 CPU records. Definitions share behavior and select an appearance row with its atlas pointer and final UVs. Reanimation/particle evaluation and drawing preparation run on the CPU; there are no GRU1/PGP1 GPU animation records in the runtime package.

## Runtime sampling contract

`Sexy::TextureRegion` is a public aggregate:

```cpp
struct TextureRegion {
    float mU0, mV0, mU1, mV1;
    int mWidth, mHeight;
};
```

The four floats are normalized source UV bounds. Width and height are independent
logical drawing dimensions, not atlas dimensions. The offline generator divides
packed pixel coordinates by the atlas extent when producing C++ initializers;
runtime atlas draws submit those UVs directly. `TextureSource` contains only a
borrowed `GpuTexture* mTexture` and sampling flags. It contains neither dimensions
nor an owning image pointer.

Reanimation/particle appearance rows, projectile/shadow visuals, scene views and
Zombatar entries use this same region aggregate. Definition appearance dimension
arrays, projectile/Zombatar atlas-size members and public scene Width/Height
constants are removed. Generated loaders pass expected PNG dimensions as literals
for validation, without storing another sampling extent in runtime definitions.
Backend textures and ordinary `Image` retain dimensions needed for allocation,
render targets, clone, pixel operations and dynamic crops.

Font base glyphs, scaled glyph regions and queued glyph commands also store UV
regions with logical dimensions. Loading/resize operations temporarily reconstruct
pixel regions for exact integer-weight GPU font resampling. Button image states
and the Coin draw table store UV regions; unused checkbox source fields are removed.
Screen destinations, clipping and collision rectangles remain pixel geometry.
Floating drawing-offset changes are deferred. This representation change does
not alter PNG pixels, pak/offline interchange or save layouts.

The program has 210 named `Image*` bindings declared in `Resources.h`; scene atlas
migration removed 76 declaration/definition pairs with no remaining C++ consumer.
Runtime `ResourceId`, `ImageSymbolId` and font SymbolId registries are retired;
`ProgramImageSymbols.h/.cpp` have been removed. Generated loading functions
assign named pointers and retain a named `SharedImageRef` owner for each image.
There is no global image number, resource-slot table or pointer-to-ID lookup.
NRSC4 covers the named assignment contract and loading payload with separate
CRCs; the executable, generated bindings and package must be rebuilt together.
All twelve QuickPlay PNG sources remain in `assets/runtime/images/`, while
their UI code, runtime bindings, descriptors and package entries are removed.
The 879 historical `ResourceId` identities used by
old saves remain frozen in the converter's `FrozenResourceIds.h`; the runtime
does not keep a mixed 879-slot table or typed-null placeholders for them. Released v2.0
artifacts retain their original program registry. The definition atlas bytes and
compiled definition records are independent of ordinary image binding retirement.
The frozen 2,287-entry historical image mapping is retained for offline
reproduction; it is not a production build input or runtime texture registry.

## Offline projectile resource record

The projectile DSL frontend accepts the existing fourteen `ProjectileType`
names, PNG source paths and finite positive float scales. The compiler outputs
source metadata, then the common atlas pipeline packs the thirteen unique
images, extrudes two pixels, and emits one atlas PNG. The fire pea has no direct
image; normal and zombie pea currently share a source, but mods may give them
different PNGs. The DSL also specifies day/night shadow source crops.

The intermediate `compiled/projectiles/projectiles.compiled` is a raw 332-byte little-endian PRJ1
record, not an animation envelope:

| Field | Encoding |
| --- | --- |
| Magic, version, type count, atlas width, atlas height | five u32: `0x314A5250`, 1, 14, width, height |
| Projectile visuals in `ProjectileType` order | fourteen `{i32 x,y,width,height; f32 scale}` |
| Day and night shadows | two `{i32 x,y,width,height}` |

It contains neither paths nor image symbol IDs. Generated image bindings select
the atlas; the projectile resource object retains one borrowed GPU pointer and
fixed inline `TextureRegion` arrays in static storage; no atlas width/height
members remain. The PRJ1 pixel coordinates above are only offline interchange.
Default puff scale 0.3 preserves its
original 0.3-to-1 age curve; authored changes multiply that curve by
`scale / 0.3`. Other authored scales replace the old draw table values.
Cob collision size uses its logical source dimensions; the other object sizes remain
40 by 40. Projectile type IDs, motion algorithms and save fields do not change.

## Complete static definitions

The build configuration declares `definition_order(reanim ENUM:Definition ...)`
and `definition_order(particle ENUM:Definition ...)`. These drive the reanimation
type map and generated `GeneratedDefinitions.h/.cpp`; the build checks the
runtime catalogs and emits a `static_assert` for every numeric enum value.
There are 144 runtime reanimation IDs and 106 particle IDs. The two additional
authored reanimation roots `Zombie_dancer` and `Zombie_Jackson` have no runtime slots
and their exclusive atlases are omitted from the runtime package.

Each definition's sampling rows are one dense `TextureRegion[]`, indexed as
`variant * slots + imageSlot`. Reanimation roots and eight `ReanimData*.cpp`
shards retain static tracks, transforms, UVs, option transitions and precomputed
attacher action ranges. Particle roots start empty and load their tracks,
fields, prefilled defaults and UV rows from `PTC1`; the root owns these arrays
until release. The former eight `ParticleData*.cpp` shards are removed.
Generated particle C++ contains only root slots, compiled paths and enum checks.
No instance duplicates loaded root behavior. Temporary decoded atlas pixels are discarded after upload.
The current corpus contains 398381 reanimation frames, 5328 particle curve
nodes and 689 precomputed attacher segments.

## Compiled particle CPU records

Particle roots contain all CPU simulation and drawing tracks. PTC1 is an explicit little-endian stream, without native pointers or copied C++ aggregates. Appearance rows share the root's emitter, field and track arrays.

Each `compiled/particle-runtime/<root>.compiled` contains:

| Section | Layout |
| --- | --- |
| Header | six u32: `PTC1` (`0x31435450`), emitter-name count, emitter-name registry hash, emitter count, variant count, slots per variant |
| Emitter header | nine words: u32 region offset/count; i32 image frames, animated, flags; u32 emitter type, name ID, duration-target index, render-track mask. Target `0xffffffff` means none |
| Emitter tracks and fields | 22 system tracks, particle-field array, system-field array, then 16 particle tracks in `TodEmitterDefinition` declaration order |
| Track | u32 node count followed by `{f64 time; f32 low,high; u32 curve,distribution}` nodes, 24 bytes each |
| Field array | u32 count followed by `{u32 type; Track x; Track y}` records; at most four |
| Each variant | u32 atlas width, height and UTF-8 path byte count; path bytes with no terminator; then one `{f32 U0,V0,U1,V1; i32 width,height}` per slot |

Defaults are filled offline. Authored time fractions are converted to double
once during compilation, preserving values such as `1/3` without decimal
quantization. Atlas dimensions and paths are loading inputs; the loaded root
retains UV rows and borrowed GPU texture slots. The reader checks file length,
registry identity, count products, field types, node values/IDs, emitter slot
ranges, target/name indices, path lengths and UV bounds. A failed load unbinds
pending atlas destinations and frees every partially allocated array. A later
load may return different addresses while preserving all authored values.

## Package layout

Runtime particle records are shipped as 106 `compiled/particle-runtime/*.compiled` CPU records.
The original `compiled/particles/` PAV6 records remain offline intermediates;
reanimation CPU behavior and projectile PRJ1 records are also offline.
The package additionally includes atlas PNGs,
20 font records and one NRSC4 resource loading plan. `DefinitionImageBindings.h` and the
manifest retain offline verification metadata; the game does not include that
header. The remaining wire-format sections below document offline interchange.

An appearance
defaults to a sibling `<appearance-name>.atlas.png`; explicit groups can share
that image. The default row uses the root name, and generated reanimation appearance names use
`<Root>_V<index>` with an eight-digit decimal index. Explicit groups instead use
`compiled/atlases/<group>.atlas.png`. These paths are literal calls generated
in `GeneratedDefinitionLoading.cpp` for reanimations or length-prefixed strings
inside PTC1 for particles.
Loading code assigns the definition's texture bindings directly.
An image-free row has no PNG and uses empty UV regions. Particle and reanimation
roots retain `mTextures`, indexed by appearance, plus a flat `mImageRects`
table of `TextureRegion` values indexed by appearance and image slot. They do not
retain per-appearance atlas dimensions. A particle emitter
stores an offset/count into each appearance row, while a shared reanimation
transform stores one slot index. No image or atlas
wrapper lies on the drawing path. A path-indexed loading registry
decodes each live path once. The renderer owns the shared GPU texture pointers
and their diagnostic dimensions. Definitions are registered only at their final addresses. The
primary-thread loading barrier uploads each shared atlas once, assigns the
same GPU pointer to its waiting definitions, and discards assignment records.
Early loading-screen definitions bind on the primary thread before drawing.
Individual definition cleanup cancels pending assignment and clears its borrowed pointer.
`DefinitionFreeAtlasImages()` releases any unfinished decode payload and asks
the renderer to free shared atlas textures after all particle and reanimation
definitions are freed, while the renderer is still alive.
No global image symbol is created for the group.

CPU and GPU images have separate C++ types. `MemoryImage` is an independent CPU
bitmap utility and cannot be passed to drawing or GPU upload interfaces.
`Image` is the concrete ordinary GPU image, directly owning one `GpuTexture*`
plus dimensions and ordinary drawing/lifetime state. It stores no row/column
counts or animation rectangle table.
There is no separate drawing-image subclass or heap-allocated texture-data
wrapper. Definition textures bypass `Image` entirely.
There is no `ImageType`, RTTI or virtual type dispatch. Image copying explicitly
clones GPU pixels; copy assignment is forbidden. Render-target copies retain
their writable target capability and ignore the caller's draw transforms and filters.

GPU image objects retain no file path, drawn marker, bitmap or palette.
The raw `ImageLib::Image*` loading payload contains decoded pixels and optional
alpha layers. Successful upload deletes that entire payload; sampling and
explicit diagnostic readback use GPU resources. Definitions and image owners
use raw pointers, with explicit cleanup on load failures. This retains the
current load-at-startup/free-at-shutdown policy; dynamic eviction is not added.
Before ordinary upload, pending alpha analysis runs even when shared-image
loading deferred it. Uploaded images return their existing texture directly;
no CPU revision/format re-upload protocol remains. Font alpha therefore cannot
be inferred from uncommitted default flags. Filter effects use sampling flags
on the original texture, without separate filter-image objects or back-pointers.
Shared cache references retain a direct Image pointer for drawing; their cache
control pointer is used only for lifetime/accounting.

`GpuTexture` remains a build-time alias for the selected backend resource object,
not an alias for its native API handle. OpenGL and D3D9 texture objects now embed
their native resources directly, with no separate Storage allocation or shared
ownership. Their draw batches borrow native texture handles. The renderer owns
live texture objects and retains destroyed/replaced versions and feedback
snapshots until queued draws are issued by Flush; destruction does not force
an extra Flush. Other backend resource representations are unchanged.

Reanimation transforms contain a `u32 mImageSlot`, rather than a rectangle or
image pointer. The definition's flat table resolves `(instance.mVariant,
transform.mImageSlot)` to a final 24-byte UV/logical-size region. The offline packer
resolves inherited source sheets and discrete cells into those slots; runtime
transforms carry no rows or columns. The original frame value remains for
visibility and animation behavior. Particle source definitions instead contain
ordered independent images:

```text
Emitter {
    images: [
        "particles/frames/zombie_boss_fireball_particles_r0_c0.png",
        "particles/frames/zombie_boss_fireball_particles_r0_c1.png"
    ];
    imageFrames: 2;
}
```

`image: "IMAGE_NAME";` is a single-image shorthand; `image: none;` or
`images: [];` has no image. `imageFrames` remains the logical animation/random
range so random calls and saved frame values do not change. It may differ from
the image-list length; the selected frame clamps to the last available image.
The list preserves order and duplicate entries. Production particle DSL rejects
`imageRow`, `imageCol`, and source rectangles.

Original grid sheets were split by integer pixel crops into lossless RGBA PNGs
without resampling, quantization, rotation or premultiplication. Every PNG is
decoded again and compared byte-for-byte with its logical source crop, including
alpha and RGB under transparent pixels. `particle-frame-images.json` records the
source, crop and RGBA digest for reproducible validation. Original sheets remain
available to other consumers. Each independent frame participates in packing;
frames need not be adjacent, equal-sized, or ordered spatially in the atlas.
Reanimation per-track `Image` overrides have been removed.

### Reanimation appearance rows

Each reanimation root retains one `ReanimationType`, one static definition and one
shared set of tracks, transforms, fonts, captions and attacher behavior. Its
appearance variants supply only image geometry and texture bindings. Instances
store one `u32 mVariant`; a selection changes that index, without copying behavior
or replacing per-track pointers.

Authoring `variant-of` declarations provide a replacement-image list for every
root track and every nonempty original-image use in that track. These authoring
slots list each unique original image once, in order of first appearance within
that track; the outer track list and each inner image list are replaced in full.
Compilation further distinguishes runtime slots by
`(track, original image, original cell)` so the shared transform behavior
contains only one numeric slot per frame. Reusing the original source image
retains its original cell; replacing it uses the full replacement image.
`none` produces an all-zero sampling region. It skips that sprite without hiding
attachments on the same track.

Row zero is the ordinary appearance. Slot zero is all zero in every row; every
other slot in row zero is valid geometry. Other rows may blank individual slots.
The definition references a flat static `[variant][slot]` UV-region array and one texture
binding per row. Related rows share textures only when the build configuration
explicitly groups them, for example with a `Root_V*` member pattern. Authoring
appearance identities do not create extra runtime types or independent compiled
definitions.

Generated options and the sorted `(trackId, option)` action table select a
variant using precomputed transitions. An action preserves choices on unrelated
tracks. Compilation derives direct action ranges per track; drawing selects the
instance's row directly, without an inheritance chain, image lookup or per-frame
geometry allocation. `ReanimAppearanceSaveCatalog.h` separately describes
historical ResourceId choices and mixed-radix indices for the offline save
converter; it is not a rendering lookup table.

The compiler emits whole-appearance enums in `ReanimVariantId.h`,
with each root represented by `enum class ReanimVariants::<Root>`, containing
`Default` and named `_V00000001` rows for
direct `SelectVariant` calls. Separately,
`tools/generate_reanim_appearances.py` emits the track-choice constants in
`ReanimVariantOptions.h`, authoring declarations, action transitions and save
catalog. `SetVariantOption(track, option)` applies a transition from the current
variant and preserves the other axes; a track option is not a whole-appearance
index. The production authoring catalog has 31 appearance families and 2,717
rows including their default rows, with 166 actions. Transition targets occupy
210,136 bytes before runtime allocation overhead. These counts describe data
structure, not a timing measurement.

### Particle appearance definitions

Particle components support appearance variants that inherit their parent's
behavior:

```text
export component ZombieMustache2 {
    kind: particle;
    source-name: "zombiemustache2";
    variant-of: ZombieMustache;
    images: [["IMAGE_REANIM_ZOMBIE_MUSTACHE2"]];
}
```

The compiler resolves `variant-of` across the complete input list. The outer
`images` list must have exactly one entry per inherited emitter, and every
inner list must have exactly the same number of images as that emitter's
inherited list. Replacements preserve order, including duplicate uses and
empty lists. This count is the number of authored images, not the emitter's
logical `imageFrames` value. Replacement images may have different pixel
dimensions. A variant declares only its identity, parent and replacement
images; it cannot declare emitters or override behavior. Cycles, missing bases,
duplicate identities and either kind of image-count mismatch are build errors.

The compiler resolves parent dependencies, validates cycles and expands
inherited emitter settings temporarily for validation and packing. Final
`PAV6` records contain one shared `IDR6` behavior body followed by all appearance
rows in the same root file. Parent paths and separate child compiled records
are not shipped. The source image name is an offline label, not a runtime
image identity.

The runtime loads one root's emitters, fields, tracks and flat UV-region rows
from PTC1 and binds its textures for all appearances. Each system stores only
`mEffectType` and `mVariant`. An emitter's
offset/count selects the same range within every row. Loading does not recurse
through parent definitions, and update/drawing do not follow an inheritance
chain. The former IDV1 runtime dependency format has been removed.

The seven existing head/arm/mustache/boss-spit author files now contain their
required appearance definitions. The 106 original effects and 56 concrete
appearances (33 heads, 20 arms, two mustaches, one ice burst) now have 106 root
records. `ParticleEffect` retains the original 106-effect order. Each root's
`ParticleVariantId.h` enum has `Default = 0`, followed by its appearance
components in lexical order. The former alphabetically ordered 162 identities
are frozen only in the offline save converter's historical tables.
The ordinary/default appearances are still needed by
gameplay; they are not a legacy fallback. Old files and runnable packages were
backed up outside the build tree before replacement. Related appearances share
atlases using `ResourceBuild.pvzbuild`; no runtime Image pointer override remains.

DAT64 v4 stores the original base effect ID at system-record offset 0 and a
`u32` internal variant at offset 4, using the existing padding without increasing
record size. No supplemental system definition ID or emitter image ResourceId
is stored. Old saves pass through the offline converter with
`--target dat64-v4` and an explicit output path. It maps historical
effect/image pairs and v2 definition IDs to base/variant
pairs, preserving particle state, frames and nanoseconds. Unknown appearances
fail conversion. The converter supports only original 1073 DAT, DAT64 v1, v2 and v4;
all v3-series inputs and targets are rejected. The game accepts only
v4; supported older headers require offline upgrade.

The authoring DSL and original source images remain build inputs. The standalone
compiler supplies a temporary `.source-record` and its own `image_sources.txt`;
its local source handles refer only to parsed author image names. It neither
reads program image bindings nor accepts `--image-registry`. The package
stage resolves source names from authoring metadata and paths, creates the atlas
and writes rectangle rows after shared behavior in the final v6 envelope. Intermediate files are
not shipped as runtime definitions. The compiler's `particle_inheritance.json`
sidecar maps each variant's canonical compiled path to its immediate parent's
path; the packer uses this compiler-only mapping to collect every appearance
under its root. Native storage contains 144 static runtime reanimation roots
and 106 particle root slots, each loaded from a single CPU record and GPU record,
rather than one behavior object per appearance.
The offline Python stage requires Pillow; the game itself has no Python or
Pillow dependency.
Exact packing additionally requires OR-Tools in the build Python environment.
Install the offline dependencies with
`python -m pip install -r tools/resourcePipeline/requirements.txt`.

## Build configuration

`assets/definitions/ResourceBuild.pvzbuild` is the build entry point.
Reanimation and particle `.dsl` files contain individual definitions. The old
`reanim/root.dsl` and `particles/all.root.dsl` aggregators are removed; the
pipeline passes explicit source lists to the native compiler, without creating
a replacement aggregate DSL.

```text
resource_build(1)
sources(reanim "reanim/units/*.dsl")
sources(particle "particles/particles/*.particle.dsl")
atlas_defaults(solver exact time_limit_seconds 60 workers 1 require_optimal false)
atlas(pea_family reanim:PeaShooter reanim:SnowPea)
# A group can also mix reanim:Name and particle:Name.
# Optional strict override: this group must prove its global minimum.
atlas_settings(pea_family time_limit_seconds unlimited require_optimal true)
```

Ungrouped definitions receive individual atlases. A definition may belong to
only one explicit group. Group members share the union of their source images,
deduplicated by authoring symbol, while the compiler writes each use's actual
rectangle into the definition. Unknown members, duplicate sources, conflicting group
membership, unsafe paths and unknown configuration commands are build errors.

Exact mode refines the native layout using integer coordinates, fixed padded
dimensions, a two-dimensional no-overlap constraint and the `width * height`
objective. The native layout supplies an incumbent and hints. The `AtlasSettings`
API defaults remain `time_limit_seconds=None` and `require_optimal=True`: an
unlimited solve that requires proof of global minimum area. The checked-in
production configuration explicitly overrides those defaults with a **60-second
CP-SAT budget per atlas** and `require_optimal false`, as authorized after the
pea-family proof search remained unproven for more than three hours. This is a
solver budget per packing unit, not a 60-second limit for the entire resource
build or its decoding, native incumbent search and publication steps.

`solver exact` is mandatory in both policies. `require_optimal true` accepts only
`OPTIMAL` or mathematically valid `LOWER_BOUND_PROVEN` results, with the proven
bound equal to the published area. A finite strict solve that ends without proof
still fails the build. Explicit `require_optimal false` is valid only with a
positive finite budget of at most 86,400 seconds. When that budget expires,
publication may retain the best validated incumbent even if its status is
`FEASIBLE` or `UNKNOWN`; `optimal` remains false. An `UNKNOWN` result can retain
the geometrically validated native or checkpoint incumbent when CP-SAT has not
reported a new feasible solution. It is not a claim of minimum area.

Before the independent publication-policy check, the caller validates rectangle
orientation, dimensions, padding, bounds and non-overlap. The policy check also
requires current model/solver versions, a recognized status and consistent
metadata: `objective_area` equals the actual atlas area, the bound lies between
zero and that area, and `gap` honestly records `(area - bound) / area` (zero for
an empty atlas). Invalid status, a false proof marker, inconsistent numbers or
an unbounded opt-out cannot be published. A result marked optimal must satisfy
the proof checks even under the bounded policy.

Previous generated headers and packages remain in place until every atlas
passes its configured policy. Old unproven caches provide validated geometry
hints only, not proof. Proof cache keys include geometry, padding, maximum side,
model version and solver version, not image pixels or search budget. Reports
retain status, bound, gap and optimality rather than relabeling a timed-out
incumbent as optimal. Checkpoints, CP-SAT search logs and
`atlas-proof-progress.json` record offline progress; the progress heartbeat
records the active atlas and its strict or bounded policy. Python builds the
model and coordinates artifacts; the computational search runs in OR-Tools'
native C++ CP-SAT engine.

Bounded builds can reuse a validated final cache without repeating the same
search for every game-code rebuild or backend. A normal completed solve records
`search_complete: true` and `solve_budget_seconds`; an in-progress checkpoint
records `search_complete: false`. With `require_optimal false`, a completed
same-geometry/model/solver cache satisfies an equal or smaller finite budget.
Increasing the budget resumes a fresh solve using the cached geometry as its
incumbent. Strict `require_optimal true` always continues unproven results,
irrespective of prior bounded completion. Cache reuse keeps the original
status, bound, gap and `optimal: false`; it is not a new optimality proof.

Older v2 final caches do not contain a budget marker. Their elapsed time alone
does not establish completion. `exact_atlas.import_completed_budget(cache,
manifest)` can import evidence from the successfully published manifest: its
explicit finite bounded policy, entire final exact metadata, dimensions,
current geometry/model/solver key and intact cache must match. The published
elapsed time must also cover that budget. Explicit unfinished checkpoints are
never promoted. Copying a valid resource cache and its matching manifest to
another backend can therefore preserve completed search effort without
weakening geometric or publication checks.

Curated portable layout caches live in `assets/definitions/atlas-cache/`. They
are build inputs only: they contain rectangle geometry and solver metadata,
not source pixels, machine paths or runtime textures, and are not included in
the game's pak. `exact_atlas.export_completed_cache(cache, manifest,
destination)` selects only the current published groups' valid optimal proofs
or completed bounded searches. It excludes unrelated historical cache files
and unfinished checkpoints. Portable records keep the v2 format and add
padding/maximum-side geometry settings so their identity can be reconstructed.
Use a fresh destination for a clean export; unrelated pre-existing destination
files are not deleted automatically.

Before resource compilation, `exact_atlas.seed_completed_cache(source, cache)`
validates each curated record's digest, current model/solver version, filename
key, rectangle geometry and completion/proof metadata. It atomically fills only
missing local cache files; an existing local search or result is never replaced.
This avoids repeating completed 60-second searches on a clean checkout or a
different backend while retaining the same policy checks. Geometry, model or
OR-Tools version changes can invalidate a seed and require a new search;
strict mode still continues any seeded result that has no optimality proof.

The native `reanimc pack-atlas-layout` command optimizes rectangle layouts in
one batch for the complete definition corpus. It tries several deterministic
MaxRects placement heuristics, rectangle orders and candidate dimensions.
The input images have no semantic placement order. Batch-global candidates
score every remaining image against the available spaces at each step, while
fixed-order candidates remain alternative heuristics. Returned coordinates
are mapped back to the original local region indices.
Both coordinate-axis orientations are searched. A search with exchanged width
and height maps its coordinates back before emitting the layout; this explores
vertical filling without rotating any source image.
The previous shelf layout remains a candidate, so compact packing cannot
increase an individual atlas's area. Search is bounded and happens only during
resource compilation. Python applies the selected coordinates and pixel
extrusion; the game does no layout search.

## Numeric resource load plans

`compiled/resources.compiled` uses `NRSC` version 4. It describes temporary
per-group loading plans rather than a persistent table of mixed resource slots.
Its little-endian header contains eight `u32` values:

| Order | Value |
| --- | --- |
| 1–2 | Magic `0x4353524E`, version `4` |
| 3–4 | Image load-record count, group count |
| 5–6 | Total load-record count, named binding contract CRC32 |
| 7–8 | Payload CRC32, payload byte count |

Each group contains three `u32` values—group ID, record count, block byte
count—followed by its length-prefixed records in authoring order. Each record
starts with four `u32` values: kind (`0` image, `1` sound, `2` font), flags,
image cache variant, and a font reference. No field encodes an image identity.
Four length-prefixed paths follow: resource, alpha image, alpha grid and font
image. An image record ends with one `u32` alpha color; it contains no grid,
animation mode, delays or frame-map fields. Sound volume/pan or font
size/style/tags finish the other record kinds. A font reference is either
`0xFFFFFFFF` or a prior font's
record position in the same group; it is never a global ResourceId.

The current authoring inputs compile to 398 explicit targets in 29 groups: 210 images, 168
sounds and 20 fonts. `ResourceLoadBindings.inc` generates one typed load call
and a matching release call for every target; group release reverses load order.
Image calls pass both the named `Image*&` target and its named
`SharedImageRef&` owner. The generated contract header records the image count,
record count, binding CRC and payload CRC.

The binding contract CRC is CRC32 of compact ASCII JSON in the form
`[[group, [[kind, "targetName"], ...]], ...]`, with groups in canonical numeric
order and targets in their emitted step order. This binds package metadata to
the concrete C++ assignments without storing those target names in the package.
The payload CRC independently covers all serialized loading parameters.
There is no 879-slot kind table, image-to-ResourceId mapping or pointer-to-ID
registry. Historical NRSC1/2 fixtures require an explicit offline test route;
the production loader accepts NRSC4 only. Intermediate NRSC3 plans must be rebuilt.

The shared-image cache uses `(physical path, variant)`. The variant discriminates
complete loading recipes only within the same path, including alpha inputs,
automatic-alpha policy, color and image flags. Offline source grid layout is
not part of the runtime record or cache recipe. A path
with one recipe uses zero; identical recipes use the same key. This value has
no global image identity or drawing lookup meaning.

The manager parses pending plans once, preserves local font references at their
final addresses, and deletes each plan when loading completes. A failed load
releases the group's partially loaded targets before discarding its plan.
Images remain owned by named `SharedImageRef`s, fonts by their named
variables, and sounds by backend handles. Font duplicates share counted
`FontData`, whose layer images hold their own shared-cache references.
Title-screen early release clears the named image and its owning reference;
repeated release and later whole-program cleanup are safe. Caption font binding
loads the credits group and assigns `FONT_BRIANNETOD32` through a direct helper,
without a runtime font ID lookup. A font sidecar checksum remains solely in
offline DSL compilation and historical record validation.
These are startup load plans, not a runtime resource eviction/reload protocol.

## Compiled records

All integers are little endian. The common envelope is three `u32` values:

| Offset | Value |
| --- | --- |
| `0x00` | Reanimation v6 cookie `0xDEADFEDB`, particle v6 cookie `0xDEADFEDC` |
| `0x04` | Payload length in bytes |
| `0x08` | Payload CRC32 |

Both payloads put shared behavior before appearance arrays:

| Order | Representation |
| --- | --- |
| Header | Two `u32`: kind-specific magic, behavior byte count |
| Behavior | Exactly that many bytes, decoded once per root |
| Appearance tail | Kind-specific counts and flat arrays |

The reanimation payload magic is `0x37564152` (`RAV7`). Its shared `IDT6`
body starts with six 32-bit words: magic `0x36544449`, track-table hash,
image hash (zero), font-table hash, FPS float bits, and track count.
Phase 1 retains each track's ID, frame count, transform stride 32 and text mode,
then eight floats per frame. Phase 2 stores a `u32` image slot and `u32` font ID
per frame, plus length-prefixed caption bytes when text mode is 1. The numeric
attacher tail is unchanged. Six-word source sheet rectangles are not shipped
in shared reanimation transforms.

After the `IDT6` behavior bytes, the reanimation appearance tail contains:

| Order | Representation |
| --- | --- |
| Counts | Three `u32`: variant count, image-slot count, action count |
| Atlas dimensions | Variant count × two `u32`: width, height |
| Action keys | Action count × two `u32`: track ID, option |
| Transition targets | Variant count × action count `u32` variant indices |
| Image rectangles | Variant count × slot count × four `i32`: x, y, width, height |

Action keys are unique and sorted by `(trackId, option)`. Transition targets
must be within the variant table and must preserve unrelated choices. Dimensions
are zero exactly for an image-free row; zero rectangles denote blank slots.
The loader validates counts, bounds, default geometry, actions, transitions and
the complete record length.

The particle payload magic is `0x36564150` (`PAV6`). Its shared `IDR6`
behavior begins with five `u32` words: magic `0x36524449`, rectangles per
appearance row, emitter-name table hash, emitter count, and emitter-body byte
count. Each shared emitter contains:

| Field | Representation |
| --- | --- |
| Rectangle range | Two `u32`: offset within each appearance row, count |
| Logical imageFrames, animated, flags, emitter type | Four 32-bit values |
| System tracks, particle/system fields, particle tracks | Existing compact layout |

The existing IDT3 duration-target and IDT5 emitter-name sections finish the
shared behavior. The particle appearance tail then contains:

| Order | Representation |
| --- | --- |
| Counts | Two `u32`: variant count, rectangles per row |
| Atlas dimensions | Variant count × two `u32`: width, height |
| Image rectangles | Variant count × rectangles per row × four `i32`: x, y, width, height |

The rectangle count must agree with the behavior header. Emitter ranges are
consecutive and cover each complete row. Counts, dimensions, rectangle bounds,
behavior byte length, CRC and the complete record ending are validated.
The same behavior scalar/track/field allocations serve every appearance.
No runtime parent path, image symbol ID, source handle, rows or columns are
stored in these final particle records.

Packing does not rotate images. Two pixels of replicated edge padding surround
each packed image; padding is outside its rectangle. Old cookies and the
IDP5/IDR5/IDV1 runtime particle representations are rejected; executable and
package must be rebuilt together. Save conversion is independent and uses the
explicit DAT64 v4 schema.

Compiler-only `.source-record` files use `IDR4`: each emitter contains an
ordered source-handle list instead of rectangles, and the second header word
is zero. These handles address `image_sources.txt`, never runtime image bindings.
The packer replaces every handle with its final rectangle and fills the total
rectangle count, groups appearances under each root, and writes shared behavior
followed by the appearance tail. Intermediate files are not shipped as runtime
definitions. Compilation rejects invalid parent paths, missing parents and cycles.

These resource CRCs detect broken build artifacts. They are independent of DAT64
save files, whose former content CRC slot is ignored to permit manual editing.

## Ownership and rendering

Particle and reanimation definitions retain arrays of borrowed GPU texture
pointers; their shared transforms contain only image-slot indices. Reanimation
sampling rows are static `TextureRegion` arrays, and instances own only a variant
index. Particle definitions use a contiguous `[variant][region]` array; each
emitter stores an offset/count into every row. Individual particles have no
texture pointer or sampling-region allocation. Emitters, tracks, fields and
name-routing data are compiled once per root. Reanimation data lives with the
program; loaded particle data lives until `TodParticleFreeDefinitions`.
There are no separately loaded child definitions or runtime parent chains.
Failed texture binding clears partial published bindings and temporary payloads
before rethrowing, allowing a clean retry; repeated cleanup is safe.
Particle simulation, drawing-track sampling, image-frame selection, crossfade, clipping and transforms are computed on the CPU. TodTriangleGroup batches the final geometry; the renderer receives triangles and textures, without reading particle behavior from GPU-resident definitions. Temporary GPU experiment packet formats are not part of this contract.

## Global image resources

Definition image fields use precompiled UV regions and direct GPU texture pointers.
Named image globals retain the 210 images needed directly by game code and UI;
font glyph paths retain their own image closure. `Resources.h/.cpp` declare and
define those ordinary `Image*` variables. Reanimation and particle compilers
resolve author labels independently and do not generate a global image registry.
The image class does not require a symbol registration.
Definition-only image descriptors and their standalone physical files are
excluded from the package; shared direct consumers protect their resources.
Existing authoring IDs can remain as offline identities without binding runtime
textures. Historical save identities belong to converter-only metadata and do
not allocate runtime texture slots.

All twelve QuickPlay source images remain in the source tree. They are excluded
from current UI code, generated bindings, load descriptors and runtime packages;
source preservation alone is not a package retention rule.

Atlas memory and duplicate pixels across definitions need measurement. Fewer
texture bindings do not by themselves establish a Vulkan performance gain.

## Whole-sheet scene atlases

`scene_atlas.py` compiles the authored Magnet, Almanac and Achievements DSLs
and one built-in FixedUI source list. The modules contain respectively 21/19,
19/19, 35/16 and 24/24 logical views/full source sheets: 99 views over 78 sheets
in total. FixedUI's list is explicit Python data: six stone-button images,
thirteen dialog pieces and five seed-chooser button states.

The packing unit is always the entire decoded source sheet at its original
pixel dimensions. It is not trimmed, split into frames, rotated or rearranged
internally. Unreferenced parts of a sheet remain present. Multiple views of the
same sheet use one packed copy. A local rectangle `[x,y,w,h]` becomes
`[sheetOriginX+x,sheetOriginY+y,w,h]`; no view image or new source PNG is created.
Two-pixel edge extrusion applies outside the sheet, without changing its interior.
The existing author's alpha composition policy is preserved before packing.

Generated `SceneAtlasData.h/.cpp` defines the global namespaces `MagnetAtlas`,
`AlmanacAtlas`, `AchievementsAtlas` and `FixedUIAtlas`. Each has one
`Sexy::GpuTexture* Texture`, a module-local `Sprite` enum, static `TextureRegion`
`Images` arrays and `Load()` / `Free()` functions. Public atlas Width/Height
constants are removed. The load
functions bind the final GPU field through `DefinitionBindAtlasPath`; freeing
unbinds it. `LoadSceneAtlases()` and reverse-order `FreeSceneAtlases()` aggregate
the four modules. There is no per-view `Image` object, texture wrapper or
global resource identity.

Magnet emits its authored non-unit `Scales` array. Achievements additionally
emits `IconCount` and `Icons[IconCount]`, sorted by the numeric suffix of
`ICON_0` through `ICON_N`. The indices must be unique and contiguous from zero;
DSL declaration order may vary. Consumers index this explicit array rather than
assuming that icon entries occupy consecutive enum values.

Scene source selection uses `sources(scene "ui/*.atlas.dsl")`; atlas members use
`scene:Magnet`, `scene:Almanac` and `scene:Achievements`. FixedUI is a built-in
packing unit. These use the existing exact solver, native incumbent, geometry
cache, no-rotation rule, two-pixel padding and 60-second budget per unit. Reports
retain proof status and gap; a bounded feasible result is not a proof of optimality.
`SceneAtlasBindings.h` stores offline extent and full-sheet RGBA verification
metadata. The offline generator normalizes view coordinates using that extent
and writes UV bounds plus logical dimensions into static C++ data. Loaders use
expected PNG dimensions as validation literals only. There is no runtime scene `.compiled`.

An `image` may be a direct source path, including a newly added file with no
`resources.json` entry. Existing `IMAGE_*` labels are optional offline authoring
aliases; neither form creates a runtime image ID. See
[scene-atlas-dsl.md](scene-atlas-dsl.md) for syntax and examples.
