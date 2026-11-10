# iiPaintEngine

[Windows build and execution](docs/WindowsBuildAndRun.md)

Package release: **0.5.4** (2026-10-27). Atomic raster region history adds bounded,
sparse multi-plane undo with explicit rejection, eviction and conflict results.
See [the raster history contract](docs/RASTER_HISTORY.md).
The 0.5.3 source sampling uses overflow-safe
coordinates. Unloaded smudge transfers source color and alpha without adding
foreground pigment or hidden-RGB fringes. See [wet brush sampling](docs/WET_BRUSH_SAMPLING.md).
The 0.5.2 [selection and raster-edit corrections](docs/SELECTION_RASTER_EDIT.md) remain included.


iiPaintEngine is a C++23 bitmap painting library. Its public API uses the C++ standard library; its bundled image codec is MIT-licensed stb, compiled inside a C++23 translation unit. File access is supplied through `BitmapFileIO`; applications may connect the separate iiFileProvider adapter. The engine has no peer SDK, GUI framework, event loop or image plugin dependency.

Brush authoring is also available as the optional `iiPaintEngine::BrushTools`
component, integrated from iiBrushTools. It provides whole-gesture taper,
correction and tile generation while the base streaming engine retains no
input path. Existing `iiBrushTools::iiBrushTools` consumers may use the
installed compatibility package. See [the authoring contract](docs/BrushTools.md).

```cmake
find_package(iiPaintEngine 0.5 CONFIG REQUIRED COMPONENTS BrushTools)
target_link_libraries(my_app PRIVATE iiPaintEngine::BrushTools)
```

Set `IIPAINTENGINE_BUILD_BRUSH_TOOLS=OFF` to build only the base engine.

## Build and install

A C++23 compiler and standard library with `std::expected`, and CMake 3.31 or later are required. The engine builds without other SDK packages. Build artifacts belong under `build/`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build
```

The default top-level install prefix is `~/.local/SDK/iiPaintEngine`. The exported package requires C++23 transitively and uses a single native layout: `include/iiPaintEngine`, `lib`, and `lib/cmake/iiPaintEngine`. It does not select prebuilt Qt platform packages. To consume a package built for the application's platform:

```cmake
find_package(iiPaintEngine 0.5 CONFIG REQUIRED)
target_link_libraries(app PRIVATE iiPaintEngine::iiPaintEngine)
set_target_properties(app PROPERTIES CXX_EXTENSIONS OFF)
```

An earlier macOS installation mirrored the native package under `platforms/macos`; that historical installation is not evidence that this source change has been installed.

0.4 introduced the C++23 API/ABI migration; 0.5 extends public brush layouts and requires another consumer rebuild. Follow [the C++23 migration guide](docs/CXX23_MIGRATION.md). Upgrading replaces this package's public header tree so deleted APIs do not remain available accidentally.

## Bitmap painting

`BitmapPaintSession` owns an open `BitmapFile`, the selected brush, immediate preview pixels, pending stroke pixels, and dirty-rectangle pixel history. Every delivered pointer event is rasterized synchronously. An application controls its window, display, clock, and input delivery separately.

An application provides a `BitmapFileIO` implementation. The separately built
[file-provider adapter](Example/FileProvider/) supplies atomic publication and
queues previews. File operations without a provider fail safely. Existing callers
must inject a provider and rebuild after the public API/ABI change; see
[the bitmap I/O contract](docs/bitmap-io.md).

```cpp
#include <iiPaintEngine>
#include "FileProviderBitmapIO.h"
#include <memory>

