# iiPaintEngine

iiPaintEngine is a pure bitmap file painting engine for C++ / Qt /QML. The target is not an abstract screen surface but `src/BitmapFile` combined with paths and original format. Input paths are not created as vector strokes, saved, or replayed.

## Bitmap-only contract

Drawing input changes to bitmap immediately for each press, move, release event.

```text
PointerEvent
→ current StrokePoint
→ RasterDabStream(previous point only)
→ BrushDab bitmap stamp
→ RasterSample pixels
→ StrokeCompositeBuffer
→ RasterLayer
```

`InputStrokeBuilder` does not hold a coordinate list and emits only one point of the current event. `RasterDabStream` holds only the accumulated state of the previous point and interval·visual·direction·randomness. Entire trajectory, curve, original input, and replay command are not created even temporarily. `BrushDab` is a one-time bitmap stamp projected immediately to pixels, not a re-editable shape.

`configureHighFidelityPointerInput()` disables Qt's high-frequency tablet event coalescing, and macOS also disables AppKit's mouse/drag/tablet coalescing. It is called before `QGuiApplication` generation if possible. `registerIipeQmlTypes()` and `BitmapFileItem` also automatically apply this policy, so intermediate coordinates measured during delay are discarded and only the two endpoints are connected in a straight line. Live preview copies only the sample pixels actually touched by the new dab, and does not scan the entire rectangle enclosing two distant points on each input.

Live preview is the result of displaying the pending bitmap buffer. At release, that pixel buffer is directly composited into the open `src/BitmapFile` ARGB pixels. Undo/redo uses a raster snapshot or dirty-rect pixel patch. The saved file records only the current pixels and does not store the pointer trajectory.

External content such as text, SVG, and shapes must be converted to a bitmap before being placed in iiPaintEngine. The engine owns only the raster paint layer.

## Architecture

The file editing ownership structure is as follows.

```text
BitmapFile
→ file path + detected source format
→ RasterLayer
→ ARGB pixels

BitmapFileItem
→ view/input translation only
→ BitmapFile pixel mutation API
```

`src/BitmapFile` owns the file path, format detected from the actual bytes, ARGB32 sRGB pixels, and modification state together. `BitmapFileItem` does not own the file pixels; it only handles input-coordinate conversion and screen display. Changing the item's width/height does not change the file's pixel dimensions. A new operation also creates an actual file target first through `createFile(path, width, height, format)` rather than creating an anonymous surface.

In the C++ flow where a layer document archive is required, `PaintDocument` directly owns the final synthesized `DrawingSurface` and `LayerStack`. This flow also creates only a pixel-based actual content and does not create a separate screen surface model from the file edit path.

The main modules are as follows.

- `src/Core`: coordinate, rect, UUID, error, raster sample value type
- `src/Input`: mouse/tablet event normalization and current point release
- `src/Brush`: preset, dynamics, material, texture, wet/bristle settings
- `src/Stroke/Rasterizer`: bitmap dab placement from the previous point to the current point and pixel projection
- `src/Layer`: raster surface, mask, blend, layer stack
- `src/Render`: CPU / GPU raster layer compositing, tile cache, brush stamp atlas
- `src/BitmapFile`: runtime bitmap format detection/storage, file path, directly editable ARGB pixels
- `src/Document`: single bitmap surface, layer stack, and format version 3 serialization and version 2 single surface read compatibility
- `src/History`: raster patch/snapshot-based undo/redo metadata
- `src/QtAdapter`: `BitmapFileItem` view/input adapter, document adapter, and layer list facade

Render cache contains only tile cache and brush stamp atlas. There is no stroke replay cache that recalculates input paths.

## Advanced brushes

`BrushPreset` sets multi-mask tips, shape, direction, and inversion, scatter count and distribution, HSV color changes, time injection, and per-input user curves and property connections. `resolveBrushPreset()` validates the settings and converts them to runtime `BrushState`, while `builtInBrushPresets()` provides six kinds of editable examples. The same object is used for actual file painting via `BitmapFileItem::setBrushPreset()` or QML's `setBrushPresetData()`. Preset version 2 and the document's brushSources traverse the entire extended settings. [brush API, units, examples, and compatibility](docs/BRUSHES.md)are referenced.

## C++ integration

Installed consumers can use the public API via a single extensionless umbrella header.

