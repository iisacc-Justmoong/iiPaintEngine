# Brush expansion validation — 2026-10-19

## Result and scope

The C++23 brush extensions passed the Release and sanitizer suites on macOS arm64.
Current package version is 0.5.1 (ABI feature line 0.5); the independent concurrent
SDK patch-release version edit was preserved. This record covers source, host
builds, generated bitmap output and isolated installed consumers. It does not
certify a complete Photoshop superset, original Adobe pixel parity or a deployment.

| Check | Result |
| --- | --- |
| Before-change suite | 45/45 passed |
| Final Release suite | 48/48 passed |
| Address/undefined/float-cast-overflow sanitizer suite | 48/48 passed |
| Separate consumer built from a staged installed package | Passed in both builds |
| Source/install library identity | Matching SHA-256 in both builds |
| C++23 translation units | Release 115/115; sanitizer 112/112 |
| Qt in direct dependencies of engine and its Native provider | None found |
| Generated expansion gallery | Exit 0; inspected 960 x 320 PNG |
| Whitespace/error check | git diff --check passed |

Machine-readable results, source hashes, library hashes and log hashes are in the
source checkout's build/brush-expansion-validation.json. Logs are
build/brush-baseline-tests.log, build/brush-final-tests.log and
build/brush-sanitizer-tests.log. Build artifacts remain under build/.

## Tested behavior

- AbrBrushImportContract: legacy v1/v2 geometry and UTF-16 names (including surrogate
  pairs); sectioned versions 6/7/9/10 with both sample subversions; raw and PackBits
  8/16-bit coverage; literal, repeat and no-op packets; rounded 16-bit coverage;
  source IDs and stable local IDs; rendering a decoded tip; truncation, corrupted
  rows, count/decoded/encoded limits and byte mutations. Partial modern settings
  are reported instead of being represented as restored presets.
- BrushSymmetryContract: vertical, radial and mandala placement; primary index/
  random/color preservation; exact asymmetric-tip pixel reflection; reflected
  secondary-mask scattering; coincident-center deduplication; maximum 64-sector
  physical-tip/airbrush behavior; transactional event-budget failure; invalid
  settings; preset/snapshot/document persistence; real preview, cancellation and
  pixel undo/redo.
- BrushStabilizationContract: no movement inside the pulled-string radius;
  straight-line subdivision behavior; optional end-position flush; time-split
  stationary catch-up; monotonic-time rejection; invalid radius/rate; neutral
  defaults for older preset formats; real session clock, preview and history.
- Existing contracts: dependency direction, absence of retained trajectories,
  codecs, input normalization, compositing, layers, material options, archives,
  native headers, file operations and installed-package upgrade behavior.

The new contracts were introduced before implementation; the initial missing-API
build failure was recorded in build/brush-extension-red.log. An additional
asymmetric dual-mask/scatter pixel case exposed an incomplete reflection; the
secondary scatter's original frame and complete symmetry transform were then
carried through projection. The final regression remains in the suite.

## Installation and dependencies

The default host provider installation was too old for the repository's existing
iiFileProviderNative >=0.6 requirement. The already-staged Native 0.8.0 package at
SDK/iiFileProvider/build/native-p0/install was used via its package directory and
CMAKE_PREFIX_PATH. No provider implementation change or host-package replacement
was performed by this brush task.

The installed-consumer contract stages only under
build/install-upgrade-contract/prefix and
build/sanitizer/build/install-upgrade-contract/prefix. It checks exact configured
package version, new public headers, documentation, C++23 compilation, new API
linkage and bitmap/mask operations. Package-version checks use PROJECT_VERSION so
concurrent patch releases do not leave a stale literal in the test.

The library's direct dependencies are its existing Native provider, libc++ and
system libraries. Native provider direct dependencies were inspected separately;
this is a Qt boundary check, not a complete transitive license audit. The new
implementation adds no external dependency or non-C++ source.

## Reproduction

Provide a compatible iiFileProviderNative package with CMAKE_PREFIX_PATH, or use
iiFileProviderNative_DIR explicitly. This machine used:

```sh
export CMAKE_PREFIX_PATH=/Volumes/Storage/Workspace/SDK/iiFileProvider/build/native-p0/install
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --parallel 4
cmake -S . -B build/sanitizer/build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DIIPAINTENGINE_SANITIZE=ON -DIIPAINTENGINE_BUILD_EXAMPLES=OFF
cmake --build build/sanitizer/build --parallel 4
ctest --test-dir build/sanitizer/build --output-on-failure --parallel 4
build/iiPaintEngineBrushExpansionGallery build/brush-expansion-gallery.png
```

The gallery is generated from original procedural inputs: 16-fold radial at left,
12-sector mandala in the center and stabilized dual-axis strokes at right. It is
a workflow example, not an Adobe oracle or a latency benchmark.

## Explicit remaining limits

Modern ABR descriptors, preset/sample associations, patterns, groups, tool capture
and ABR export are not implemented. Sixteen-bit tip data is quantized to 8-bit
coverage; the painting surface remains ARGB32. Physical-tip and mixing models
retain their documented bitmap approximations. Zoom policy, GUI integration,
Windows/Linux/mobile execution, physical tablet input, large-brush latency,
GPU behavior and original-application parity remain outside the verified result.

API contracts, provenance and the remaining implementation order are in
[BRUSH_EXPANSION.md](BRUSH_EXPANSION.md).
