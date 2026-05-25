# iiPaintEngine

`iiPaintEngine`은 Qt 기반 페인트 엔진 라이브러리이다. Qt 화면 객체인 `PaintCanvasItem`만 `QObject`/`QQuickPaintedItem` 기반으로 두고, 나머지는 값 복사와
aggregate 초기화를 지원하는 단순 `struct` 청사진으로 둔다. 엔진 본체는 가능한 한 순수 C++ 데이터와 알고리즘으로 유지한다.

## 모듈 청사진

- Core: `EngineConfig`, `EngineError`, `PaintUuid`, `PaintPoint`, `PaintRect`, `CoordinateSpace`, `RasterSample`,
  `RasterBlendMode`
- Document: `PaintDocument`, `DocumentMetadata`, `DocumentSnapshot`, `DocumentSerializer`
- Canvas: `Canvas`, `CanvasMetadata`, `CanvasState`, `CanvasViewport`, `CanvasSession`
- Layer: `DrawingSurface`, `Layer`, `LayerMetadata`, `LayerStack`, `RasterLayer`, `StrokeCompositeBuffer`,
  `PremultipliedPixel`,
  `StrokeLayer`, `TextLayer`, `VectorLayer`
- Stroke: `Stroke`, `StrokePoint`, `StrokeInput`, `Stabilizer`, `StrokeCurve`, `StrokeResampler`, `Rasterizer`,
  `BrushDab`, `StrokePath`, `BrushState`, `StrokeCommand`, `StrokeRepository`, `LiveStrokeFrame`, `LiveStrokeBuffer`
- Brush: `BrushPreset`, `BrushDynamics`, `BrushDynamicsInput`, `BrushDynamicsResult`, `BrushShape`, `BrushTip`,
  `BrushLibrary`, `BrushSnapshot`, `BrushResolve`
- Render: `Renderer`, `RenderContext`, `DirtyRegion`, `Compositor`, `CpuRenderer`, `GpuRenderer`
- History: `Command`, `HistoryStack`, `UndoRedoController`, `HistorySnapshot`
- Input: `PointerEvent`, `TabletState`, `InputNormalizer`, `InputStrokeBuilder`
- Color: `PaintColor`, `Palette`, `ColorSpace`, `Gradient`
- QtAdapter: `PaintCanvasItem`, `CanvasEventWork`, `registerIipeQmlTypes`, `PaintEngineController`, `DocumentAdapter`,
  `LayerListModel`

`PaintCanvasItem`은 QML/QQuickItem 연동을 위한 마우스 입력 경계와 화면 페인트 경계를 제공한다. 그 외 모든 객체는 생성자를 제공하지 않고 공개 필드만 유지하는 경량 값 타입으로 시작한다.

## 의존 방향

허용되는 의존 방향은 아래와 같다.

```text
QtAdapter
-> Document / Canvas / Input
Document
-> Canvas
Canvas
-> Layer / Stroke
Layer / Stroke / Brush / Render / History / Color
-> Core
```

`Core`는 최하층이며 어떤 프로젝트 모듈에도 의존하지 않는다. `Layer`, `Brush`, `Render`, `History`, `Color`는 `Core` 타입을 통해 최소한의 식별자와 좌표만 공유한다.
`Stroke`는 브러시 파라미터를 dab 명령으로 해석하기 위해 `BrushDynamics` 값 타입만 읽을 수 있다. `Canvas`는 `LayerStack`과 `StrokeRepository`를 묶는 작업
공간이며, `Document`는 하나 이상의 `Canvas`와 문서 메타데이터를 소유한다. `QObject`, `QQuickItem`, QML 관련 include는 `QtAdapter`에만 둔다.

`Input`은 포인터 이벤트를 `StrokeInput` 값으로 바꾸기 위해 `Stroke`의 순수 값 타입만 사용할 수 있다. `Input`은 Qt, QML, 브러시, 래스터 레이어를 알 수 없으며, 장치별 이벤트
해석과 스트로크 단위의 벡터 입력 구성까지만 맡는다.

## Core 계약

Core는 모든 상위 계층이 공유하는 최하층 타입만 가진다.

