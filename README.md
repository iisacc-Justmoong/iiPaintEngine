# iiPaintEngine

iiPaintEngine은 C++/Qt/QML용 순수 비트맵 페인팅 엔진이다. 캔버스의 기준 데이터는 ARGB 픽셀이며, 입력 경로를 벡터 스트로크로 만들거나 저장하거나 재생하지 않는다.

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

live preview는 pending bitmap buffer를 그린 결과이다. release 시 그 픽셀 버퍼를 active raster layer에 합성한다. undo/redo는 raster snapshot
또는 dirty-rect pixel patch를 사용한다. 저장 파일은 레이어 픽셀을 저장하며 포인터 궤적을 저장하지 않는다.

텍스트, SVG, 도형 같은 외부 콘텐츠는 iiPaintEngine에 넣기 전에 비트맵으로 변환해야 한다. 엔진은 raster paint layer만 소유한다.

## Architecture

문서 소유 구조는 다음과 같다.

```text
PaintDocument
→ LayerStack
→ Layer
→ DrawingSurface
→ ARGB pixels
```

`PaintDocument`가 최종 합성 `DrawingSurface`와 `LayerStack`을 직접 소유한다. 별도 `Canvas` 도메인 타입이나 `Canvas/` 모듈은 없다. QML의
`Iipe.Canvas`는 비트맵 표면을 화면에 표시하고 입력을 전달하는 Qt UI 타입일 뿐 문서 모델이 아니다.

주요 모듈은 다음과 같다.

- `Core`: 좌표, rect, UUID, 오류, raster sample 값 타입
- `Input`: mouse/tablet event normalization과 현재 점 방출
- `Brush`: preset, dynamics, material, texture, wet/bristle 설정
- `Stroke/Rasterizer`: 직전 점에서 현재 점까지 bitmap dab 배치와 pixel projection
- `Layer`: raster surface, mask, blend, layer stack
- `Render`: CPU/GPU raster layer 합성, tile cache, brush stamp atlas
- `Document`: 단일 bitmap surface, layer stack, format version 3 직렬화와 version 2 단일 캔버스 읽기 호환성
- `History`: raster patch/snapshot 기반 undo/redo 메타데이터
- `QtAdapter`: QML canvas, 문서 adapter, layer list facade

Render cache에는 tile cache와 brush stamp atlas만 있다. 입력 경로를 다시 계산하는 stroke replay cache는 없다.

## C++ integration

설치된 소비자는 extensionless umbrella header 하나로 공개 API를 사용할 수 있다.

```cpp
#include <iiPaintEngine>
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
import iipe 1.0 as Iipe

Iipe.Canvas {
    id: canvas
    anchors.fill: parent

    documentX: 0
    documentY: 0
    zoom: 1
    canvasDevicePixelRatio: 1

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
}
```

viewport API는 `documentX`, `documentY`, `zoom`, `canvasDevicePixelRatio`, `setDocumentViewport`, `resetView`, `panBy`,
`zoomAt`을 제공한다. 브러시 API는 color, size, spacing, flow, opacity, hardness, pressure curve와 각 enabled flag를 제공한다. 상태 API는
`liveStrokeActive`, `strokeCount`, `inputDevice`, `inputPressure`를 읽을 수 있다.

`CanvasAdapter`는 같은 bitmap surface 위에 `newCanvas`, `openRaster`, `saveToFile`, `saveToFileAs`, `undo`, `redo`,
`toolMode`, `brushConfig`, `viewportConfig`, `runtimeConfig`, `stateSnapshot`을 제공한다. `supportedOpenFormats`와
`supportedSaveFormats`는 현재 Qt 런타임에 실제 설치된 래스터 코덱만 보고하며, `lastFileError`는 최근 열기·저장 실패 원인을 제공한다. `runtimeConfig`는 live
preview on/off만 가진다. smoothing이나 frame-batched path 처리를 위한 설정은 없다.

```qml
Iipe.CanvasAdapter {
    anchors.fill: parent
    toolMode: "brush"
    Component.onCompleted: newCanvas(1024, 768)
}
```

`toolMode: "eraser"`는 pending bitmap alpha mask를 destination-out으로 합성한다.

## Bitmap file compatibility

