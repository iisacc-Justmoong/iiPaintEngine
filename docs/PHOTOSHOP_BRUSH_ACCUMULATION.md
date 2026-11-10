# Brush accumulation: Photoshop observation and engine contract

## Reference experiment

On 2026-11-05, Photoshop 2026 was used on a new 1920 × 1080 RGB/8 sRGB
white document at 100% zoom. The black soft-round brush was 100 px wide,
with hardness 0% and smoothing 0%. A click created a dot; a click followed
by Shift-click created a straight stroke; a second stroke crossed the first.
The exported image is
`resources/photoshop-brush-reference-20261105/photoshop-strokes.png`. The
two earlier black lines at the top are setup marks and are not measurements.

At opacity 50%, flow 100%, the red channel of the dot 20 pixels below its
center was 172 and that of the straight stroke at the same distance from its
centerline was 143. At opacity 100%, flow 20%, the dot and straight-stroke
center values were 204 and 154 respectively. In both cases, a lower value
means more black pigment on the white canvas. The crossings of separate
strokes were darker. These measurements show the qualitative behavior under
the stated setup; they do not identify Photoshop's internal algorithm or
establish pixel, input-device, color-management, or overall feature parity.

## iiPaintEngine requirements

For each source-over dab, the effective stroke cap is source alpha multiplied
by enabled stroke opacity and the dab's opacity-cap dynamics. The projected
sample alpha is that cap multiplied by the tip/material mask and dab flow.
It is the effective alpha of that dab on an empty stroke. A partial mask is
coverage, not a per-pixel ceiling: repeated deposits can build toward the
stroke cap without exceeding it. With a constant cap, a sample contributes
its normalized coverage to the remaining capacity of that pixel. Separate
strokes composite independently. Lower dynamic caps cannot erase pigment
already accumulated in the same stroke.

The same behavior applies to circle, procedural, and image-mask brush tips.
Incremental input batches must produce the same pixels as a single batch;
preview and commit must agree. Eraser input accumulates the same coverage
before it is applied as destination-out to the committed pixels, and its
preview, commit and pixel-based undo/redo must agree. For the low-level
`paintRasterSamples` API, consecutive destination-out samples form an
eraser run, and only touched pixels are accumulated. A change in blend mode
flushes the preceding run before processing the next one.

The reference experiment is about brush appearance. No drawing-latency or
physical stylus performance claim follows from it.

## Verification

On macOS arm64, the canonical `build/` was built and all 68 CTest cases
passed. `PhotoshopBrushAccumulationTest` checks soft-edge buildup for three
tip implementations, moving dabs, low flow, independent strokes,
incremental batches, dynamic caps and translucent sources.
`BitmapPaintSessionContractTest` checks an eraser's changed preview, its
commit and undo/redo pixels. `StrokeCompositingTest` checks repeated
low-flow erasure and ordering across blend-mode changes; its existing stroke
compositing expectations use the stated per-deposit alpha contract.

The previously built macOS Congregation painting-damage integration test was
also run with `DYLD_LIBRARY_PATH` selecting this build's
`libiiPaintEngine.dylib`; the loader confirmed that path and the test returned
`PAINTING_DAMAGE_INTEGRATION_OK`. This is a test-process integration check,
not evidence that a new Congregation application was built, installed, or
deployed to iPad, nor a measurement of pen latency.