- `Types`: `Scalar`, `Pixel`, `Byte` 별칭을 제공한다.
- `PaintUuid`: 16바이트 순수 C++ UUID 저장소이다.
- `CoordinateSpace`: `Document`, `Canvas`, `View`, `DevicePixel` 좌표계를 구분한다. `CanvasPoint`/`CanvasRect`는 초기 코드 호환용 별칭이며
  현재 stroke 원본의 기준 좌표는 `DocumentPoint`/`DocumentRect`이다.
- `PaintPoint<Space>` / `PaintRect<Space>`: 좌표계 marker를 템플릿 인자로 받아 서로 다른 좌표계끼리 대입되지 않는다.
- `DocumentPoint`, `CanvasPoint`, `ViewPoint`, `DevicePixelPoint`: 문서 좌표, 호환 캔버스 좌표, 뷰 좌표, 장치 픽셀 좌표의 명시적 별칭이다.
- `DocumentRect`, `CanvasRect`, `ViewRect`, `DevicePixelRect`: 좌표계별 사각형 별칭이다.
- `RasterSample`: 래스터라이저가 만든 장치 픽셀 좌표, ARGB 색상, 스트로크 opacity 상한을 가진 한 점이다.
- `EngineError`: 에러 코드와 선택적 메시지 포인터만 가진 경량 값 타입이다.
- `EngineConfig`: 기본 DPI와 device pixel ratio만 가진 최소 설정 값 타입이다.

장치 픽셀 좌표는 `Types::Pixel` 정수 좌표를 사용하고, 캔버스/뷰 좌표는 `Types::Scalar` 실수 좌표를 사용한다.

## 첫 파이프라인

현재 엔진의 최소 동작 파이프라인은 아래와 같다.

```text
Mouse PointerEvent
-> InputStrokeBuilder
-> StrokeInput
-> LiveStrokeBuffer / StrokeCommand
-> Stabilizer
-> StrokeResampler
-> StrokeCurve
-> BrushDab
-> RasterSample
-> StrokeCompositeBuffer
-> RasterLayer
-> DrawingSurface
-> LayerStack
-> Canvas
-> PaintDocument
```

상위 문서 구조는 `PaintDocument -> Canvas -> LayerStack -> Layer -> DrawingSurface`이다. `Canvas`는 사용자가 바라보는 작업 공간이고,
`DrawingSurface`는 width, height, pixel format, color space, DPI, backing store, dirty region, texture handle 같은 실제 렌더
가능한 물리 표면이다. `DrawingSurface`는 레이어 목록을 갖지 않는다. 레이어 편집 구조는 `LayerStack`과 `Layer`가 맡고, 레이어 공통 정보는
`LayerMetadata`가 맡는다. stroke 원본 및 command 보존은 `StrokeRepository`가 맡는다.

메타데이터는 문서, 캔버스, 레이어 단위로 나눈다. `DocumentMetadata`는 제목, 작성자, 저장 경로, 버전, document id 같은 문서 전체 정보를 담고,
`CanvasMetadata`는 배경색, 단위, 의도한 export 크기, DPI, 색공간, thumbnail 같은 캔버스 표현 정보를 담는다. `LayerMetadata`는 layer id,
name, visible, opacity, blend mode 같은 레이어 편집 공통 정보를 담는다.

`DocumentArchive`는 저장 포맷의 값 계약이다. `PaintDocument` 본문, brush source snapshot, color space profile, 외부 asset, history
stack을 한 묶음으로
보관한다. `serializeDocumentArchive`/`deserializeDocumentArchive`는 이 archive를 key/value payload로 왕복시켜 레이어 표면, layer
metadata, stroke command,
brush state, brush source mask, ICC profile bytes, asset bytes, history command가 파일 컨테이너 구현 전에 먼저 보존되는지 고정한다.
모듈 경계에서는 `PaintDocument` 자체가 여전히 `Canvas`만 소유하고, `DocumentSerializer`만 저장 포맷 경계로서 `Brush`, `Color`, `History`의
persistent value를
읽을 수 있다.