```cpp
#include <QGuiApplication>
#include <iiPaintEngine>

int main(int argc, char **argv)
{
    configureHighFidelityPointerInput();
    QGuiApplication app(argc, argv);
    // register types and load the application...
    return app.exec();
}
```

```cmake
find_package(iiPaintEngine CONFIG REQUIRED)
target_link_libraries(app PRIVATE iiPaintEngine::iiPaintEngine)
```

When drawing to a document layer, already rasterized pixels are passed.

```cpp
PaintEngineController engine = makePaintEngineController(1024, 768);

std::vector<RasterSample> pixels{
    RasterSample{{100, 100}, 0xFF202020U},
    RasterSample{{101, 100}, 0xFF202020U},
};

commitPaintSamples(engine, pixels, {{100.0, 100.0}, 2.0, 1.0});
```

`commitPaintSamples` updates the pixels of the active layer and records the raster paint history, leaving no original coordinates or brush trajectory in the document.

## QML API

The application calls `registerIipeQmlTypes()` at startup and then loads the `iipe` module.

```qml
import QtQuick 2.15
import QtCore
import iipe 1.0 as Iipe

Iipe.BitmapFile {
    id: bitmapFile
    anchors.fill: parent

    documentX: 0
    documentY: 0
    zoom: 1
    bitmapDevicePixelRatio: 1

    brushColor: "#111111"
    brushSize: 18
    brushSpacingRatio: 0.1
    brushSpacingEnabled: true
    brushFlow: 1.0
    brushFlowEnabled: true
    brushOpacity: 1.0
    brushOpacityEnabled: true
    brushHardness: 1.0
    brushHardnessEnabled: true
    pressureToOpacityEnabled: true
    livePreviewEnabled: true

    Component.onCompleted: {
        const path = StandardPaths.writableLocation(StandardPaths.TempLocation) + "/paint.png"
        createFile(path, 1024, 768, "png")
    }
}
```

The viewport API provides `documentX`, `documentY`, `zoom`, `bitmapDevicePixelRatio`, `setDocumentViewport`, `resetView`, `panBy`, and `zoomAt`. The brush API provides color, size, spacing, flow, opacity, hardness, pressure curve, and each enabled flag. The state API can read `liveStrokeActive`, `strokeCount`, `inputDevice`, and `inputPressure`.

`BitmapFileItem` provides `createFile`, `openFile`, `save`, `saveAs`, `undo`, `redo`, `toolMode`, `brushConfig`, `viewportConfig`, `runtimeConfig`, and `stateSnapshot` as a single file-centric API. `filePath`, `fileFormat`, `fileOpen`, `modified`, `pixelWritable`, `canSaveInPlace`, `bitmapWidth`, and `bitmapHeight` check the current file status. `supportedOpenFormats` and `supportedSaveFormats` inspect the actual bitmap codec of the current Qt runtime, and `supportedEditableFormats` is the intersection where both reading and writing are possible. Pixel editing is also possible for read-only formats, and if no original format writer exists, the installed write format `saveAs` is selected.

`toolMode: "eraser"` composites the pending bitmap alpha mask with destination-out.

## Bitmap file compatibility

`src/BitmapFile` already includes an image I/O plugin in Qt Gui, so no external dependencies are added. Reading prioritizes detecting the actual byte format over file extension, applies EXIF direction, and normalizes to ARGB32 sRGB pixels of its own `RasterLayer`. Saving uses the detected format preserved in the object, so even files with wrong extensions are rewritten as the original bitmap format. `saveAs` uses explicit format or extension, and opaque formats like JPEG are composited to the background color, and all saving atomically replaces with `QSaveFile`.

PNG, JPEG, BMP, WebP, TIFF, GIF, ICO / ICNS, HEIF, JPEG 2000, etc., are exposed only when the codec is installed at the corresponding Qt runtime. SVG, SVGZ, PDF and similar vector inputs are excluded from the allow list even if the plugin is installed and are not passed to the decoder. Therefore, vector shapes or paths are not created even as temporary representations. The animation container reads only the first bitmap frame.

## Document format

`DocumentArchive::formatVersion` and `DocumentSerializer::formatVersion` are 3. version 3 directly saves the surface and layer stack of `PaintDocument`. The single-surface archive of version 2 is migrated to the same bitmap document structure when reading, and is recorded as version 3 from the next save. Legacy archives with multiple surfaces and higher future versions return `compatible=false` and an error instead of taking some data, thereby preserving the original. The archive round-trips the following data.

