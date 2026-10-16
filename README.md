# iiPaintEngine

iiPaintEngine 0.4.2 is a C++23 bitmap painting library. Its public API uses the C++ standard library; its only bundled external implementation is the MIT-licensed stb image codec, compiled inside a C++23 translation unit. It has no GUI framework, event loop, image plugin discovery, or external SDK dependency. Source, examples, and tests compile as C++ only.

## Build and install

A C++23 compiler and standard library with `std::expected` and `std::ios::noreplace`, plus CMake 3.31 or later, are required. Build artifacts belong under `build/`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build
```

The default top-level install prefix is `~/.local/SDK/iiPaintEngine`. The exported package requires C++23 transitively and uses a single native layout: `include/iiPaintEngine`, `lib`, and `lib/cmake/iiPaintEngine`. It does not select prebuilt Qt platform packages. To consume a package built for the application's platform:

```cmake
find_package(iiPaintEngine 0.4 CONFIG REQUIRED)
target_link_libraries(app PRIVATE iiPaintEngine::iiPaintEngine)
set_target_properties(app PROPERTIES CXX_EXTENSIONS OFF)
```

The verified local macOS installation also mirrors this native package under `platforms/macos` to retain the earlier package-directory convention. Both paths contain the same C++23 library and standard C++ headers; no GUI runtime is bundled. This mirror represents the current host build only.

0.4 is an API/ABI migration. Rebuild consumers and follow [the C++23 migration guide](docs/CXX23_MIGRATION.md). Upgrading replaces this package's public header tree so deleted APIs do not remain available accidentally.

## Bitmap painting

`BitmapPaintSession` owns an open `BitmapFile`, the selected brush, immediate preview pixels, pending stroke pixels, and dirty-rectangle pixel history. Every delivered pointer event is rasterized synchronously. An application controls its window, display, clock, and input delivery separately.

```cpp
#include <iiPaintEngine>