int main()
{
    BitmapPaintSession paint(std::make_shared<paint_file::FileProviderBitmapIO>());
    if (!paint.createFile("paint.png", 256, 128)) return 1;
    auto preset = builtInBrushPresets().front();
    preset.size = 24;
    preset.dynamics.sizeVariation.enabled = true;
    preset.dynamics.sizeVariation.control = BrushDynamicsControl::PenPressure;
    if (!paint.setBrushPreset(preset)) return 2;
    paint.setBrushColor(0xFF3366CCU);

    PointerEvent event;
    event.device = PointerDeviceKind::Tablet;
    event.tool = PointerToolKind::Pen;
    event.phase = PointerEventPhase::Press;
    event.button = PointerButton::Primary;
    event.primaryButtonDown = true;
    event.documentPosition = {32, 64};
    event.pressure = 0.7;
    if (!paint.processPointerEvent(event).accepted) return 3;
    event.phase = PointerEventPhase::Release;
    event.primaryButtonDown = false;
    if (!paint.processPointerEvent(event).committed) return 4;
    return paint.save() ? 0 : 5;
}
```

During a stroke, `previewPixels()` exposes the composed preview while `file().pixels()` remains committed. Release commits exactly those preview pixels. Cancel discards pending pixels. Undo/redo exchange pixel patches, without replaying input. A preset or color selected during a stroke applies to the next stroke; the active brush is a snapshot. Successful erodible-tip commits carry wear to the selected preset unless it changed during the stroke.

Input coordinates are document pixels and time is monotonic seconds. Use the standard C++ viewport transforms when mapping a window's coordinates. For stationary airbrush emission or enabled pulled-string catch-up call `advanceTime(seconds)` from the application's clock. The engine does not create timers or coalesce events. Applications must deliver every measured event they need rendered.

## Local correction masks

`MaskPaintSession` creates normalized float pixel masks with diameter, feather, flow, density, stroke opacity, Add/Erase and start-color-guided AutoMask. Preview, commit, cancel, undo and redo use pixel states; no input path is retained. `PixelMask` coverage can be borrowed by a separate image-adjustment library without linking the SDKs. Optional size and opacity pressure switches reuse existing brush dynamics. See [local mask brush behavior and limits](docs/LOCAL_MASK_BRUSH.md) and [recovered localized brush numeric helpers](docs/LOCALIZED_BRUSH_MATH.md).

## Bitmap-only contract

```text
PointerEvent → current StrokePoint → RasterDabStream (one preceding point)
→ BrushDab bitmap stamps → RasterSample pixels → StrokeCompositeBuffer → RasterLayer
```

Neither `InputStrokeBuilder` nor `RasterDabStream` owns a point collection. A dab is projected and discarded in the current call. Pending preview and history retain pixels; they do not retain paths, vector shapes, future coordinates, or replayable drawing commands. External text, SVG, and shapes must be rasterized before crossing the engine boundary. Causal smoothing uses one preceding filtered point and one current raw pointer target.

## Brushes and documents

`BrushPreset` is an editable value object for tips, shape, spacing, color, dynamics, pose, texture, secondary masks, and bitmap material effects. `resolveBrushPreset()` validates every setting and creates `BrushState`. Six editable built-in presets demonstrate pressure ink, grain pencil, soft airbrush, flat marker, color spray, and wet flat mixing. [Brush API and units](docs/BRUSHES.md) and [extended option behavior](docs/BRUSH_OPTIONS.md) document the contract.

Preset format 4 preserves extended options and reads formats 1/2/3. Pixel document format 3 stores layer pixels, metadata, masks, brush presets, color metadata, assets, and raster history. It reads compatible format 2 single-surface documents. Unknown or incompatible content is rejected. The Photoshop investigation informed option coverage; renderer formulas remain independent bitmap models and do not establish Adobe pixel parity.

0.5 adds [bounded ABR tip import, symmetry and stabilization](docs/BRUSH_EXPANSION.md).
ABR v1/v2 geometry and v6/v7/v9/v10 sampled tips can become editable presets;
modern descriptor/tool/texture settings remain explicitly unsupported. Symmetry
includes 64-segment radial and mandala modes, with reflected bitmap tips and
per-event resource bounds. Pulled-string stabilization offers clock-driven
catch-up and optional endpoint flushing. All use the existing preview, commit,
cancel and raster history paths.

`Application/DocumentAdapter`, `LayerListModel`, and `PaintEngineController` provide standard C++ document/layer editing alongside the file session. Lower modules do not depend on Application; brushes do not depend on stroke execution.

## File formats

Format detection uses file contents, including extensionless files and misleading extensions. `BitmapFile` owns `std::filesystem::path`, a detected format name, editable ARGB32 pixels, and modification state. Its queries return a fixed registry matching the included codecs.

| Operation | Formats |
|---|---|
| Read | BMP, GIF, HDR, JPEG, PIC, PNG, binary PNM (P5/P6), PSD, TGA |
| Write / save in place | BMP, HDR, JPEG, PNG, binary PNM (P6), TGA |

Binary PNM is decoded with standard C++ code that normalizes Maxval and reads 16-bit samples in big-endian order, following the [Netpbm format specification](https://netpbm.sourceforge.net/doc/ppm.html). GIF reads its first frame. PSD reads the flattened raster composite, not layers. HDR and 16-bit data are converted to the engine's 8-bit channels. EXIF orientation and ICC color transforms are not applied; metadata and embedded profiles are not preserved. Formats previously supplied by optional Qt plugins, including TIFF, WebP, HEIF, ICO/ICNS, and JPEG 2000, are not part of this build. Vector/PDF input is rejected. Read-only formats remain editable and can be saved using `saveAs` in a supported write format.

Maximum dimensions are 16384 per axis, maximum decoded pixels are 64 Mi, and encoded files are limited to 256 MiB. Failed create/open/save operations preserve the current object. Encoding completes in memory before the supplied file provider publishes it; the provider must preserve an existing file when publication fails.

PNG/BMP/TGA preserve alpha. JPEG/PNM/HDR flatten alpha against `BitmapFileWriteOptions::backgroundArgb`. JPEG quality accepts -1 (default 90) or 0–100. `save()` preserves the detected source format; `saveAs()` uses an explicit format or the destination extension.

## Runnable C++ examples

```sh
cmake -S Example/FileProvider -B build/file-provider-app \
  -DCMAKE_PREFIX_PATH="/path/to/iiPaintEngine;/path/to/iiFileProvider"
