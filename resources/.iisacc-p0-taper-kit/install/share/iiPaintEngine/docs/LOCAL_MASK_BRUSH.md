# Local correction mask brush

iiPaintEngine 0.4.2 adds `PixelMask`, `LocalMaskBrushOptions` and `MaskPaintSession`.
Applications supply document-pixel pointer events and receive normalized float
mask pixels. A separate adjustment library can borrow the row-major coverage
without either SDK linking to the other.

## Evidence and implementation boundary

The Lightroom 9.6 investigation's
`Engineering/Projects/Adobe/Lightroom/docs/Scripts.md` records local brush
declarations. Its `recovered/scripts/decompiled/Contents/PlugIns/Develop.lrmodule/Contents/Resources/DevelopLocalizedCommon.lua:7136–7184`
contains normalized size, flow, density, stroke opacity and feather constants.
`.../PlugIns/organize.lrmodule/Contents/Resources/EditParameters2.lua:12621–12767`
declares their preference keys and percentage ranges, and disables density in
subtract mode. AutoMask is declared at 10073–10108. These are decompiler output;
their high-level execution semantics were not verified. No Adobe native pixel
algorithm is executed by this API.

Adobe's [Lightroom desktop masking documentation](https://helpx.adobe.com/lightroom/desktop/edit-photos/masking.html)
(checked 2026-10-14) describes diameter in pixels, feathered edges, application
rate, stroke transparency and similar-color AutoMask. Lua's normalized size
does not establish Adobe's actual pixel conversion. The formulas, defaults and
color model below are iiPaintEngine decisions. Existing Photoshop brush options
remain in [BRUSH_OPTIONS.md](BRUSH_OPTIONS.md). The separately recovered numeric
helpers in [LOCALIZED_BRUSH_MATH.md](LOCALIZED_BRUSH_MATH.md) are not claimed to
be Adobe's native dab pixel kernel and do not silently replace this feather model.
The numeric [original vectors](localized-brush-vectors.json) and
[provenance](localized-brush-math-provenance.json) are installed passive documentation;
the C++ test consumes the separate header fixture.

## Public API

```cpp
#include <iiPaintEngine>

MaskPaintSession session;
session.createMask(640, 480);
LocalMaskBrushOptions options;
options.sizePixels = 24;
options.feather = .75;
options.flow = .25;
options.density = .8;
options.strokeOpacity = .5;
session.setBrushOptions(options);

PointerEvent event;
event.phase = PointerEventPhase::Press;
event.button = PointerButton::Primary;
event.primaryButtonDown = true;
event.documentPosition = {320, 240};
session.processPointerEvent(event);
const PixelMask& preview = session.previewMask();

event.phase = PointerEventPhase::Release;
event.primaryButtonDown = false;
session.processPointerEvent(event);
const PixelMask& committed = session.mask();
```

`PixelMask` has `width`, `height` and `std::vector<float> coverage`, one finite
[0,1] value per pixel in row-major order. Pixel centers are integer document
positions. Dimensions must be positive and no greater than 16384 per axis;
total pixels must not exceed 64 Mi. `replaceMask` validates all coverage and
resets guidance, pixel history and unfinished input. Failed create/replace
preserves the previous session. Core mask factories throw on invalid input;
session mutation methods return bool and explain failure through `lastError()`.

| Option | Default | Domain and effect |
|---|---:|---|
| `sizePixels` | 24 | Diameter [0.01,4096] document pixels |
| `feather` | 1 | [0,1], radial edge transition fraction |
| `flow` | 1 | [0,1], per-dab rate; zero is inert |
| `density` | 1 | [0,1], absolute Add coverage ceiling |
| `strokeOpacity` | 1 | [0,1], entire stroke's maximum contribution |
| `pressureAffectsSize` | false | Reuse existing pressure-to-size dynamics |
| `pressureAffectsOpacity` | false | Reuse per-dab pressure opacity ceiling |
| `autoMask` | false | Enable fixed start-color guidance |
| `autoMaskTolerance` | .15 | [0,1], normalized RGB RMS distance threshold |
| `mode` | Add | Add or Erase; eraser pointer forces Erase for its stroke |

Invalid enum, nonfinite and out-of-domain options are rejected without replacing
selected settings. Selecting options during a stroke affects the next stroke;
active options and sampled color remain frozen. Default flow 1 is an SDK choice:
recovered common and panel declarations differ (1 and .5), so no effective
Lightroom UI default is claimed.

## Float pixel model

The session resolves a procedural round `BrushPreset` and reuses
`InputStrokeBuilder`, `RasterDabStream`, `BrushDab` placement and dab device-pixel
bounds. Its preset placement density is always 1. **`BrushPreset::density`
retains its existing dab-placement meaning** and serialization.

For normalized radius distance `r`, the edge coverage is
`clamp((1-r)*radius + .5,0,1)`. Feather 0 keeps this antialiased hard edge;
positive feather multiplies by `clamp((1-r)/feather,0,1)`. This follows the
existing procedural round-tip geometry and half-pixel edge model while keeping
float precision. Subpixel areas are approximate, not analytic area integrals.
Only pixels intersecting the mask are visited. Flow is not quantized into ARGB8.