int main()
{
    BitmapPaintSession paint;
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

Input coordinates are document pixels and time is monotonic seconds. Use the standard C++ viewport transforms when mapping a window's coordinates. For stationary airbrush emission call `advanceTime(seconds)` from the application's clock. The engine does not create timers or coalesce events. Applications must deliver every measured event they need rendered.

## Local correction masks

`MaskPaintSession` creates normalized float pixel masks with diameter, feather, flow, density, stroke opacity, Add/Erase and start-color-guided AutoMask. Preview, commit, cancel, undo and redo use pixel states; no input path is retained. `PixelMask` coverage can be borrowed by a separate image-adjustment library without linking the SDKs. Optional size and opacity pressure switches reuse existing brush dynamics. See [local mask brush behavior and limits](docs/LOCAL_MASK_BRUSH.md) and [recovered localized brush numeric helpers](docs/LOCALIZED_BRUSH_MATH.md).

## Bitmap-only contract

```text
PointerEvent → current StrokePoint → RasterDabStream (one preceding point)
→ BrushDab bitmap stamps → RasterSample pixels → StrokeCompositeBuffer → RasterLayer
```

Neither `InputStrokeBuilder` nor `RasterDabStream` owns a point collection. A dab is projected and discarded in the current call. Pending preview and history retain pixels; they do not retain paths, vector shapes, future coordinates, or replayable drawing commands. External text, SVG, and shapes must be rasterized before crossing the engine boundary. Causal smoothing uses only the current and preceding filtered point.

## Brushes and documents

`BrushPreset` is an editable value object for tips, shape, spacing, color, dynamics, pose, texture, secondary masks, and bitmap material effects. `resolveBrushPreset()` validates every setting and creates `BrushState`. Six editable built-in presets demonstrate pressure ink, grain pencil, soft airbrush, flat marker, color spray, and wet flat mixing. [Brush API and units](docs/BRUSHES.md) and [extended option behavior](docs/BRUSH_OPTIONS.md) document the contract.

Preset format 3 preserves extended options and reads formats 1/2. Pixel document format 3 stores layer pixels, metadata, masks, brush presets, color metadata, assets, and raster history. It reads compatible format 2 single-surface documents. Unknown or incompatible content is rejected. The Photoshop investigation informed option coverage; renderer formulas remain independent bitmap models and do not establish Adobe pixel parity or ABR compatibility.

`Application/DocumentAdapter`, `LayerListModel`, and `PaintEngineController` provide standard C++ document/layer editing alongside the file session. Lower modules do not depend on Application; brushes do not depend on stroke execution.

## File formats

Format detection uses file contents, including extensionless files and misleading extensions. `BitmapFile` owns `std::filesystem::path`, a detected format name, editable ARGB32 pixels, and modification state. Its queries return a fixed registry matching the included codecs.

| Operation | Formats |
|---|---|
| Read | BMP, GIF, HDR, JPEG, PIC, PNG, binary PNM (P5/P6), PSD, TGA |
| Write / save in place | BMP, HDR, JPEG, PNG, binary PNM (P6), TGA |

Binary PNM is decoded with standard C++ code that normalizes Maxval and reads 16-bit samples in big-endian order, following the [Netpbm format specification](https://netpbm.sourceforge.net/doc/ppm.html). GIF reads its first frame. PSD reads the flattened raster composite, not layers. HDR and 16-bit data are converted to the engine's 8-bit channels. EXIF orientation and ICC color transforms are not applied; metadata and embedded profiles are not preserved. Formats previously supplied by optional Qt plugins, including TIFF, WebP, HEIF, ICO/ICNS, and JPEG 2000, are not part of this build. Vector/PDF input is rejected. Read-only formats remain editable and can be saved using `saveAs` in a supported write format.

Maximum dimensions are 16384 per axis, maximum decoded pixels are 64 Mi, and encoded files are limited to 256 MiB. Failed create/open/save operations preserve the current object. Encoding completes in memory before a same-directory exclusive staging file is written. Replacement is atomic where standard filesystem rename supports overwrite; otherwise the old file is retained with a restore-on-failure fallback. This fallback is not an atomic exchange and is not a multi-writer synchronization mechanism.

PNG/BMP/TGA preserve alpha. JPEG/PNM/HDR flatten alpha against `BitmapFileWriteOptions::backgroundArgb`. JPEG quality accepts -1 (default 90) or 0–100. `save()` preserves the detected source format; `saveAs()` uses an explicit format or the destination extension.

## Runnable C++ examples

```sh
build/Example/bin/iiPaintEngineExample build/paint.png
build/iiPaintEngineBrushGallery build/brush-gallery.png
```

Both examples produce bitmap files and require no windowing runtime. The gallery renders all six presets.

## Dependencies and validation

The bundled stb files are pinned to commit `2c980bb59875b0d32144a71867fbdebb2f77cd20` of [nothings/stb](https://github.com/nothings/stb). [Provenance and SHA-256 hashes](third_party/stb/provenance.json) and the selected [MIT license](third_party/stb/LICENSE) accompany the package. No separate C source, assembly, Objective-C, scripting runtime, or GUI library is compiled. iiPaintEngine itself retains its [AGPL-3.0 license](LICENSE).

Contract tests cover source/dependency boundaries, public C++23 headers, codec round trips and failure preservation, immediate preview versus commit, cancel, undo/redo, input normalization, brush options/materials, serialization, document/layer operations, and installing then compiling/running a separate C++ consumer. Sanitizer builds are available with `-DIIPAINTENGINE_SANITIZE=ON`. Host tests do not prove physical pen input or runtime operation on other platforms.

Layer composition supports 27 pixel-layer blend modes, reproducible Dissolve samples, masked clipping chains and correct alpha weighting over transparent backdrops. See [Layer compositing](docs/LAYER_COMPOSITING.md). Brush stroke blend-mode validation retains its existing supported subset.
