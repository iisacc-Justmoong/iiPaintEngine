# Brush interoperability, symmetry and stabilization (0.5)

This release strengthens the existing C++23 bitmap brush pipeline in three areas:
ABR tip import, current-stamp symmetry and causal pulled-string stabilization.
The existing pressure/tilt/rotation response curves, ordered bindings, textures,
dual tips, bristles, wet/mixer models, airbrush and scalar tip wear remain in use.
No Qt, Rust implementation, scripting runtime or new third-party library is added.

The current Adobe/Photoshop checkout is PhotoCraft, an independent Rust
reconstruction at commit d95fdf3181c01ad41e95456091a394b105a9afb2. It is a design and
format reference, not recovered Adobe native source. References and source hashes
are recorded in [brush-expansion-provenance.json](brush-expansion-provenance.json).
Public behavior references are Adobe's [symmetry documentation](https://helpx.adobe.com/photoshop/desktop/apply-painting-techniques/create-fill-with-patterns/paint-symmetrical-patterns.html)
and [stroke smoothing documentation](https://helpx.adobe.com/photoshop/desktop/repair-retouch/clean-restore-images/create-smoother-more-polished-brush-strokes-with-stroke-smoothing.html).
The algorithms here are independent SDK models; no Adobe pixel-parity claim is made.

## ABR import

The Brush module exposes a memory-only, atomic decoder. File access stays with
the caller and iiFileProvider; the decoder does not depend on Stroke/Application.

```cpp
#include <iiPaintEngine>

// bytes is a borrowed span over an ABR file read by the application.
auto result = importAbrBrushes(bytes);
if (!result) {
    // result.error(): typed code, absolute byte offset and diagnostic message.
    return;
}
for (const auto &entry : result->brushes) {
    // Inspect result->warnings and entry.completeSettings before treating
    // a sampled tip as a full Photoshop brush preset.
    auto resolved = resolveBrushPreset(entry.preset, 0xFF336699U, 1234);
    if (resolved.brush) { /* select entry.preset or render immediately */ }
}
```

| Layout | Imported content | Boundaries |
| --- | --- | --- |
| ABR v1/v2 | Computed tip diameter/hardness/roundness/angle/spacing; sampled mask; v2 sampled name | Legacy miscellaneous flags are not modern dynamics; anti-alias-off is reported as unsupported |
| ABR v6/v7/v9/v10, subversion 1/2 | Every supported sampled tip and its source ID | Modern descriptors, preset names/associations, patterns, folders and tool settings are not decoded |
| Sample encoding | 8/16-bit, raw and PackBits including no-op, literal and repeat packets | 16-bit coverage is rounded to the engine's 8-bit mask and produces a warning |

Modern imports always set completeSettings to false and return a warning. They
produce usable neutral sampled-tip presets; they do not restore a complete
Photoshop brush. Unsupported sections are named in warnings. A known record with
bad lengths, invalid geometry, malformed RLE or unsupported depth/compression fails
the whole operation. Unknown legacy brush types may be skipped with a warning;
a collection with no usable tips fails. No partial result mutates a brush library.

Default limits are 256 MiB encoded input, 4096 brushes and 64 MiB total decoded
8-bit masks. Individual tips respect the existing 4096-pixel edge and 16 MiB mask
limits. Callers may tighten AbrImportLimits; hard bounds cap encoded/decoded data
at 256 MiB, brush count at 20000, name length at 4096 UTF-16 units / 8192 UTF-8
bytes and modern sections at 4096. Counts are checked before dangerous
allocation/arithmetic. Import IDs are stable, non-cryptographic local identifiers;
sourceId retains a modern sample ID separately. ABR writing is not introduced.

## Current-stamp symmetry

```cpp
BrushPreset brush;
brush.stroke.symmetry.mode = BrushSymmetryMode::Mandala;
brush.stroke.symmetry.center = {512, 512};
brush.stroke.symmetry.segments = 16;
brush.stroke.symmetry.angleRadians = 0.2;
```

Modes are None, Vertical, Horizontal, DualAxis, Radial and Mandala. Center is in
document pixels. Angle is radians and rotates reflection axes; the original stamp
is always retained. Radial repeats each stamp 2–64 times. Mandala reflects and
rotates each stamp through 2–64 sectors, producing up to 128 copies. Both remain
within the common 65536-stamp per-event budget. Settings also apply to valid
bristle, erodible and airbrush bitmap models.

Each original scatter mark is expanded after its random/dynamics resolution.
Replicas inherit the seed, color, Fade index, pressure-derived scales and wear.
They never consume extra random numbers or advance the brush's physical/input
state. Reflection changes the asymmetric primary tip orientation and tip-local
texture/dual-mask coordinates. Document/paper textures remain anchored to their
configured document coordinates. Equal centers within one stamp's expansion are
emitted once, with the first orientation retained, to avoid repeated center/axis
deposits. Other overlapping branches retain ordinary flow accumulation.

applyBrushSymmetry() is also available for current BrushDabSpan values.
appendRasterDabs() invokes it automatically. Bounds, clipping, source sampling,
preview, commit, cancel and raster undo/redo consume the expanded stamps through
the existing pipeline. Invalid transformations or an exceeded budget reject the
event without changing RasterDabStream. No paths or coordinate histories are stored.

## Pulled-string stabilization

```cpp
brush.stroke.smoothingEnabled = true;
brush.stroke.smoothing = 1.0;
brush.stroke.smoothingMode = BrushSmoothingMode::PulledString;
brush.stroke.smoothingRadius = 8.0;
brush.stroke.smoothingCatchUp = true;
brush.stroke.smoothingCatchUpRate = 12.0;
brush.stroke.smoothingCatchUpOnEnd = true;
// During a pause, the application calls session.advanceTime(monotonicSeconds).
```

Exponential preserves the previous causal filter and remains the default.
PulledString holds the current painted anchor within a document-space radius of
the pointer. Effective radius is smoothingRadius * smoothing; when the pointer
leaves that circle, the anchor advances by the excess distance. Radius accepts
0–4096 pixels. Straight-line endpoint placement does not depend on subdividing
the same movement into more pointer events.

When smoothingCatchUp is enabled and the raw pointer target is unchanged,
remaining lag decays by exp(-rate * elapsedSeconds). Rate accepts 0.01–1000 per
second. Splitting a stationary time interval preserves the final anchor position.
BitmapPaintSession::advanceTime() supports this mode as well as timed airbrush;
the engine creates no timer. Airbrush keeps its existing timed placement policy.
smoothingCatchUpOnEnd flushes the final pointer position before release commits.
Disabling it preserves the filtered endpoint. Amount zero disables filtering.
Velocity/time-driven dynamics can still differ when event timing changes.

The stream retains one previous filtered point and one current raw target.
It never retains a point list, future input or replayable trajectory. Radius is
document-space only; screen/zoom-dependent smoothing remains application policy.

## Storage, ABI and validation

Preset format 4 stores all symmetry/stabilization settings. Formats 1/2/3 remain
readable with neutral symmetry and legacy smoothing defaults. Snapshot/restore
and document format 3 brushSources preserve the same values; the historical
presetV2 field name stays unchanged. Older readers reject preset format 4.
New enum values, ranges and nonfinite settings are validated even when disabled.

Public struct layouts changed in ABI 0.5; the current package version is 0.5.1:
rebuild consumers. The concurrent package-release edit to 0.5.1 was preserved.
This task stages an installed-package consumer under build/; it does not replace
the active ~/.local installation or redeploy consumer applications.

iiPaintEngine requires the existing Qt-free iiFileProviderNative >=0.6 package.
Use CMAKE_PREFIX_PATH or iiFileProviderNative_DIR when the default installation is
older. The local run used the already-staged Native 0.8.0 package; provider source
and the host installation were not changed.

The new contracts test ABR versions/encoding/budgets/truncation/mutations; geometry
and asymmetric pixel reflection; 64-sector physical-tip/time/Fade behavior;
event rejection; live preview/cancel/undo; endpoint/clock stabilization and
preset/snapshot/document compatibility. The final validation record is
[BRUSH_EXPANSION_VALIDATION.md](BRUSH_EXPANSION_VALIDATION.md).

The C++ gallery exercises 16-fold radial, 12-sector mandala and stabilized
dual-axis strokes:

```sh
build/iiPaintEngineBrushExpansionGallery build/brush-expansion-gallery.png
```

## Remaining work toward a Photoshop-class superset

- Map modern ABR descriptors, sampled-tip references, embedded patterns, names,
  groups and tool-capture semantics; add explicit loss reporting/export policy.
- Add independently validated high-precision/color-managed paint surfaces.
  Current pixels and imported coverage remain 8-bit.
- Improve the existing wet/bristle/erodible models beyond their documented bitmap
  approximations, with measured performance and real pen/physical-tip evidence.
- Validate real, licensed user ABR collections and original-application outputs.
  The current suite uses original synthetic files and model invariants.
- Measure large-brush latency and validate Windows/Linux/mobile/tablet runtimes.
  Host tests and a wider symmetry range alone do not establish full Photoshop
  behavior, performance or ecosystem compatibility.