초기 브러시는 `Rasterizer`의 기본값인 검은색 원형 브러시를 사용한다. 브러시 알파 이미지가 지정되면 `Rasterizer`는 먼저 `StrokeCurve`의 벡터 구간을 spacing/density
간격으로 순회하여 `BrushDab` 명령 시퀀스를 만든다. 각 dab은 position, scale, rotation, alpha, color, blendMode를 가진 작은 브러시 투영 명령이다. 그 다음 dab
위치에 브러시 알파 이미지를 변환하여 `RasterSample`로 투영한다.

브러시 알파 이미지는 dab의 scale, ellipse scale, rotation을 역변환한 뒤 source mask를 bilinear sampling으로 읽는다. 정수 픽셀에 nearest로 찍지 않고
subpixel 위치를 보존하며, 축소된 dab은 픽셀 영역을 샘플링해 작은 브러시가 격자 사이에서 사라지지 않도록 한다. `Rasterizer::hardness`는 bilinear mask alpha에 적용되는
커브이며, 낮을수록 soft mask 가장자리가 더 부드럽게 감쇠한다.

`StrokeInput`은 단순 점 목록이 아니라 시간, 압력, 속도, 기울기를 가진 샘플 시퀀스로 보존된다. `makeStrokeCurve`는 샘플의 시간 차이와 이동 거리로 velocity를 계산한다. dab의
scale은 pressure, rotation은 tilt 또는 곡선 접선, 간격은 spacing/density와 velocity spacing 계수, alpha는 flow에 의해 결정된다.

입력 원본과 stroke command는 document coordinate로 저장한다. `CanvasViewport`는 document, view, device pixel 좌표 사이의 변환을 제공하고,
`PaintCanvasItem`은 QML mouse 위치를 먼저 document 좌표로 변환한 뒤 `InputStrokeBuilder`에 넘긴다. 렌더링은 `RasterProjection`을 통해
`projectBrushDabs` 호출 시점에만 viewport transform을 적용한다. 따라서 pan/zoom이 바뀌어도 raw stroke와 dab command는 그대로 두고 다시 투영할 수 있다.

`BrushDynamics`는 pressure, velocity, tilt, deterministic random 값을 dab 파라미터로 해석한다. pressure는 size, opacity cap, flow
contribution에 매핑된다. velocity는 spacing, opacity, dry-out에 매핑된다. tilt는 rotation, ellipse scale, texture direction에 매핑된다.
`BrushState::randomSeed`는 rotation jitter와 grain 값을 재현 가능하게 만든다.

raw input과 rendered stroke는 분리한다. `StrokeCommand`는 `StrokePath::rawInput`에 사용자가 입력한 원본 사건열을 그대로 보존하고,
`StrokePath::renderedInput`과 `StrokePath::renderedCurve`에 smoothing/interpolation 이후의 파생 데이터를 둔다. 브러시 알고리즘, 해상도, export
조건이 바뀌어도 raw input에서 다시 재생할 수 있어야 한다.

`StrokeResampler`는 렌더 전용 path에서 Catmull-Rom 또는 linear interpolation을 수행한다. raw sample은 수정하지 않고, 보간된 sample에는 timestamp,
velocity, pressure, tilt, device state, 누적 `arcLength`가 들어간다. `StrokeCommand`와 `LiveStrokeBuffer`는 stabilizing 이후
resampling된 rendered input을 사용한다.

dab 배치는 입력 이벤트 개수가 아니라 누적 arc length를 기준으로 한다. 기본 간격은 `brushSize * spacingRatio`이며, `brushSize`가 아직 지정되지 않은 초기 경로에서는 기존
절대 `spacing` 값을 fallback으로 사용한다. segment 경계는 특별히 찍지 않고, 곡선 전체 길이 위에서 다음 dab 거리를 누적하여 계산한다.

`BrushState::randomSeed`는 jitter와 texture 계열 브러시가 같은 stroke를 항상 같은 결과로 재생하도록 저장된다. 현재는 rotation jitter가 deterministic
seed를 사용한다.

stroke 시작과 끝은 `warmupDistance`, `taperDistance`로 dab alpha를 감쇠할 수 있다. 이 값은 raw sample을 바꾸지 않고 dab command 생성 단계에서만 적용된다.

