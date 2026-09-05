# iiPaintEngine

iiPaintEngine은 C++/Qt/QML용 순수 비트맵 파일 페인팅 엔진이다. 작업 대상은 추상적인 화면 표면이 아니라 경로와 원본 형식이 결합된 `BitmapFile`이다. 입력 경로를 벡터 스트로크로
만들거나 저장하거나 재생하지 않는다.

## Bitmap-only contract

그리기 입력은 press, move, release 이벤트마다 즉시 비트맵으로 바뀐다.

```text
PointerEvent
→ current StrokePoint
→ RasterDabStream(previous point only)
→ BrushDab bitmap stamp
→ RasterSample pixels
→ StrokeCompositeBuffer
→ RasterLayer
```

`InputStrokeBuilder`는 좌표 목록을 보유하지 않고 현재 이벤트의 점 하나만 방출한다. `RasterDabStream`은 직전 점 하나와 spacing/random 누적 상태만 가진다. 전체 궤적,
curve, 원본 입력, replay command는 임시로도 만들지 않는다. `BrushDab`은 재편집 가능한 도형이 아니라 즉시 픽셀로 투영되는 일회성 비트맵 스탬프이다.

`configureHighFidelityPointerInput()`은 Qt의 고빈도·태블릿 이벤트 병합을 끄고 macOS에서는 AppKit의 mouse/drag/tablet coalescing도 끈다. 가능한 경우
`QGuiApplication` 생성 전에 호출한다. `registerIipeQmlTypes()`와 `BitmapFileItem`도 이 정책을 자동 적용하므로 지연 중 측정된 중간 좌표를 버리고 두 끝점만 직선으로 잇지 않는다.
live preview는 새 dab이 실제로 건드린 sample pixel만 복사하며, 멀리 떨어진 두 점을 감싸는 사각형 전체를 매 입력마다 훑지 않는다.

live preview는 pending bitmap buffer를 표시한 결과이다. release 시 그 픽셀 버퍼를 열린 `BitmapFile`의 ARGB 픽셀에 직접 합성한다. undo/redo는 raster
snapshot
또는 dirty-rect pixel patch를 사용한다. 저장 파일에는 현재 픽셀만 기록하며 포인터 궤적을 저장하지 않는다.

텍스트, SVG, 도형 같은 외부 콘텐츠는 iiPaintEngine에 넣기 전에 비트맵으로 변환해야 한다. 엔진은 raster paint layer만 소유한다.

## Architecture

파일 편집 소유 구조는 다음과 같다.

```text
BitmapFile
→ file path + detected source format
→ RasterLayer
→ ARGB pixels

BitmapFileItem
→ view/input translation only
→ BitmapFile pixel mutation API
```

`BitmapFile`이 파일 경로, 실제 바이트에서 감지한 형식, ARGB32 sRGB 픽셀, 수정 상태를 함께 소유한다. `BitmapFileItem`은 파일 픽셀을 소유하지 않으며 입력 좌표 변환과 화면 표시만
담당한다.
item의 width/height가 바뀌어도 파일 픽셀 크기는 바뀌지 않는다. 새 작업도 익명 표면 생성이 아니라 `createFile(path, width, height, format)`으로 실제 파일 대상을 먼저
만든다.

레이어 문서 archive가 필요한 C++ 흐름에서는 `PaintDocument`가 최종 합성 `DrawingSurface`와 `LayerStack`을 직접 소유한다. 이 흐름도 실제 콘텐츠는 픽셀뿐이며 파일 편집
경로와 별개의 화면 표면 모델을 만들지 않는다.

주요 모듈은 다음과 같다.

- `Core`: 좌표, rect, UUID, 오류, raster sample 값 타입
- `Input`: mouse/tablet event normalization과 현재 점 방출
- `Brush`: preset, dynamics, material, texture, wet/bristle 설정
- `Stroke/Rasterizer`: 직전 점에서 현재 점까지 bitmap dab 배치와 pixel projection
- `Layer`: raster surface, mask, blend, layer stack
- `Render`: CPU/GPU raster layer 합성, tile cache, brush stamp atlas
- `BitmapFile`: 런타임 bitmap format 감지/저장, 파일 경로, 직접 수정 가능한 ARGB 픽셀
- `Document`: 단일 bitmap surface, layer stack, format version 3 직렬화와 version 2 단일 표면 읽기 호환성
- `History`: raster patch/snapshot 기반 undo/redo 메타데이터
- `QtAdapter`: `BitmapFileItem` view/input adapter, 문서 adapter, layer list facade

