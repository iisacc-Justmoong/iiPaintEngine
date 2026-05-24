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

## 검증

빌드 디렉터리는 `build/`만 사용한다.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

`iiPaintEngineDependencyBoundary` 테스트는 헤더 include 방향과 Qt 의존 위치를 검사한다.