`BitmapFileCodec`는 Qt Gui에 이미 포함된 이미지 I/O 플러그인을 사용하므로 외부 의존성을 추가하지 않는다. 읽기는 파일 확장자보다 실제 바이트 형식을 우선 감지하고 EXIF 방향을 적용한 뒤
ARGB32 sRGB 픽셀로 정규화한다. 저장은 명시 형식 또는 확장자를 사용하고 JPEG 같은 불투명 형식은 배경색에 합성하며, `QSaveFile`로 원자적으로 교체한다.

PNG, JPEG, BMP, WebP, TIFF, GIF, ICO/ICNS, HEIF, JPEG 2000 등은 해당 Qt 런타임에 코덱이 설치되어 있을 때만 노출한다. SVG, SVGZ, PDF 같은 벡터 입력은
플러그인이 설치되어 있어도 허용 목록에서 제외하며 디코더로 넘기지 않는다. 따라서 벡터 도형이나 경로를 임시 표현으로도 생성하지 않는다. 애니메이션 컨테이너는 첫 번째 비트맵 프레임만 읽는다.

## Document format

`DocumentArchive::formatVersion`과 `DocumentSerializer::formatVersion`은 3이다. version 3은 `PaintDocument`의 surface와 layer
stack을 직접 저장한다. version 2의 단일 캔버스 archive는 읽을 때 같은 비트맵 문서 구조로 이관하고, 다음 저장부터 version 3으로 기록한다. 여러 캔버스를 가진 레거시 archive와 더
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

`Example/Main.qml`은 LVRS control과 `Iipe.Canvas`를 사용하며 size, flow, opacity, hardness, spacing, pressure curve, live
preview, clear/reset view를 실제 공개 API로 검증한다.

## Install

기본 설치 prefix는 `~/.local/iiPaintEngine`이다. Unix 계열에서는 다음을 실행한다.

```sh
./install.sh
IIPAINTENGINE_INSTALL_PLATFORMS=macos ./install.sh
```

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
~/.local/iiPaintEngine/
~/.local/iiPaintEngine/platforms/macos/
~/.local/iiPaintEngine/platforms/linux/
~/.local/iiPaintEngine/platforms/windows/
~/.local/iiPaintEngine/platforms/ios/
~/.local/iiPaintEngine/platforms/android/
~/.local/iiPaintEngine/platforms/wasm/
```

설치 스크립트는 shared library, headers, CMake package config, license를 함께 배치하고 host 테스트를 실행한다. 선택적 cross SDK가 없는 기본
all-platform 설치는 해당 플랫폼을 건너뛰며, 플랫폼을 명시 요청했는데 toolchain이 없으면 실패한다.

Windows의 layout-contract 테스트 실행 파일은 `iiPaintEngineLayoutContractTests`라는 이름을 쓴다. `Install...`로 시작하는 실행 파일에 적용될 수 있는
Windows installer detection heuristic 및 elevation 오탐을 피하기 위한 계약이다.

## Verification contracts

핵심 검증은 다음과 같다.

- `iiPaintEngineBitmapOnlyArchitectureContract`: 금지된 경로/레이어 소스 부재, 문서의 retained stroke 부재, input point collection 부재,
  이벤트별 pixel 누적
- `iiPaintEngineBitmapFileCompatibilityContract`: 런타임 래스터 코덱 필터, content sniffing, PNG/JPEG/BMP/WebP/TIFF 왕복, SVG/PDF 차단
- `iiPaintEngineCanvasModuleRemovalContract`: `Canvas/` 부재, direct bitmap document ownership, 독립 viewport transform
- `iiPaintEnginePipelineHeartbeat`: pointer event부터 document raster surface까지 최소 파이프라인
- `iiPaintEngineDocumentSerializerContract`: format version 3 bitmap archive 왕복, version 2 단일 캔버스 이관, raw trajectory 부재
- `iiPaintEngineAppDocumentApiContract`: raster sample commit, layer/history, save/open 왕복
- `iiPaintEnginePointerStrokeFlow`: mouse event별 bitmap dab spacing/flow
- `iiPaintEngineRasterPaintingModel`: velocity, pressure, tilt, spacing, opacity/flow의 bitmap dab 반영
- `iiPaintEngineCanvasLivePreviewRealtimeContract`: pending raster preview, commit, eraser, undo/redo
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

iiPaintEngine은 GNU Affero General Public License v3 전용으로 배포된다. 전체 조건은 `LICENSE`를 따른다.
