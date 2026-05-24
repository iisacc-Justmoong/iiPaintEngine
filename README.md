# iiPaintEngine

`iiPaintEngine`은 Qt 기반 페인트 엔진 라이브러리이다. 캔버스 객체만 `QObject`/`QQuickItem` 기반으로 두고, 나머지는 값 복사와 aggregate 초기화를 지원하는 단순 `struct` 청사진으로 둔다.

## 모듈 청사진

- Core: `EngineConfig`, `EngineError`, `PaintUuid`, `PaintPoint`, `PaintRect`, `CoordinateSpace`
- Document: `PaintDocument`, `DocumentMetadata`, `DocumentSnapshot`, `DocumentSerializer`
- Layer: `Layer`, `LayerStack`, `RasterLayer`, `StrokeLayer`, `TextLayer`, `VectorLayer`
- Stroke: `Stroke`, `StrokePoint`, `StrokeInput`, `Stabilizer`, `StrokeCurve`, `Rasterizer`
- Brush: `BrushPreset`, `BrushDynamics`, `BrushShape`, `BrushTip`, `BrushLibrary`, `BrushSnapshot`, `BrushResolve`
- Render: `Renderer`, `RenderContext`, `DirtyRegion`, `Compositor`, `CpuRenderer`, `GpuRenderer`
- History: `Command`, `HistoryStack`, `UndoRedoController`, `HistorySnapshot`
- Input: `PointerEvent`, `TabletState`, `InputNormalizer`, `InputStrokeBuilder`
- Color: `PaintColor`, `Palette`, `ColorSpace`, `Gradient`
- QtAdapter: `PaintCanvasItem`, `PaintEngineController`, `DocumentAdapter`, `LayerListModel`

`PaintCanvasItem`은 QML/QQuickItem 연동을 위한 최소 생성자를 제공한다. 그 외 모든 객체는 생성자를 제공하지 않고 공개 필드만 유지하는 경량 플레이스홀더로 시작한다.

## 검증

빌드 디렉터리는 `build/`만 사용한다.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
