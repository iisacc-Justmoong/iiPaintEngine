# Brush options contract (0.4.0)

`BrushPreset` remains the editable brush object. Its `dynamics`, `material`,
`shape`, `stroke`, and `color` members carry these additions through validation,
runtime resolution, preset storage, snapshots, and document `brushSources`.
The new fields are optional effects with neutral defaults. They use C++ values
and standard-library storage; the Brush and Stroke implementation adds no Qt
dependency.

## Evidence and interpretation

This change follows the recovered Photoshop **27.10.0** snapshot in
`Engineering/Projects/PhotoshopReverse`. The interface investigation rechecked
2,414 original file hashes and inventoried 61 relevant files, 718 control
declarations, 157 main/group/tool/preset observations and 51 related-tool
observations. The JavaScript investigation searched all 11,300 recovered files
(429,149,893 bytes), verified their manifest hashes, and directly reviewed 18
key application/preset sources. These are bounded searches of the recovered
corpus, not recovery of the native Photoshop brush renderer.

The principal `Required/layouts/Painting/Panels/brushStylerContents-4181.exv`
has SHA-256 `24dd2fbaeab85033063b7705a7343b150570c67dbfd0a9058359e9c6b38ce070`.
The source manifest SHA-256 is
`550754a55ada18c9d74b7ffd09ac1c7a5bdde3e98b333b2d69427146a73f192d`.
The machine-readable records and complete search limits are retained in
`PhotoshopReverse/evidence/brushes/{interface,javascript}-findings.json`;
the corresponding reports are `docs/BrushInterface.md` and
`docs/BrushJavascript.md` in that project.

| Observed family / main EXV lines | iiPaintEngine object contract |
|---|---|
| Ordinary tip, 122–168 | Existing `tip`, `shape`, size, hardness and stroke spacing |
| Bristle, 175–219 | Existing count/length/stiffness/shape, added thickness and angle |
| Erodible, 226–243 | Added `material.erodible`, scalar wear and `sharpenErodibleTip`; existing `shape` supplies the 2D footprint |
| Airbrush tip, 250–297 | Added `material.airbrushTip`; existing hardness and spacing apply |
| Shape dynamics, 308–390 | Named size/angle/roundness variations, flip jitter and tip projection; existing `tiltToEllipse`; initial stroke direction |
| Scattering, 396–436 | Existing scatter geometry; named amount/count variation controls |
| Texture, 446–521 | Invert, brightness, contrast, each-tip, mode, depth/floor and named depth variation |
| Dual tip, 529–572 | Existing mask/scale/spacing/modes; secondary flip and scatter/count, more blend modes |
| Color, 582–634 | Existing HSV/mix/per-stroke settings; mix jitter, purity and named foreground/background variation |
| Transfer, 643–741 | Named opacity/flow/wetness/mix variations and a Mix binding target |
| Pose, 752–793 | Explicit pressure/tilt X/tilt Y/rotation overrides |
| Global effects, 88–109 | Noise, wet edges, existing time-based airbrush, causal smoothing, Protect Texture |
| Mixer toolbar, `mixerBrush-1383.exv:36–44` | Existing wetness/mix/pickup/deposit; added reservoir load |

