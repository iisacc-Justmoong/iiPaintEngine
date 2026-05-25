# iiPaintEngine

`iiPaintEngine`은 Qt 기반 페인트 엔진 라이브러리이다. Qt 화면 객체인 `PaintCanvasItem`만 `QObject`/`QQuickPaintedItem` 기반으로 두고, 나머지는 값 복사와 aggregate 초기화를 지원하는 단순 `struct` 청사진으로 둔다. 엔진 본체는 가능한 한 순수 C++ 데이터와 알고리즘으로 유지한다.

## 모듈 청사진

- Core: `EngineConfig`, `EngineError`, `PaintUuid`, `PaintPoint`, `PaintRect`, `CoordinateSpace`, `RasterSample`
- Document: `PaintDocument`, `DocumentMetadata`, `DocumentSnapshot`, `DocumentSerializer`
- Canvas: `CanvasState`, `CanvasViewport`, `CanvasSession`
- Layer: `Layer`, `LayerStack`, `RasterLayer`, `StrokeLayer`, `TextLayer`, `VectorLayer`
- Stroke: `Stroke`, `StrokePoint`, `StrokeInput`, `Stabilizer`, `StrokeCurve`, `Rasterizer`
- Brush: `BrushPreset`, `BrushDynamics`, `BrushShape`, `BrushTip`, `BrushLibrary`, `BrushSnapshot`, `BrushResolve`
- Render: `Renderer`, `RenderContext`, `DirtyRegion`, `Compositor`, `CpuRenderer`, `GpuRenderer`
- History: `Command`, `HistoryStack`, `UndoRedoController`, `HistorySnapshot`
- Input: `PointerEvent`, `TabletState`, `InputNormalizer`, `InputStrokeBuilder`
- Color: `PaintColor`, `Palette`, `ColorSpace`, `Gradient`
- QtAdapter: `PaintCanvasItem`, `PaintEngineController`, `DocumentAdapter`, `LayerListModel`

`PaintCanvasItem`은 QML/QQuickItem 연동을 위한 마우스 입력 경계와 화면 페인트 경계를 제공한다. 그 외 모든 객체는 생성자를 제공하지 않고 공개 필드만 유지하는 경량 값 타입으로 시작한다.

## 의존 방향

허용되는 의존 방향은 아래와 같다.

```text
QtAdapter
-> Canvas / Input
-> Document
-> Layer / Stroke / Brush / Render / History / Color
-> Core
```

`Core`는 최하층이며 어떤 프로젝트 모듈에도 의존하지 않는다. `Layer`, `Stroke`, `Brush`는 서로 직접 알지 않고 `Core` 타입을 통해 최소한의 식별자와 좌표만 공유한다. `QObject`, `QQuickItem`, QML 관련 include는 `QtAdapter`에만 둔다.

`Input`은 포인터 이벤트를 `StrokeInput` 값으로 바꾸기 위해 `Stroke`의 순수 값 타입만 사용할 수 있다. `Input`은 Qt, QML, 브러시, 래스터 레이어를 알 수 없으며, 장치별 이벤트 해석과 스트로크 단위의 벡터 입력 구성까지만 맡는다.

## Core 계약

Core는 모든 상위 계층이 공유하는 최하층 타입만 가진다.

- `Types`: `Scalar`, `Pixel`, `Byte` 별칭을 제공한다.
- `PaintUuid`: 16바이트 순수 C++ UUID 저장소이다.
- `CoordinateSpace`: `Canvas`, `View`, `DevicePixel` 좌표계를 구분한다.
- `PaintPoint<Space>` / `PaintRect<Space>`: 좌표계 marker를 템플릿 인자로 받아 서로 다른 좌표계끼리 대입되지 않는다.
- `CanvasPoint`, `ViewPoint`, `DevicePixelPoint`: 캔버스 좌표, 뷰 좌표, 장치 픽셀 좌표의 명시적 별칭이다.
- `CanvasRect`, `ViewRect`, `DevicePixelRect`: 좌표계별 사각형 별칭이다.
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
-> Stabilizer
-> StrokeCurve
-> Rasterizer
-> RasterLayer
-> PaintDocument
```

초기 브러시는 `Rasterizer`의 기본값인 검은색 원형 브러시를 사용한다. 브러시 알파 이미지가 지정되면 `Rasterizer`는 먼저 `StrokeCurve`의 벡터 구간을 spacing/density 간격으로 순회하여 `BrushDab` 명령 시퀀스를 만든다. 각 dab은 position, scale, rotation, alpha, color, blendMode를 가진 작은 브러시 투영 명령이다. 그 다음 dab 위치에 브러시 알파 이미지를 변환하여 `RasterSample`로 투영한다.

`StrokeInput`은 단순 점 목록이 아니라 시간, 압력, 속도, 기울기를 가진 샘플 시퀀스로 보존된다. `makeStrokeCurve`는 샘플의 시간 차이와 이동 거리로 velocity를 계산한다. dab의 scale은 pressure, rotation은 tilt 또는 곡선 접선, 간격은 spacing/density와 velocity spacing 계수, alpha는 flow에 의해 결정된다.

`flow`는 opacity가 아니다. `flow`는 각 dab이 단위 거리마다 더하는 안료량이고, `opacity`는 한 스트로크가 도달할 수 있는 최대 농도이다. `RasterLayer`는 같은 위치에 여러 dab이 들어오면 alpha를 누적하되 `RasterSample::opacityCap`을 넘지 않도록 합성한다. 따라서 낮은 flow는 여러 dab이 겹칠수록 천천히 진해지고, 낮은 opacity는 최종 농도를 제한한다.

`Stabilizer`는 입력 점의 양 끝을 보존하고 내부 점만 단순 평균 기반으로 안정화한다. `StrokeCurve`는 안정화된 샘플 배열을 캔버스 좌표 곡선으로 보유한다. `RasterLayer`는 샘플을 픽셀 버퍼에 칠하고, `PaintDocument`는 그 래스터 레이어를 소유한다.

`PaintCanvasItem`은 Qt 경계에서 왼쪽 마우스 입력만 받아 `PointerEvent`로 정규화한다. 터치나 태블릿 입력은 현재 스트로크 빌더의 완료 조건에 포함하지 않는다. 마우스 press/move/release가 하나의 `StrokeInput`으로 완료된 뒤 안정화, 곡선화, 브러시 투영, 레이어 페인트 순서로 커밋된다.

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
`iiPaintEnginePointerStrokeFlow` 테스트는 마우스 포인터만 스트로크를 완성하고, 벡터 스트로크 위에 브러시 알파 이미지가 flow/spacing에 따라 투영되는지 검사한다.
`iiPaintEngineHybridPaintingModel` 테스트는 샘플 velocity/tilt 보존, dab 배치, 브러시 투영, flow 누적과 opacity 상한 분리를 검사한다.