Render cache에는 tile cache와 brush stamp atlas만 있다. 입력 경로를 다시 계산하는 stroke replay cache는 없다.

## C++ integration

설치된 소비자는 extensionless umbrella header 하나로 공개 API를 사용할 수 있다.

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

문서 레이어에 그릴 때는 이미 rasterized된 픽셀을 전달한다.

```cpp
PaintEngineController engine = makePaintEngineController(1024, 768);

std::vector<RasterSample> pixels{
    RasterSample{{100, 100}, 0xFF202020U},
    RasterSample{{101, 100}, 0xFF202020U},
};

commitPaintSamples(engine, pixels, {{100.0, 100.0}, 2.0, 1.0});
```

`commitPaintSamples`는 active layer의 픽셀을 갱신하고 raster paint history를 기록한다. 원본 좌표나 brush trajectory는 문서에 남기지 않는다.

## QML API

애플리케이션 시작 시 `registerIipeQmlTypes()`를 호출한 뒤 `iipe` 모듈을 가져온다.

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

viewport API는 `documentX`, `documentY`, `zoom`, `bitmapDevicePixelRatio`, `setDocumentViewport`, `resetView`, `panBy`,
`zoomAt`을 제공한다. 브러시 API는 color, size, spacing, flow, opacity, hardness, pressure curve와 각 enabled flag를 제공한다. 상태 API는
`liveStrokeActive`, `strokeCount`, `inputDevice`, `inputPressure`를 읽을 수 있다.

`BitmapFileItem`은 `createFile`, `openFile`, `save`, `saveAs`, `undo`, `redo`, `toolMode`, `brushConfig`,
`viewportConfig`, `runtimeConfig`,
`stateSnapshot`을 하나의 파일 중심 API로 제공한다. `filePath`, `fileFormat`, `fileOpen`, `modified`, `pixelWritable`,
`canSaveInPlace`, `bitmapWidth`,
`bitmapHeight`로 현재 파일 상태를 확인한다. `supportedOpenFormats`와 `supportedSaveFormats`는 현재 Qt 런타임의 실제 bitmap codec을 보고하며,
`supportedEditableFormats`는 읽기와 쓰기가 모두 가능한 교집합이다. 읽기 전용 형식도 픽셀 편집은 가능하며, 원래 형식 writer가 없으면 `saveAs`로 설치된 쓰기 형식을 선택한다.

`toolMode: "eraser"`는 pending bitmap alpha mask를 destination-out으로 합성한다.

## Bitmap file compatibility

`BitmapFile`은 Qt Gui에 이미 포함된 이미지 I/O 플러그인을 사용하므로 외부 의존성을 추가하지 않는다. 읽기는 파일 확장자보다 실제 바이트 형식을 우선 감지하고 EXIF 방향을 적용한 뒤
자체 `RasterLayer`의 ARGB32 sRGB 픽셀로 정규화한다. 저장은 객체에 보존된 감지 형식을 사용하므로 확장자가 잘못된 파일도 원래 bitmap 형식으로 다시 쓴다. `saveAs`는 명시 형식 또는
확장자를 사용하고 JPEG 같은 불투명 형식은 배경색에 합성하며, 모든 저장은 `QSaveFile`로 원자적으로 교체한다.

PNG, JPEG, BMP, WebP, TIFF, GIF, ICO/ICNS, HEIF, JPEG 2000 등은 해당 Qt 런타임에 코덱이 설치되어 있을 때만 노출한다. SVG, SVGZ, PDF 같은 벡터 입력은
플러그인이 설치되어 있어도 허용 목록에서 제외하며 디코더로 넘기지 않는다. 따라서 벡터 도형이나 경로를 임시 표현으로도 생성하지 않는다. 애니메이션 컨테이너는 첫 번째 비트맵 프레임만 읽는다.

## Document format

`DocumentArchive::formatVersion`과 `DocumentSerializer::formatVersion`은 3이다. version 3은 `PaintDocument`의 surface와 layer
stack을 직접 저장한다. version 2의 단일 표면 archive는 읽을 때 같은 비트맵 문서 구조로 이관하고, 다음 저장부터 version 3으로 기록한다. 여러 표면을 가진 레거시 archive와 더
높은 미래 버전은 일부 데이터만 취하는 대신 `compatible=false`와 오류를 반환하여 원본을 보존한다. archive는 다음 데이터를 왕복한다.