- document composite and raster layer pixel surface
- layer metadata, mask, children, blend/opacity
- brush source preset
- color space and ICC profile
- embedded asset
- raster command history metadata and pixel patch payload

The archive does not include the entire input point, curve, dab sequence, or replayable stroke.

## Build and example

The build directory is always `build/`.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The example executable is created in the following build-tree path, not in the source tree.

```text
build/Example/bin/iiPaintEngineExample
```

`Example/Main.qml` uses LVRS controls and `Iipe.BitmapFile`, creates a temporary PNG file as the actual working target, and verifies size, flow, opacity, hardness, spacing, pressure curve, live preview, and clear/reset view through the public API. Example slider ranges are 2–72 px for size, 0.05–1.0for flow/opacity/hardness, and 0–1.0 for spacing. `iiPaintEngineExampleDemoContract` verifies this QML contract.

## Install

The default install prefix is `~/.local/SDK/iiPaintEngine`. On Unix-like systems, run the following. The default root of the local Qt kit is `/Volumes/Storage/Qt/6.8.3` and can be overridden with `IIPAINTENGINE_QT_ROOT`.

```sh
./install.sh
IIPAINTENGINE_INSTALL_PLATFORMS=macos ./install.sh
```

macOS default full installation automatically detects Homebrew Android SDK /NDK and specifies the detected NDK toolchain to the Qt Android chainload path, avoiding use of old Qt cache paths. iOS · Android · WASM installation passes the current operating system's Qt tool path to `QT_HOST_PATH`. A separate host Qt installation is specified via `IIPAINTENGINE_HOST_QT_PREFIX` or `QT_HOST_PATH`. After mobile installation, the actual iOS / Android library is checked to see if it is iOS ARM64 and Android AArch64 respectively.

```sh
python3 tests/verify_mobile_packages.py --prefix ~/.local/SDK/iiPaintEngine --ndk "$ANDROID_NDK_ROOT"
```

At Windows PowerShell, the following is executed.

```powershell
.\install.ps1

$env:IIPAINTENGINE_INSTALL_PLATFORMS = "windows"
.\install.ps1
```

Windows host builds are fixed to use the selected Qt kit matching Qt MinGW 13.1.0 and Ninja combination. Visual Studio generator and MinGW Qt binary are not mixed. The install script specifies matching compiler and Ninja path to CMake.

Host package and platform mirror are installed next.

```text
~/.local/SDK/iiPaintEngine/
~/.local/SDK/iiPaintEngine/platforms/macos/
~/.local/SDK/iiPaintEngine/platforms/linux/
~/.local/SDK/iiPaintEngine/platforms/windows/
~/.local/SDK/iiPaintEngine/platforms/ios/
~/.local/SDK/iiPaintEngine/platforms/android/
~/.local/SDK/iiPaintEngine/platforms/wasm/
```

The root CMake package version is architecture independent so one prefix can
dispatch both 64-bit native consumers and 32-bit WASM consumers. Each
platform package keeps its generated binary architecture compatibility check.

The installation script places shared library, headers, CMake package config, and license together and runs host tests. Since the single configuration generator is also configured as Release, the installed `iiPaintEngine::iiPaintEngine` target includes `IMPORTED_LOCATION_RELEASE` that consumers can link. Default all-platform installation without optional cross SDK skips the corresponding platform, and fails if the platform is explicitly requested but no toolchain is available. Upgrade installation replaces the tree owned by the package `include/iiPaintEngine` with the current public headers. Therefore, deleted API headers remain in the prefix without colliding with new types, and consumer-only headers must be kept outside the directory owned by this package. macOS package config removes the legacy AGL framework with no binary in the current SDK from the Qt link interface, so consumers linking the installation target can also build from the latest macOS SDK. Additionally, since the installation path exists for the compiler's `LIBRARY_PATH`, the package target directly passes the iiPaintEngine dylib runtime search path even if CMake judges it as an implicit link directory. Apple shared library is built with install RPATH from the start, so repeated installation to the same prefix does not reprocess the already removed build RPATH. Qt WASM uses a static Qt SDK, so WASM packages are installed as `libiiPaintEngine.a` static archives. The native platform holds the shared library and, even when combined with a final executable in a separate installation library with a static archive at WASM, does not include duplicate symbols at Qt. Installed CMake targets also propagate the Emscripten embind link options required by Qt WASM platform plugins to the consumer. At macOS, it also automatically detects Homebrew's `~/Library/Android/sdk` and `/opt/homebrew/share/android-commandlinetools` in addition to `/opt/homebrew/share/android-ndk`.

