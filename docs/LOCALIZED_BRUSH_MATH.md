# Localized brush numeric helpers

`Brush/LocalizedBrushMath.h` exposes the pure feather/radius calculations
verified in the shipped Lightroom 9.6 Lua 5.1 bytecode. These functions belong
to Brush and depend only on standard C++23, without Stroke or Application.
They do not alter the independent float feather model in `MaskPaintSession`.

`localizedBrushDabParameters(radius, feather)` returns a radius in the caller's
input units and a normalized `centerWeight`. Feather is finite `[0,1]`; radius
is finite and nonnegative. The observed calculation starts with `w=1-feather`
and repeats `w=(w*0.5)*(1+w)` three times. It finds the normalized radius where
the helper profile reaches 0.3 using exactly sixteen bisections followed by
linear interpolation, then returns `radius/root` and `w`. The original memoizes
the root by weight; this small allocation-free function computes it directly.

`localizedBrushProfile(normalizedRadius, centerWeight)` exposes that helper
profile for finite normalized arguments. It preserves the original arithmetic
ordering instead of replacing repeated multiplication with `pow`. Both return
`std::expected` errors for invalid input; a derived radius that overflows is
also rejected. These are additional SDK bounds, not bounds present in Adobe's
original helper.

## Source and limits

The actual input is
`AgCameraRawControllerLocalizedCorrectionMethods.lua`, SHA-256
`2b2bb57cfa8953bd7c4af07da970a723e424bc097fbd53788f46a94f1a77c3bc`.
The original Lua bytecode prototypes f66/f67/f68/f70/f71 and their opcode words,
typed constants, upvalues, and file positions were checked against the retained
disassembly. The corresponding readable
[helper excerpt](/Volumes/Storage/Workspace/Engineering/Projects/Adobe/Lightroom/recovered/scripts/decompiled/Contents/PlugIns/Develop.lrmodule/Contents/Resources/AgCameraRawControllerLocalizedCorrectionMethods.lua:3878)
is at 3878..4014, and the native parameter handoff is at 4805..4818.

The complete high-level decompiler file fails Lua syntax checks. Its readable
text alone therefore is not authority for execution. The investigation's
[source audit](/Volumes/Storage/Workspace/Engineering/Projects/Adobe/evidence/sdk-local-processing/evidence-audit.json)
preserves the matching original opcode/constant anchors. `centerWeight` and
adjusted radius are handed to the native CameraRaw boundary; the native brush
pixel kernel, full brush rendering, and Adobe output parity are not recovered
by these numeric functions.

Tests cover 101 feather values, radial boundedness/monotonicity, the independent
analytic weight 1 root, endpoints/units/zero radius, invalid values, and output
overflow. In addition, 21 original-prototype vectors test the output radius,
center weight, and profile with exact float64 bit comparisons. Their provenance
is in [localized-brush-math-provenance.json](localized-brush-math-provenance.json).
The passive [golden vector record](localized-brush-vectors.json) is documentation;
the tests consume only the corresponding standard C++ fixture header.
The original byte ranges of six pure numeric prototypes were copied unmodified
into a compiler-created Lua 5.1 closure harness and executed with the existing
official Lua 5.1.5 interpreter. Verified upvalues link only these helpers, the
constants 0.3/16, and a controlled memoization table. Global opcodes are absent;
Adobe's module main, external cache factory, native bindings, and GUI are not
executed. This scoped oracle is distinct from the SDK's float mask renderer.
Exact arithmetic requires contraction and fast-math disabled for this translation
unit. Java/Lua investigation tools are not product dependencies.
