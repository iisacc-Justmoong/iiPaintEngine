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

The exported target adds `cxx_std_23` to consumers. Its configuration finds the existing Qt-free iiFileProviderNative >=0.6 package for file ownership. CMake's build language is CXX only. The native installation replaces package-owned headers, including retired adapter headers.

## Session behavior

Create/open select a real bitmap file and discard previous file history/pending pixels only after success. The selected brush, color, and eraser mode survive file replacement. Failed operations report `lastError()` and preserve the current file.

A press captures the selected brush and color. Moves update preview immediately, touching only sample pixels. Release records a dirty-rectangle pixel patch and commits the preview. Cancel discards pending pixels without recording history or advancing wear. A changed brush/color/eraser selection affects subsequent strokes. Protected Texture selection uses the same policy as core `selectBrushPreset`; pass false to bypass it explicitly. Undo/redo/clear cancel unfinished painting and exchange raster patches. History does not rewind the selected brush's scalar wear.

Preview uses the same source-over/destination-out calculations as full bitmap composition. A sample with no accumulated coverage leaves the committed pixel unchanged, including RGB bytes beneath zero alpha. Duplicate sample positions are idempotent when copied from the same pending buffer.

`file()` is const so mutations that would bypass session history/preview cannot occur accidentally. Standalone pixel editing continues through `BitmapFile`. Saving during an unfinished stroke saves committed pixels; release first when the preview should be included. Session calls are synchronous; applications serialize calls to a given session. No GUI callbacks, threads, event queues, timers, or retained coordinate paths are introduced.

Pressure, tilt, stylus rotation, wheel/tangential pressure, and custom scalars travel through `PointerEvent`. Mouse input uses pressure 1. Tablet hover does not paint. `RasterViewport` and `ViewportTransform` remain standard C++ utilities for view/document/device coordinate conversion. The engine does not change OS input coalescing or supply a native display window.

## Codec migration