The layout-contract test executable for Windows uses the name `iiPaintEngineLayoutContractTests`. It is a contract to avoid applying `Install...` installer detection heuristics and elevation false positives to executables starting with Windows.

## Verification contracts

Core validations are as follows.

- `iiPaintEngineBitmapOnlyArchitectureContract` : absence of forbidden path/layer source, absence of retained stroke in document, absence of input point collection, per-event pixel accumulation
- `iiPaintEngineBitmapFileCompatibilityContract` : ownership of path/format/pixel in file hierarchy, direct pixel mutation, full runtime bitmap codec write round-trip, content sniffing, SVG / PDF block
- `iiPaintEngineBitmapFileArchitectureContract`: absence of legacy screen surface API, public contract with `src/BitmapFile` and `BitmapFileItem`
- `iiPaintEngineInstallUpgradeContract` : remove deleted public headers upon reinstallation, current header layout, separate CMake consumer's `find_package` ·link·load using installation package
- `iiPaintEnginePipelineHeartbeat` : minimum pipeline from pointer event to document raster surface
- `iiPaintEngineDocumentSerializerContract` : bitmap archive round-trip of format version 3, version 2 single surface transfer, absence of raw trajectory
- `iiPaintEngineAppDocumentApiContract` : raster sample commit, layer/history, save/open round-trip
- `iiPaintEnginePointerStrokeFlow` : bitmap dab spacing/flow per mouse event
- `iiPaintEngineHighFidelityPointerInputContract` : deactivation of Qt · macOS pointer event merging and `BitmapFileItem` automatic application
- `iiPaintEngineRasterPaintingModel` : bitmap dab reflection of velocity, pressure, tilt, spacing, opacity/flow
- `iiPaintEngineBitmapFileLivePreviewRealtimeContract` : pending raster preview of open files, direct pixel commit, eraser, undo/redo
- `iiPaintEngineBrushDynamicsMapping` : dab attribute reflection of pressure/velocity/tilt/random
- `iiPaintEngineBrushTextureLayerContract` : document/tip/follow/paper bitmap texture and dual brush
- `iiPaintEngineWetBrushSimulationContract`: source raster sampling, pickup/deposit/mix, bristle footprint
- `iiPaintEngineRenderCacheBackendContract` : raster tile cache, brush stamp atlas, CPU / GPU plan
- `iiPaintEnginePublicUmbrellaHeaderContract` : single `#include <iiPaintEngine>` contract for external consumers
- `iiPaintEnginePublicCxx17HeaderContract`: C++17 public header compatibility
- `iiPaintEngineInstallLayoutContract`: Unix/ Windows install, package export, example output, license contract

During static verification, the product source is checked together to ensure that no layers other than the full input collection, curve/path model, and replay cache are regenerated.

## License

SPDX-License-Identifier: AGPL-3.0-only

The self-written code and documents of iiPaintEngine are distributed under the GNU Affero General Public License version 3 only ( `AGPL-3.0-only` ). The full terms follow [LICENSE](LICENSE).

External Qt, LVRS, and other third-party code, libraries, tools, and models retain their respective licenses and copyright notices, and this repository's license does not replace them.

### SDK workspace and installation paths

The source checkout is `Workspace/SDK/iiPaintEngine`. Both `./install.sh` and a
fresh direct CMake configuration default to `~/.local/SDK/iiPaintEngine`.
An explicit `-DCMAKE_INSTALL_PREFIX` remains authoritative for direct CMake
configuration. Build outputs stay in the repository's `build/` directory; the
installer regenerates stale CMake caches after a workspace move.

The default LVRS dependency prefix is `~/.local/SDK/LVRS`;
`IIPAINTENGINE_LVRS_PREFIX` remains available for an explicit override.

<a id="파일-저장-소유권"></a>

## File storage ownership

BitmapFile delegates file reads and atomic output to iiFileProvider 0.5. Image codecs receive the QIODevice opened by the provider. Raster editing, color conversion, and file-format selection remain in iiPaintEngine. The provider does not reference this SDK.

## Source layout

Implementation files and their headers live together under `src/`. Existing feature and platform subdirectories retain their responsibilities. Build configuration, tests, documentation, resources, and maintenance scripts remain at the project root. Configure and build using the repository-local `build/` directory.