For one dab, `a = flow * geometricCoverage * guidanceWeight`. Pending stroke
coverage accumulates as `P + (1-P)*a`, limited by the dab's opacity ceiling.
Accumulation never decreases an existing `P` when a later dab has a lower
opacity ceiling. With pressure opacity disabled, the ceiling is 1.

Let `B` be committed coverage, `D` density and `O` stroke opacity:

- Add preview is `B + max(0,D-B)*(O*P)`.
- Erase preview is `B*(1-O*P)`; density is ignored in Erase mode.

Add cannot exceed `max(B,D)` or reduce a pixel above a newly selected lower
density. Repeated dabs within a stroke cannot bypass stroke opacity. A later
stroke uses the new committed `B` and can progress further toward density.
From zero, flow 1/density .6/stroke opacity .25 gives .15 on the first stroke
and .2625 on the next; repeated passes in the first stroke stay at .15.
Erase actually decreases float coverage. An unchanged position emits no extra
distance-based dab; the API creates no timer or input queue. Existing per-event
65,536-dab limits reject excessive input before mask pixels change.

Optional pressure switches reuse `BrushDynamics::pressureToSize` and
`pressureToOpacity`, each set to 1 when enabled. They do not change the existing
dynamics model. Mouse input uses neutral pressure 1; synthetic tablet input
supplies its measured pressure, normalized by existing dynamics. Tablet tests
compare pressure .25/1 at the same configured diameter, test actual coverage
and footprint changes, and ensure later lighter contact cannot reduce prior
Add coverage. Disabled switches are neutral. Hover and touch follow the
existing input builder's non-painting policy. Cancel always discards pending
pixels. No physical tablet or Adobe pressure-response parity is claimed.

## AutoMask and source guidance

`setColorGuidance(std::span<const std::uint32_t>)` copies straight ARGB32 source
pixels with the mask's exact pixel count. Caller storage may be released after
the call. Replacing/clearing guidance while a stroke is active is rejected.
AutoMask requires a start inside the source on a nonzero-alpha pixel and
captures that nearest integer-center RGB once. Later movement keeps it fixed;
the next stroke samples a new reference.

For each touched source pixel, normalized RGB RMS distance `d` is compared with
tolerance `t`. Positive `t` uses `1-smoothstep(clamp(d/t,0,1))`, multiplied by
source alpha. Tolerance zero accepts exact RGB only. Alpha-zero source pixels
always contribute zero; hidden RGB cannot seed or enter an automatic mask.

This is a soft color-edge restriction of actual brushed pixels. It does not
segment objects, flood-fill connected regions, recognize subjects, run AI,
transform ICC profiles or use a perceptually uniform color space. Similar
disconnected colors can be painted when touched. Image noise, lighting and
supplied color space affect selection. Callers should supply a stable original
image rather than a changing correction preview. Adobe AutoMask pixel parity
has not been established.

## Pixel lifecycle and history

Every accepted event synchronously updates touched pending/preview pixels;
`mask()` remains committed until release or `commitStroke()`. Release at an
unchanged position commits exactly the preview. Release at a new position
first paints the final segment. Empty/unchanged strokes create no history.
Cancel restores committed pixels to preview.

Undo/redo exchange dirty-rectangle float pixel patches. Clear is undoable;
new commits discard redo. Undo/redo/clear discard unfinished input. The input
builder owns one active flag, and the existing stream owns one preceding point
plus scalar placement state. Guidance, pending state and history contain
pixels; there is no point list, pointer event history or replayable stroke.
Invalid finite/range/timestamp/dab-budget input preserves existing mask pixels.

The session owns three mask planes, an optional ARGB guidance plane and
application-lifetime pixel history. Callers must budget large masks/history;
there is no automatic backing store, history eviction or concurrent-mutation
contract. Borrowed coverage spans remain valid only while pixel storage is
valid and the caller synchronizes access. The engine owns no window, render
thread, clock, external adjustment SDK or user image file.

## Tests and installation

`MaskPaintSessionContract` checks actual diameter/feather pixels, low-flow
float precision, repeated accumulation, independent caps, stroke reentry,
erasure, preview/commit/cancel, pixel history, option snapshots, pressure and
failure preservation. `MaskAutoMaskContract` checks contrasting edges, reference
freezing/resampling, source-copy lifetime, tolerance, partial/zero alpha and
missing/invalid guidance. Public umbrella and staged install consumer tests
compile and execute the new API. Full Release and ASan/UBSan suites run under
`build/`; install tests write only to `build/` staging prefixes. Host fixtures
do not establish physical pen input or Lightroom runtime parity.

On macOS the shared target explicitly uses `@rpath/libiiPaintEngine.dylib` for
both build and install IDs. An old cache's absolute install-name directory must
not bind a newly linked consumer to an earlier installed SDK. The staged
installation contract verifies this actual ID before compiling the consumer.