UI IDs such as `szjt`, `fpop` and `step` are container-local view identifiers.
Menu resources 805/806 and 1351–1363 do not identify serialized enum values.
All public enums, defaults, normalized units and formulas below are
**iiPaintEngine implementation decisions**. They are not recovered Adobe C++
source or proof of Adobe pixel parity. Adobe's public descriptions corroborate
the concepts: [dynamic brush controls](https://helpx.adobe.com/photoshop/using/adding-dynamic-elements-brushes.html),
[texture and dual tips](https://helpx.adobe.com/photoshop/desktop/apply-painting-techniques/brushes-presets/textured-and-dual-brush-settings.html),
and [CS6 tip/pose descriptions](https://helpx.adobe.com/pdf/cs6/photoshop_reference.pdf).

Libraries `PHXS_UXP/js/brushUtils.js:24–59` and `PHXS/jsx/brush.jsx:87–132`
read existing current-tool options and replace selected tool/flow/opacity/mode/
foreground-color metadata. Native `applyBrushFile` and `save` handle ABR files;
the recovered scripts contain no full ABR codec. `PHXS/js/push.js:78–89` carries
all metadata keys without providing a parameter schema. The scripts' truthy
flow/opacity checks skip zero; the SDK continues to accept a valid explicit
zero. This change does not introduce an ABR importer.

## Named variations

`BrushDynamicsVariation` is a repeated contract for these eleven members of
`BrushDynamics`:

`sizeVariation`, `angleVariation`, `roundnessVariation`, `scatterVariation`,
`scatterCountVariation`, `textureDepthVariation`, `colorMixVariation`,
`opacityVariation`, `flowVariation`, `wetnessVariation`, `mixVariation`.

| Field | Default | Supported domain |
|---|---|---|
| `enabled` | false | bool |
| `control` | Off | Off, Fade, PenPressure, PenTilt, StylusWheel, StylusRotation, InitialDirection, Direction |
| `jitter` | 0 | [0,1] |
| `minimum` | 0 | [0,1], floor for scalar variations |
| `fadeSteps` | 25 | integer [1,9999], emitted marks |

The order is pose substitution, legacy property responses, named variations,
then existing ordered `bindings`. A binding can deliberately replace the result
of a named variation. Size, roundness, scatter, count, depth, opacity, flow,
wetness and mixing variations multiply their existing scale result.

For a scale control, `q` is 1 for Off, `max(0,1-dabIndex/fadeSteps)` for Fade,
pressure or tilt magnitude for pen controls, `(tangentialPressure+1)/2` for the
wheel, or the wrapped fraction of one turn for rotation/direction controls.
The factor is `minimum + (1-minimum)*q*(1-jitter*random)`. Pen input disable
flags substitute the neutral control value. Random input disable suppresses
jitter and flip jitter. Independent unsigned random channels are derived from
the seed-backed input and mark index; repeated input yields repeated results.

Angle control produces a radian offset. Fade advances from zero to one full
turn, pen controls cover a turn, and direction/rotation controls provide the
actual angle. Its jitter is symmetric about zero; `minimum` has no angular
meaning. Foreground/background control adds a normalized color mix; Fade moves
from foreground to secondary color, and Off contributes only its optional
jitter/floor. No Photoshop numeric menu mapping is implied.

`BrushDynamicsInput::dabIndex` counts emitted marks from the start, including
scattered marks and time-injected marks. `RasterDabStream` supplies it. Direct
callers of `resolveBrushDynamics` may set it explicitly.

`shape.angleMode = InitialStrokeDirection` keeps the first nonzero movement
angle for later marks. The initial press has angle zero because future input
is unavailable. `initialDirectionRadians` is also supplied to angle controls.
Only a scalar angle and a flag are retained. `flipXJitter` and `flipYJitter`
toggle each mark's existing reflection independently. `projectTip` compresses
the 2D footprint by stylus tilt; it is a bitmap projection approximation.

## Pose and color

`dynamics.pose` defaults to disabled. It contains four independent override
flags and values: `pressure` [0,1], `tiltX`/`tiltY` [-1,1], and
`rotationRadians` [-1024,1024]. Defaults are pressure 1 and other channels 0.
Only selected channels are substituted; all subsequent mappings and tip/angle
selection consume the same substituted inputs. Sensor disable flags still
apply. `applyBrushPose()` exposes the same value transformation to callers.

`color.mixJitter` is [0,1], default 0, and adds a random secondary-color mixture.
`color.purity` is [-1,1], default 0; negative purity reduces saturation toward
zero and positive purity raises it toward one. These apply only with
`color.enabled`. Existing `perStroke` now freezes the resolved color once per
stroke, including input-driven mix changes. It retains one ARGB value, not an
input history. Setting it false applies color changes per mark.

## Texture and secondary brush

Both `material.texture` and `paperGrain` gain these fields:

| Field | Default | Domain / effect |
|---|---|---|
| `invert` | false | Reverse adjusted pattern alpha |
| `brightness` | 0 | [-1,1], additive normalized tone |
| `contrast` | 0 | [-1,1], contrast multiplier `1+contrast` around 0.5 |
| `depth` | 1 | [0,1], zero hides pattern modulation |
| `minimumDepth` | 0 | [0,1], floor of effective depth |
| `textureEachTip` | false | True selects tip-local texture coordinates and depth dynamics |
| `blendMode` | Multiply | Multiply, Subtract, Darken, Lighten, ColorDodge, ColorBurn, LinearBurn, HardMix, Height, LinearHeight |

Brightness/contrast are clamped before inversion. The selected scalar blend
combines brush coverage and pattern alpha; `strength * effectiveDepth` mixes
that result with original coverage. Multiply uses the existing multiplication
formula. Height is the minimum of coverage and texture; LinearHeight adds
texture height about 0.5 and clamps. These are explicit SDK scalar models,
not native Photoshop height compositing. Existing grain/asset-cache options
remain available. When `textureEachTip` is false, the existing `space` field
remains authoritative. Pattern indices wrap before integer conversion, even
for large valid offsets.

`material.dualBrush` gains `flipX`, `flipY` (false) and its own `BrushScatter`.
Scatter has the same radius/count/axes/distribution/jitter/relative-size
contract as primary scatter, defaults to disabled, and affects only the
secondary mask. Its deterministic offsets change at secondary `spacingRatio`
intervals; count combines offset secondary mask samples by maximum coverage.
It does not add primary marks or retain a second input path. Projection scale
converts the secondary mask and offsets consistently to device pixels.
Additional dual blend modes are Darken, Lighten, ColorDodge, ColorBurn,
LinearBurn and HardMix. Existing enum values retain their numeric order.

## Bristle, erodible, airbrush and mixing bitmap models

`material.bristle.thickness` defaults to 1 and accepts [0.001,1]. It scales
the existing 2D footprint width. `angleRadians` defaults to 0 and accepts
[-1024,1024]. Existing count, length, stiffness and shape remain in use.

`material.erodible` defaults to disabled, softness 0.5 and wear 0. Softness
and wear are [0,1]. For a mark at accumulated distance `d`, wear is
`clamp(initialWear + softness*d/(max(1,brushSize)*100),0,1)`. The footprint
widens by `1+wear/2` and hardness decreases by `1-0.8*wear`. Its shape remains
the existing procedural/image tip. `BrushDab::tipWear` and
`RasterDabStream::tipWear` expose the scalar result. Core callers may copy the
result to the next preset; `sharpenErodibleTip()` sets wear to zero.
`BitmapPaintSession` carries wear after a successful pixel commit, but cancelled
strokes and a preset changed during a stroke do not overwrite the selected
brush's wear.

`material.airbrushTip` defaults to disabled. Granularity, spatter and
distortion are [0,1], default 0. SpatterSize is [0.01,4096] document pixels,
default 1. Granularity modulates pixel coverage with deterministic grain;
spatter removes seeded pattern cells of the configured size; distortion
compresses the footprint with tilt. Existing size/hardness/spacing and optional
time-based airbrush control placement. This is not a 3D conical fluid model.

`material.simulation.load` is [0,1], default 1, and multiplies the existing
wet/mixer bitmap pigment contribution. Zero deposits no pixels. Named mixing
variation and the new `BrushDynamicsTarget::Mix` scale `mixStrength` during
source-pixel color mixing. Existing models sample bitmap pixels rather than
maintaining a fluid simulation or a multi-color reservoir.

## Effects, smoothing and preset selection

`stroke.noiseEnabled` and `wetEdgesEnabled` default to false. Their strengths
default to 0.5 and accept [0,1]. Noise modulates coverage using document-position,
seed and mark-index values. Wet edges reduce dense coverage relative to soft
edges; for coverage `a`, its multiplier is `1-strength*a`. It works on the
bitmap tip mask without reconstructing an entire stroke contour.

`stroke.smoothingEnabled` defaults to false; smoothing defaults to 0.5 in
[0,1]. Each incoming position is filtered immediately against the preceding
filtered position with weight `1-0.95*smoothing`. It never queues points,
replays a path, requires future input, or snaps to a predicted endpoint. This
is a causal position filter, not Photoshop's pulled-string/catch-up modes.

`stroke.protectTexture` defaults to false.
`selectBrushPreset(current,replacement)` preserves the current pattern bytes,
dimensions, asset-cache identity and scale when this flag is set; replacement
tone/depth/modes and other settings remain editable. The returned preset keeps
protection enabled. Pass `false` as its third argument to bypass protection.
`BitmapPaintSession::setBrushPreset(preset, preserveProtectedTexture=true)` and
`setBrushPresetData(payload, preserveProtectedTexture=true)` use this same
policy. Explicit false allows disabling protection or replacing a protected
pattern. Invalid replacements fail before mutation, even when their texture
would otherwise be replaced by protection.

## Storage, validation and test boundaries

New output is preset format **3**. Versions 1 and 2 remain readable and acquire
neutral defaults for missing options. Validation rejects invalid enum values,
nonfinite/out-of-range numbers and zero/excessive Fade steps, including disabled
settings. Snapshot/restore copy all nested settings.

Document format remains 3. The historically named `presetV2` field embeds the
versioned payload and now carries preset format 3. Its key remains stable;
the embedded `formatVersion` is authoritative. Older engines reject the new
preset version instead of silently dropping these options. Public structs have
changed layout, so SDK consumers must be rebuilt for package 0.4.0.

`BrushOptionsContractTest` checks controls, pose, initial direction, color,
texture/depth/modes, secondary reflection/count/zoom, noise/wet-edge coverage,
tip extensions, mixer sampling, smoothing, Protect Texture, invalid values,
legacy loading, and full preset/snapshot/document round trips.
`BitmapPaintSessionContractTest` covers real bitmap commits, serialized option
application, persistent wear, cancellation, mid-stroke preset replacement,
and protected/unprotected selection. The existing architecture tests continue
to prohibit retained trajectories and enforce dependency direction.

Observed UI controls not implemented as shared brush options here include
per-panel preset locks/capture policies, symmetry, Photoshop's pulled-string
and catch-up modes, multi-layer sampling/clean-after-stroke tool policy,
healing/clone/selection/remove algorithms, and native menu enum mappings.
They are retained in the investigation ledger rather than stored as ineffective
renderer toggles. Native physical formulas, ABR codec compatibility, complete
Photoshop brush-source recovery and Adobe pixel parity remain unverified.