`flow`는 opacity가 아니다. `flow`는 각 dab이 단위 거리마다 더하는 안료량이고, `opacity`는 한 스트로크가 도달할 수 있는 최대 농도이다. `StrokeCompositeBuffer`는 한
stroke 내부의 dab들을 premultiplied alpha로 source-over 누적하되 `RasterSample::opacityCap`을 넘지 않도록 제한한다. stroke가 끝나면 local buffer
전체가 `RasterLayer`에 source-over로 합성된다. 따라서 낮은 flow는 같은 stroke 안에서 여러 dab이 겹칠수록 천천히 진해지고, 낮은 opacity는 최종 농도를 제한한다.

`Stabilizer`는 입력 점의 양 끝을 보존하고 내부 점만 단순 평균 기반으로 안정화한다. `StrokeCurve`는 안정화된 샘플 배열을 document 좌표 곡선으로 보유한다. `RasterLayer`는
stroke 합성용 임시 픽셀 버퍼이고, 문서 구조에 저장될 때는 `Layer`의 `DrawingSurface`로 들어간다.

`LiveStrokeBuffer`는 pointer move 중의 즉시 표시용 파생 버퍼이다. `LiveStrokeFrame`은 raw input을 그대로 복사하고, 표시용 `displayedInput`은
smoothing을 적용하되 마지막 입력 tip은 raw 위치와 시간을 그대로 유지한다. `PaintCanvasItem`은 committed `RasterLayer`와 live `RasterLayer`를 분리해서
그린다. move 중에는 live layer만 계속 다시 그리며, release 시 live layer를 지우고 같은 raw stroke를 `StrokeCommand`로 만들어 committed layer에
합성한다.

각 dab은 document dirty bounds를 계산할 수 있고, `StrokeCommand`와 `LiveStrokeFrame`은 dab별 dirty bounds와 stroke 전체 document dirty
bounds를 함께 가진다. 렌더링 경계에서는 같은 dab bounds를 viewport projection으로 device dirty rect 목록으로 바꾸고 `DirtyRegion`이 전체 bounds를 만든다.
`PaintCanvasItem`은 live preview를 지울 때 이전 live dirty rect만 비우고, 새 live/committed stroke도 해당 dirty bounds만 `update(rect)`로
요청한다.

`PaintCanvasItem`은 Qt 경계에서 왼쪽 마우스 입력만 받아 `PointerEvent`로 정규화한다. 터치나 태블릿 입력은 현재 스트로크 빌더의 완료 조건에 포함하지 않는다. 마우스 press/move는
live preview job을 만들고, release가 하나의 `StrokeInput`을 완료하면 commit job을 만든다.

캔버스 이벤트의 무거운 계산은 `CanvasEventWork` 값 타입 job으로 분리한다. worker thread는 raw input, brush state, stabilizer, raster projection
snapshot만 받아 `LiveStrokeFrame`, `StrokeCommand`, `RasterSample`, dirty bounds를 계산한다. `PaintCanvasItem`의 `RasterLayer`,
`LiveStrokeBuffer`, QML property, `update(rect)` 호출은 GUI thread에서만 변경한다. live preview와 commit은 서로 다른 worker queue를 사용하고
각 queue는 순서를 보존하기 위해 1개 thread로 시작한다. viewport/size/clear가 바뀌면 revision과 live generation으로 오래된 결과를 버린다.

## QML API

