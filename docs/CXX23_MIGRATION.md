# C++23-only migration (0.4.0)

The library and public headers now require C++23. Applications must rebuild against 0.4.0; the package version contract does not treat 0.3 as ABI compatible. The previous brush extensions and preset/document storage formats are retained.

## API mapping

| Earlier API | C++23 API |
|---|---|
| `QtAdapter/BitmapFileItem` and QML `Iipe.BitmapFile` | `Application/BitmapPaintSession` |
| `QtAdapter/DocumentAdapter`, `LayerListModel`, `PaintEngineController` | Same C++ value APIs under `Application/` |
| `QString` file path | `std::filesystem::path` |
| `QString` format/error, `QStringList` registry | `std::string`, `std::vector<std::string>` |
| `QImage` image access | `const RasterLayer& BitmapFile::pixels()` |
| QColor brush/background | ARGB32 `std::uint32_t` |
| Item mouse/tablet handlers | `processPointerEvent(const PointerEvent&)` |
| Item live preview | `previewPixels()`; observe `BitmapPaintUpdate::dirtyBounds` |
| Item airbrush timer | Caller clock invoking `advanceTime(monotonicSeconds)` |
| Qt/QML registration and event policy hooks | Removed; window/input policy belongs to the application |

The exported target adds `cxx_std_23` to consumers. Its configuration does not find any additional package. CMake's build language is CXX only. The native installation replaces package-owned headers, including retired adapter headers.

## Session behavior

Create/open select a real bitmap file and discard previous file history/pending pixels only after success. The selected brush, color, and eraser mode survive file replacement. Failed operations report `lastError()` and preserve the current file.

A press captures the selected brush and color. Moves update preview immediately, touching only sample pixels. Release records a dirty-rectangle pixel patch and commits the preview. Cancel discards pending pixels without recording history or advancing wear. A changed brush/color/eraser selection affects subsequent strokes. Protected Texture selection uses the same policy as core `selectBrushPreset`; pass false to bypass it explicitly. Undo/redo/clear cancel unfinished painting and exchange raster patches. History does not rewind the selected brush's scalar wear.

Preview uses the same source-over/destination-out calculations as full bitmap composition. A sample with no accumulated coverage leaves the committed pixel unchanged, including RGB bytes beneath zero alpha. Duplicate sample positions are idempotent when copied from the same pending buffer.

`file()` is const so mutations that would bypass session history/preview cannot occur accidentally. Standalone pixel editing continues through `BitmapFile`. Saving during an unfinished stroke saves committed pixels; release first when the preview should be included. Session calls are synchronous; applications serialize calls to a given session. No GUI callbacks, threads, event queues, timers, or retained coordinate paths are introduced.

Pressure, tilt, stylus rotation, wheel/tangential pressure, and custom scalars travel through `PointerEvent`. Mouse input uses pressure 1. Tablet hover does not paint. `RasterViewport` and `ViewportTransform` remain standard C++ utilities for view/document/device coordinate conversion. The engine does not change OS input coalescing or supply a native display window.

## Codec migration

PNM decoding uses standard C++ and the [Netpbm specification](https://netpbm.sourceforge.net/doc/ppm.html), including Maxval normalization and big-endian 16-bit samples. The remaining codecs use pinned MIT stb, implemented in `BitmapCodec.cpp` as C++23. Internal `.hpp` and stb headers are private, while installed consumers see standard C++ types only. The fixed registry is identical on every platform that builds this source; codecs are not loaded from GUI plugins.

Read: BMP, GIF (first frame), HDR, JPEG, PIC, PNG, PNM (binary P5/P6), PSD (flattened composite), TGA. Write: BMP, HDR, JPEG, PNG, PNM (P6), TGA. TIFF, WebP, HEIF, JPEG 2000 and icon containers are unsupported in 0.4. EXIF orientation/ICC transforms and metadata preservation are not supplied; applications must normalize such input before painting if necessary. HDR/16-bit input is reduced to ARGB32 and writing HDR converts these normalized channels using gamma 2.2. PSD layer editing is not exposed through this codec.

Unrecognized/vector content or decoder-reported failures, excessive dimensions/pixel counts, invalid write quality, and unknown writers fail without replacing the current file object. A complete encoded image is written to an exclusively created sibling before replacement. The standard rename overwrite path is atomic on supported filesystems. Where overwrite is unavailable, the library backs up the old destination, attempts replacement, and restores on failure; this fallback is not atomic. No concurrent-writer locking or metadata retention is claimed.

## Build and installation scope

Use `cmake -S . -B build`, build and run CTest, then install. Additional verification builds live under `build/` as well. The source supports platform-neutral C++ APIs and no longer distributes Qt/mobile/GUI package machinery. A compiled host installation establishes operation on that host only; Windows, Linux, mobile, and WebAssembly require their own toolchain and runtime validation.

The pre-migration source/dirty-state snapshot and old installed package are retained in this checkout's `build/` directory. These historical backups are excluded from active sources and the new installation. Do not reintroduce them as library dependencies.

The local macOS package retains a native `platforms/macos` mirror for existing directory references. Its library, headers, and dependency-free configuration match the flat native package. Other platform binaries are not implied by this host installation. `BitmapFile` depends only on Core/Layer/Render pixel interfaces; stroke execution is composed in Application rather than imported by file I/O.