- document composite와 raster layer pixel surface
- layer metadata, mask, children, blend/opacity
- brush source preset
- color space와 ICC profile
- embedded asset
- raster command history metadata와 pixel patch payload

archive는 전체 입력점, 곡선, dab sequence 또는 replay 가능한 stroke를 포함하지 않는다.

## Build and example

빌드 디렉터리는 항상 `build/`이다.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

예제 실행 파일은 소스 트리가 아니라 다음 build-tree 경로에 생성된다.

```text
build/Example/bin/iiPaintEngineExample
```

`Example/Main.qml`은 LVRS control과 `Iipe.BitmapFile`을 사용하며 임시 PNG 파일을 실제 작업 대상으로 생성한 뒤 size, flow, opacity, hardness,
spacing,
pressure curve, live preview, clear/reset view를 공개 API로 검증한다.
예제의 slider 범위는 size 2–72 px, flow/opacity/hardness 0.05–1.0, spacing 0–1.0이며 `iiPaintEngineExampleDemoContract`가 이 QML
계약을 검증한다.

## Install

기본 설치 prefix는 `~/.local/SDK/iiPaintEngine`이다. Unix 계열에서는 다음을 실행한다.

```sh
./install.sh
IIPAINTENGINE_INSTALL_PLATFORMS=macos ./install.sh
```

macOS의 기본 전체 설치는 Homebrew Android SDK/NDK도 자동 탐지하며, 탐지한 NDK toolchain을 Qt Android 체인로드 경로로 명시해 오래된 Qt cache 경로를 사용하지
않는다.

Windows PowerShell에서는 다음을 실행한다.

```powershell
.\install.ps1

$env:IIPAINTENGINE_INSTALL_PLATFORMS = "windows"
.\install.ps1
```

Windows host 빌드는 선택한 Qt kit과 일치하는 Qt MinGW 13.1.0과 Ninja 조합을 고정해 사용한다. Visual Studio generator와 MinGW Qt binary를 섞지 않는다.
설치 스크립트는 matching compiler와 Ninja 경로를 CMake에 명시한다.

host package와 플랫폼 mirror는 다음에 설치된다.

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

설치 스크립트는 shared library, headers, CMake package config, license를 함께 배치하고 host 테스트를 실행한다. 단일 구성 generator도 Release로
구성하므로 설치된 `iiPaintEngine::iiPaintEngine` target에는 소비자가 링크할 수 있는 `IMPORTED_LOCATION_RELEASE`가 포함된다. 선택적 cross SDK가 없는
기본 all-platform 설치는 해당 플랫폼을 건너뛰며, 플랫폼을 명시 요청했는데 toolchain이 없으면 실패한다.
업그레이드 설치는 package가 소유하는 `include/iiPaintEngine` 트리를 현재 공개 헤더로 교체한다. 따라서 삭제된 API 헤더가 prefix에 남아 새 타입과 충돌하지
않으며, 소비자 전용 헤더는 이 package 소유 디렉터리 밖에 둬야 한다.
macOS package config는 현재 SDK에 binary가 없는 legacy AGL framework를 Qt link interface에서 제거하므로 설치 target을 링크하는 소비자도 최신 macOS
SDK에서 빌드할 수 있다. 또한 compiler의 `LIBRARY_PATH`에 설치 경로가 있어 CMake가 이를 implicit link directory로 판단하더라도 package target이
iiPaintEngine dylib의 runtime search path를 직접 전달한다.
Apple shared library는 처음부터 install RPATH로 빌드하므로 같은 prefix에 반복 설치해도 이미 제거된 build RPATH를 다시 후처리하지 않는다.
Qt WASM은 정적 Qt SDK를 사용하므로 WASM 패키지는 `libiiPaintEngine.a` static archive로
설치한다. 네이티브 플랫폼은 shared library를 유지하며, WASM 정적 archive는 다른 설치
라이브러리와 한 최종 실행 파일에 결합해도 Qt 심볼을 중복 포함하지 않는다. 설치된 CMake
target은 Qt WASM 플랫폼 플러그인에 필요한 Emscripten embind 링크 옵션도 소비자에게 전파한다.
macOS에서는 `~/Library/Android/sdk` 외에 Homebrew의
`/opt/homebrew/share/android-commandlinetools`와 `/opt/homebrew/share/android-ndk`도
자동으로 탐지한다.