QML에서는 `registerIipeQmlTypes()`를 한 번 호출한 뒤 `iipe` 모듈의 `Canvas` 타입 하나를 사용한다. QML import alias는 대문자 식별자를 써야 하므로, namespace
표기가 필요하면 `import iipe 1.0 as Iipe` 뒤 `Iipe.Canvas`로 사용한다.

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
    brushSize: 6
    brushSpacing: 1
    brushSpacingRatio: 0.25
    brushFlow: 0.5
    brushOpacity: 1.0
    brushHardness: 0.8
    livePreviewEnabled: true
    multithreadedEventsEnabled: true
}
```

`Canvas`는 현재 QML 단일 진입점이다. viewport API는 `documentX`, `documentY`, `zoom`, `canvasDevicePixelRatio`,
`setDocumentViewport(x, y, zoom)`, `resetView()`, `panBy(dx, dy)`, `zoomAt(viewX, viewY, factor)`를 제공한다. 브러시 API는
`brushColor`, `brushSize`, `brushSpacing`, `brushSpacingRatio`, `brushFlow`, `brushOpacity`, `brushHardness`,
`setBrush(size, color, flow, opacity)`를 제공한다. 편집/상태 API는 `clear()`, `livePreviewEnabled`, `multithreadedEventsEnabled`,
`liveStrokeActive`, `strokeCount`를 제공한다.

QML은 `Canvas` 하나만 직접 다룬다. `InputStrokeBuilder`, `LiveStrokeBuffer`, `StrokeCommand`, `RasterProjection`, `DirtyRegion`,
`LayerStack` 같은 내부 구조는 C++ 엔진 경계 안에 남긴다.

## 검증

빌드 디렉터리는 `build/`만 사용한다.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

`iiPaintEngineDependencyBoundary` 테스트는 헤더 include 방향과 Qt 의존 위치를 검사한다.
`iiPaintEngineCoreContract` 테스트는 Core 값 타입, UUID 크기, 좌표계 분리 계약을 검사한다.
`iiPaintEnginePipelineHeartbeat` 테스트는 최소 입력 획이 래스터 레이어에 그려지고 문서에 보관되는지 검사한다.
`iiPaintEngineCanvasDocumentStructure` 테스트는 `Document -> Canvas -> LayerStack -> Layer -> DrawingSurface` 소유 구조,
`DocumentMetadata`/`CanvasMetadata`/`LayerMetadata` 분리, DrawingSurface의 물리 표면 책임을 검사한다.
`iiPaintEngineDocumentSerializerContract` 테스트는 `DocumentArchive`가 레이어, brush source, stroke command, color space
profile, asset, history
command를 문자열 payload로 저장하고 다시 열 수 있는지 검사한다.
`iiPaintEngineCanvasQmlApi` 테스트는 `registerIipeQmlTypes()`로 `iipe.Canvas`를 등록하고 QML에서 viewport, brush, live preview,
clear API를 하나의 객체로 사용할 수 있는지 검사한다.
`iiPaintEnginePointerStrokeFlow` 테스트는 마우스 포인터만 스트로크를 완성하고, 벡터 스트로크 위에 브러시 알파 이미지가 flow/spacing에 따라 투영되는지 검사한다.
`iiPaintEngineHybridPaintingModel` 테스트는 샘플 velocity/tilt 보존, dab 배치, 브러시 투영, flow 누적과 opacity 상한 분리를 검사한다.
`iiPaintEngineStrokePhysicalContract` 테스트는 raw/rendered stroke 분리, 누적 arc length 기반 spacing, deterministic seed,
warm-up/taper, stroke dirty bounds를 검사한다.
`iiPaintEngineLiveStrokeRendering` 테스트는 pointer move 중 live buffer가 즉시 샘플을 만들고, 현재 tip을 raw 위치에 유지하며, release 뒤
committed layer로 넘어가는지 검사한다.
`iiPaintEngineBrushDynamicsMapping` 테스트는 pressure/velocity/tilt/random seed가 dab size, flow, opacity cap, spacing,
ellipse, texture direction, grain에 반영되는지 검사한다.
`iiPaintEngineStrokeResampler` 테스트는 Catmull-Rom rendered path가 raw input을 보존하면서 timestamp와 누적 arc length를 가진 보간 샘플을 만드는지
검사한다.
`iiPaintEngineStrokeCompositing` 테스트는 stroke-local buffer의 opacity cap, premultiplied source-over, layer commit 합성을
검사한다.
`iiPaintEngineBrushMaskSampling` 테스트는 브러시 마스크의 subpixel 위치, bilinear sampling, rotation, soft hardness curve, 축소 dab의 픽셀
영역 샘플링을 검사한다.
`iiPaintEngineCoordinateDirtyRegion` 테스트는 view 입력이 document 좌표로 저장되고, 렌더링 때만 viewport projection이 적용되며, dab별 dirty
bounds가 stroke dirty region으로 묶이는지 검사한다.
`iiPaintEngineCanvasEventThreading` 테스트는 live/commit 캔버스 이벤트 계산이 Qt 객체 없이 값 타입 worker job으로 실행되고, 별도 thread에서 만든 sample과
dirty bounds를 GUI 적용 단계로 넘길 수 있는지 검사한다.