cmake --build build/file-provider-app --parallel
build/file-provider-app/iiPaintEngineExample Example/FileProvider/resources/paint.png
build/file-provider-app/iiPaintEngineBrushGallery Example/FileProvider/resources/brush-gallery.png
build/file-provider-app/iiPaintEngineBrushExpansionGallery Example/FileProvider/resources/brush-expansion-gallery.png
```

All examples produce bitmap files and require no windowing runtime. The preset gallery renders all six presets; the expansion gallery demonstrates radial, mandala and stabilized strokes.

## Dependencies and validation

The bundled stb files are pinned to commit `2c980bb59875b0d32144a71867fbdebb2f77cd20` of [nothings/stb](https://github.com/nothings/stb). [Provenance and SHA-256 hashes](third_party/stb/provenance.json) and the selected [MIT license](third_party/stb/LICENSE) accompany the package. No separate C source, assembly, Objective-C, scripting runtime, or GUI library is compiled. iiPaintEngine itself retains its [MIT license](LICENSE).

Contract tests cover source/dependency boundaries, public C++23 headers, codec round trips and failure preservation, immediate preview versus commit, cancel, undo/redo, input normalization, brush options/materials, serialization, document/layer operations, and installing then compiling/running a separate C++ consumer. Sanitizer builds are available with `-DIIPAINTENGINE_SANITIZE=ON`. Host tests do not prove physical pen input or runtime operation on other platforms.

Layer composition supports 27 pixel-layer blend modes, reproducible Dissolve samples, masked clipping chains and correct alpha weighting over transparent backdrops. See [Layer compositing](docs/LAYER_COMPOSITING.md). Brush stroke blend-mode validation retains its existing supported subset.

## License

SPDX-License-Identifier: MIT

The original code and documentation in this repository are licensed under the
[MIT License](LICENSE). Third-party code, libraries, fonts, tools, and model
weights retain their respective licenses.

<!-- workspace-docs:readme:start -->
## Documentation entry points

- Human overview: this English `README.md`.
- Agent instructions: [AGENT.md](AGENT.md).
- Human documentation: [documentation index](docs/README.md).
- Structured agent documentation: [agent index](docs/agent-index.json).
<!-- workspace-docs:readme:end -->