Windows의 layout-contract 테스트 실행 파일은 `iiPaintEngineLayoutContractTests`라는 이름을 쓴다. `Install...`로 시작하는 실행 파일에 적용될 수 있는
Windows installer detection heuristic 및 elevation 오탐을 피하기 위한 계약이다.

## Verification contracts

핵심 검증은 다음과 같다.

- `iiPaintEngineBitmapOnlyArchitectureContract`: 금지된 경로/레이어 소스 부재, 문서의 retained stroke 부재, input point collection 부재,
  이벤트별 pixel 누적
- `iiPaintEngineBitmapFileCompatibilityContract`: 파일 계층의 path/format/pixel 소유, 직접 pixel mutation, 런타임 bitmap codec 전체 쓰기
  왕복, content sniffing, SVG/PDF 차단
- `iiPaintEngineBitmapFileArchitectureContract`: 구형 화면 표면 API 부재, `BitmapFile`과 `BitmapFileItem` 공개 계약
- `iiPaintEngineInstallUpgradeContract`: 재설치 시 삭제된 공개 헤더 제거, 현재 헤더 배치, 설치 package를 사용하는 별도 CMake 소비자의
  `find_package`·link·load
- `iiPaintEnginePipelineHeartbeat`: pointer event부터 document raster surface까지 최소 파이프라인
- `iiPaintEngineDocumentSerializerContract`: format version 3 bitmap archive 왕복, version 2 단일 표면 이관, raw trajectory 부재
- `iiPaintEngineAppDocumentApiContract`: raster sample commit, layer/history, save/open 왕복
- `iiPaintEnginePointerStrokeFlow`: mouse event별 bitmap dab spacing/flow
- `iiPaintEngineHighFidelityPointerInputContract`: Qt·macOS pointer event 병합 비활성화와 `BitmapFileItem` 자동 적용
- `iiPaintEngineRasterPaintingModel`: velocity, pressure, tilt, spacing, opacity/flow의 bitmap dab 반영
- `iiPaintEngineBitmapFileLivePreviewRealtimeContract`: 열린 파일의 pending raster preview, direct pixel commit, eraser,
  undo/redo
- `iiPaintEngineBrushDynamicsMapping`: pressure/velocity/tilt/random의 dab 속성 반영
- `iiPaintEngineBrushTextureLayerContract`: document/tip/follow/paper bitmap texture와 dual brush
- `iiPaintEngineWetBrushSimulationContract`: source raster sampling, pickup/deposit/mix, bristle footprint
- `iiPaintEngineRenderCacheBackendContract`: raster tile cache, brush stamp atlas, CPU/GPU 계획
- `iiPaintEnginePublicUmbrellaHeaderContract`: 외부 소비자의 단일 `#include <iiPaintEngine>` 계약
- `iiPaintEnginePublicCxx17HeaderContract`: C++17 공개 header 호환성
- `iiPaintEngineInstallLayoutContract`: Unix/Windows install, package export, example output, license 계약

정적 확인 시 제품 소스에서 전체 입력 collection, curve/path 모델, replay cache, 비트맵 외 레이어가 다시 생기지 않았는지 함께 검사한다.

## License

SPDX-License-Identifier: AGPL-3.0-only

iiPaintEngine의 자체 작성 코드와 문서는 GNU Affero General Public License version 3 only
(`AGPL-3.0-only`)로 배포된다. 전체 조건은 [LICENSE](LICENSE)를 따른다.

외부에서 제공하는 Qt, LVRS 및 그 밖의 서드파티 코드·라이브러리·도구·모델은 각각의
라이선스와 저작권 고지를 유지하며, 이 저장소의 라이선스가 이를 대체하지 않는다.

### SDK workspace and installation paths

The source checkout is `Workspace/SDK/iiPaintEngine`. Both `./install.sh` and a
fresh direct CMake configuration default to `~/.local/SDK/iiPaintEngine`.
An explicit `-DCMAKE_INSTALL_PREFIX` remains authoritative for direct CMake
configuration. Build outputs stay in the repository's `build/` directory; the
installer regenerates stale CMake caches after a workspace move.

The default LVRS dependency prefix is `~/.local/SDK/LVRS`;
`IIPAINTENGINE_LVRS_PREFIX` remains available for an explicit override.