PNM decoding uses standard C++ and the [Netpbm specification](https://netpbm.sourceforge.net/doc/ppm.html), including Maxval normalization and big-endian 16-bit samples. The remaining codecs use pinned MIT stb, implemented in `BitmapCodec.cpp` as C++23. Internal `.hpp` and stb headers are private, while installed consumers see standard C++ types only. The fixed registry is identical on every platform that builds this source; codecs are not loaded from GUI plugins.

Read: BMP, GIF (first frame), HDR, JPEG, PIC, PNG, PNM (binary P5/P6), PSD (flattened composite), TGA. Write: BMP, HDR, JPEG, PNG, PNM (P6), TGA. TIFF, WebP, HEIF, JPEG 2000 and icon containers are unsupported in 0.4. EXIF orientation/ICC transforms and metadata preservation are not supplied; applications must normalize such input before painting if necessary. HDR/16-bit input is reduced to ARGB32 and writing HDR converts these normalized channels using gamma 2.2. PSD layer editing is not exposed through this codec.

HDR encoding indexes a 256-entry table of the original `float(pow(channel / 255.0, 2.2))`
values. It reuses the table in round-to-nearest mode and computes the table in the
current mode for other rounding settings. Alpha flattening writes directly to the
linear float buffer passed to the HDR writer. The PNM writer places flattened RGB
directly after its header in the encoded output buffer.

The pinned stb decoder returns a temporary RGBA buffer. The codec converts it
into the `RasterLayer` ARGB32 vector with a byte shuffle on eligible ARM64 NEON
builds, a vectorizable word loop on other little-endian builds, and explicit byte
assembly on other targets. Both buffers coexist during conversion. stb selects
SSE2 on eligible x86 builds. The codec enables NEON
when the target is little-endian ARM and the compiler supplies the NEON flag and
intrinsic header. Other targets use the stb scalar kernels. A build can select
these kernels explicitly by defining `STBI_NO_SIMD` for the codec translation unit.

`BitmapCodecOptimization` checks all 256 HDR table entries against the original
float conversion, compares RGBE bytes, and counts the HDR RGB staging allocation.
It also verifies exact ARGB round trips for PNG, BMP, and TGA and identical JPEG
decode pixels against pinned scalar stb for 4:2:0 and 4:4:4 inputs.

Unrecognized/vector content or decoder-reported failures, excessive dimensions/pixel counts, invalid write quality, and unknown writers fail without replacing the current file object. A complete encoded image is written to an exclusively created sibling before replacement. The standard rename overwrite path is atomic on supported filesystems. Where overwrite is unavailable, the library backs up the old destination, attempts replacement, and restores on failure; this fallback is not atomic. No concurrent-writer locking or metadata retention is claimed.

## Build and installation scope

Use `cmake -S . -B build`, build and run CTest, then install. Additional verification builds live under `build/` as well. The source supports platform-neutral C++ APIs and no longer distributes Qt/mobile/GUI package machinery. A compiled host installation establishes operation on that host only; Windows, Linux, mobile, and WebAssembly require their own toolchain and runtime validation.

The pre-migration source/dirty-state snapshot and old installed package are retained in this checkout's `build/` directory. These historical backups are excluded from active sources and the new installation. Do not reintroduce them as library dependencies.

The local macOS package retains a native `platforms/macos` mirror for existing directory references. Its library, headers, and native configuration match the flat native package. Other platform binaries are not implied by this host installation. `BitmapFile` depends only on Core/Layer/Render pixel interfaces; stroke execution is composed in Application rather than imported by file I/O.

## 0.5 brush extension

Public brush and stream layouts gained symmetry and pulled-string/catch-up settings; rebuild consumers against iiPaintEngine 0.5. Preset format 4 preserves them and reads formats 1/2/3 with neutral defaults. Preset versions are independent of document archive versions. See [Brush expansion](BRUSH_EXPANSION.md) for the bounded ABR reader, model semantics and current compatibility limits.

## Document archive read limits

`deserializeDocumentArchive()` applies the default limits to formats 3 and 4 and compatible legacy single-surface archives. `deserializeDocumentArchiveWithLimits()` and `deserializeDocumentArchiveView()` accept `DocumentArchiveReadLimits` through `<iiPaintEngine>`. Callers can lower individual limits, and values above the defaults are capped at those defaults. A layer at the root has depth 1.

| Limit | Maximum per archive |
| --- | ---: |
| `maxLayers` | 10,000 layers including descendants |
| `maxLayerDepth` | 64 levels |
| `maxDirtyRects` | 1,000,000 rectangles across document and layer surfaces |
| `maxHistoryCommands` | 100,000 commands across undo and redo |
| `maxHistoryPatches` | 1,000,000 patches across undo and redo |
| `maxBrushSources` | 10,000 brush sources |
| `maxColorSpaces` | 1,024 color spaces |
| `maxAssets` | 100,000 assets |

Present collection counts must contain only unsigned decimal digits and fit their remaining budget. A missing count denotes an empty collection. Every declared index must have its own field from the archive schema before its collection is reserved. For example, a layer needs its `surface.width` and a history patch needs its `targetId`. Optional fields still use their existing defaults. Legacy text readers accept LF and CRLF line endings.

Collection traversal and allocation are proportional to the actual records. The hash index references the input buffer and uses O(K) entries for K distinct keys. Binary chunk bodies are skipped by length and the expected lookup work is linear in the sum of key lengths. Pixel, mask and asset storage is sized from the encoded data present in the archive.

Malformed counts, absent records, excessive depth and invalid chunk framing return `compatible = false` with a diagnostic in `compatibilityError`. Length and allocation failures during parsing also return an incompatible archive. Failed reads discard all partially decoded data.

`iiPaintEngineDocumentSerializerContract` checks that small malformed inputs are rejected in under 50 ms per call. It also checks the depth boundary, shared budgets, legacy migration and byte-identical serialization after reopening a document with layers, brush data, assets and undo/redo history.

## 문서 아카이브 버전 4

파일은 `iiPaintDocumentBinary\n` 서명으로 시작한다. 텍스트 레코드는 `key\tvalue\n`을
사용한다. 인용 문자열은 인용부호와 역슬래시를 이스케이프하며 줄바꿈과 캐리지 리턴을
각각 `\n`과 `\r`로 저장한다. 마지막의 `formatVersion\t4\n`이 버전을 지정한다.

바이너리 레코드는 `@key\tbyteCount\n` 뒤에 정확히 `byteCount` 바이트와 LF 한 바이트를
둔다. 길이는 부호 없는 10진수이며 0도 허용한다. 본문에 NUL, 탭 또는 LF가 포함돼도
레코드 경계는 길이로 결정된다. 표면 픽셀과 썸네일은 32비트 ARGB 값을 리틀엔디언으로
저장한다. 마스크, 프리셋, ICC 프로필, 에셋과 이력의 바이트 배열은 원래 바이트를 보존한다.

`serializeDocumentArchive()`는 최종 크기를 먼저 계산하고 문자열 버퍼를 한 번 예약한다.
`serializeDocumentArchiveTo()`는 한 패스로 호출자가 제공한 스트림에 기록한다. 파일
스트림은 바이너리 모드로 열어야 한다. 기록이 끝나기 전에 실패하면 `false`를 반환하며,
부분 출력과 원자적 게시 여부는 스트림을 제공한 호출자가 관리한다.

`deserializeDocumentArchiveView()`는 호출 중 입력을 빌린다. 복원된 문서는 픽셀과 바이트
배열을 자체 벡터로 소유한다. 기존 문자열 API도 이 경로를 사용한다. 새 SDK는 버전 3의
16진수 데이터와 호환 가능한 단일 표면 문서를 읽으며, 다시 저장하면 버전 4를 출력한다.
버전 4 파일은 이를 지원하는 SDK나 애플리케이션에서 연다.

## 16진수 필드와 표면 크기 검증

기존 아카이브의 16진수 배열은 필드 타입에 맞는 바이트 또는 32비트 워드 단위로 끝나야
한다. `0–9`, `A–F`, `a–f`를 읽으며 잘못된 문자와 잘린 단위를 거부한다. 아카이브에
명시된 UUID는 버전에 관계없이 정확히 32자리의 16진수여야 한다. 생략한 UUID는 기본값을
사용한다.

문서와 레이어 표면의 가로와 세로는 각각 `Types::Pixel` 범위의 부호 없는 10진수로 읽는다.
생략한 치수의 기본값은 0이다. 두 치수의 곱을 64비트로 계산하고, 16진수 데이터나 바이너리
청크에서 확인한 픽셀 개수와 비교한 뒤 버퍼를 할당한다. 픽셀 레코드를 생략한 표면은
면적이 0일 때 유효하다.

검증에 실패하면 `compatible = false`와 문제 필드 경로를 담은 `compatibilityError`를
반환한다. 픽셀 수가 다른 경우 진단에 기대 개수와 실제 개수가 포함된다.
