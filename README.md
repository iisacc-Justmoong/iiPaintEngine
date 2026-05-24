# iiPaintEngine

`iiPaintEngine`은 Qt 기반 페인트 엔진 라이브러리이다. Qt 화면 객체인 `PaintCanvasItem`만 `QObject`/`QQuickItem` 기반으로 두고, 나머지는 값 복사와 aggregate 초기화를 지원하는 단순 `struct` 청사진으로 둔다. 엔진 본체는 가능한 한 순수 C++ 데이터와 알고리즘으로 유지한다.

## 모듈 청사진

- Core: `EngineConfig`, `EngineError`, `PaintUuid`, `PaintPoint`, `PaintRect`, `CoordinateSpace`
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

`PaintCanvasItem`은 QML/QQuickItem 연동을 위한 최소 생성자를 제공한다. 그 외 모든 객체는 생성자를 제공하지 않고 공개 필드만 유지하는 경량 플레이스홀더로 시작한다.

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

## Core 계약

Core는 모든 상위 계층이 공유하는 최하층 타입만 가진다.

- `Types`: `Scalar`, `Pixel`, `Byte` 별칭을 제공한다.
- `PaintUuid`: 16바이트 순수 C++ UUID 저장소이다.
- `CoordinateSpace`: `Canvas`, `View`, `DevicePixel` 좌표계를 구분한다.
- `PaintPoint<Space>` / `PaintRect<Space>`: 좌표계 marker를 템플릿 인자로 받아 서로 다른 좌표계끼리 대입되지 않는다.
- `CanvasPoint`, `ViewPoint`, `DevicePixelPoint`: 캔버스 좌표, 뷰 좌표, 장치 픽셀 좌표의 명시적 별칭이다.
- `CanvasRect`, `ViewRect`, `DevicePixelRect`: 좌표계별 사각형 별칭이다.
- `EngineError`: 에러 코드와 선택적 메시지 포인터만 가진 경량 값 타입이다.
- `EngineConfig`: 기본 DPI와 device pixel ratio만 가진 최소 설정 값 타입이다.

장치 픽셀 좌표는 `Types::Pixel` 정수 좌표를 사용하고, 캔버스/뷰 좌표는 `Types::Scalar` 실수 좌표를 사용한다.

## 검증

빌드 디렉터리는 `build/`만 사용한다.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

`iiPaintEngineDependencyBoundary` 테스트는 헤더 include 방향과 Qt 의존 위치를 검사한다.
`iiPaintEngineCoreContract` 테스트는 Core 값 타입, UUID 크기, 좌표계 분리 계약을 검사한다.
