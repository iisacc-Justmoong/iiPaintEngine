# iiPaintEngine

`iiPaintEngine`은 Qt 기반 페인트 엔진 라이브러리이다. Qt 화면 객체인 `PaintCanvasItem`만 `QObject`/`QQuickPaintedItem` 기반으로 두고, 나머지는 값 복사와
aggregate 초기화를 지원하는 단순 `struct` 청사진으로 둔다. 엔진 본체는 가능한 한 순수 C++ 데이터와 알고리즘으로 유지한다.
`QQuickPaintedItem`/`QPainter` 의존은 `QtAdapter/PaintCanvasItem`에만 머물러야 하며, scene graph나 texture-backed renderer로 교체할 때는 이
경계의
구현만 바꾸는 것을 목표로 한다.

## 모듈 청사진

- Core: `EngineConfig`, `EngineError`, `PaintUuid`, `PaintPoint`, `PaintRect`, `CoordinateSpace`, `RasterSample`,
  `RasterBlendMode`
- Document: `PaintDocument`, `DocumentMetadata`, `DocumentSnapshot`, `DocumentSerializer`
- Canvas: `Canvas`, `CanvasMetadata`, `CanvasState`, `CanvasViewport`, `CanvasSession`
- Layer: `DrawingSurface`, `Layer`, `LayerMetadata`, `LayerStack`, `RasterLayer`, `StrokeCompositeBuffer`,
  `PremultipliedPixel`,
  `StrokeLayer`, `TextLayer`, `VectorLayer`
- Stroke: `Stroke`, `StrokePoint`, `StrokeInput`, `Stabilizer`, `StrokeCurve`, `StrokeResampler`, `Rasterizer`,
  `BrushDab`, `StrokeGeometryReport`, `StrokeGeometrySample`, `StrokeGeometrySegment`, `StrokePath`, `BrushState`,
  `StrokeCommand`, `StrokeRepository`, `LiveStrokeFrame`, `LiveStrokeBuffer`
- Brush: `BrushPreset`, `BrushMaterial`, `BrushPresetSerializer`, `BrushDynamics`, `BrushDynamicsInput`,
  `BrushDynamicsResult`, `BrushShape`, `BrushTip`, `BrushLibrary`, `BrushSnapshot`, `BrushResolve`
- Render: `Renderer`, `RenderContext`, `DirtyRegion`, `Compositor`, `CpuRenderer`, `GpuRenderer`
- History: `Command`, `HistoryStack`, `UndoRedoController`, `HistorySnapshot`
- Input: `PointerEvent`, `PressureInput`, `TabletState`, `InputNormalizer`, `InputStrokeBuilder`
- Color: `PaintColor`, `ColorChromaticity`, `Palette`, `ColorSpace`, `Gradient`
- Selection: `SelectionMask`, `SelectionState`
- Transform: `AffineTransform`, `TransformState`
- Filter: `FilterNode`, `FilterPipeline`
- Tool: `FillOperation`, `GradientOperation`, `EraserOperation`, `ToolState`, `ToolStateMachine`
- QtAdapter: `PaintCanvasItem`, `CanvasAdapter`, `CanvasBrushConfig`, `CanvasViewportConfig`, `CanvasRuntimeConfig`,
  `CanvasStateSnapshot`, `CanvasEventWork`, `registerIipeQmlTypes`, `PaintEngineController`, `DocumentAdapter`,
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
Layer / Brush / History / Color
-> Core
Render
-> Core / Layer
Selection
-> Core
Transform
-> Core / Layer / Selection
Filter
-> Core / Layer / Selection
Tool
-> Core / Layer / Selection / Transform / Filter
```

`Core`는 최하층이며 어떤 프로젝트 모듈에도 의존하지 않는다. `Layer`, `Brush`, `History`, `Color`는 `Core` 타입을 통해 최소한의 식별자와 좌표만 공유한다.
`Render`는 layer stack을 최종 래스터 결과로 합성하기 위해 `Layer`의 값 타입을 읽을 수 있다.
`Stroke`는 브러시 파라미터를 dab 명령으로 해석하기 위해 `BrushDynamics`와 `BrushMaterial` 값 타입을 읽을 수 있다. `Canvas`는 `LayerStack`과
`StrokeRepository`를 묶는 작업
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
Mouse / Tablet PointerEvent
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
name, visible, opacity, blend mode, layer kind, clipping, alpha lock 같은 레이어 편집 공통 정보를 담는다. `LayerMask`는 레이어의 per-pixel
alpha mask를 보관하고, group layer는 `Layer::children`으로 하위 레이어를 가진다.

`Compositor`는 `LayerStack`을 `RasterLayer`로 합성한다. 현재 계약은 source-over, multiply, screen, overlay blend mode와 layer
opacity, layer
mask, clipping-to-below, group child composition을 포함한다. adjustment layer는 아직 색 변환 연산을 수행하지 않고 `LayerKind::Adjustment` 값
계약만
가진다.
`Renderer`는 `RenderContext`를 기준으로 CPU/GPU backend를 선택해 전체 layer stack을 target `RasterLayer`로 투영한다. CPU backend는 실제
`Compositor`를 호출한다. GPU backend는 아직 장치 구현이 없으면 `allowCpuFallback` 계약에 따라 CPU로 내려가며, fallback이 금지되어 있으면 `InvalidState`를
반환한다.
`RenderExecutionPlan`은 scalar CPU, SIMD CPU, GPU compute 경로와 target buffer format을 분리해 기록한다. `RenderTileCache`는 dirty
region을 tile rect로 나눠 partial invalidation을 수행하고, `BrushStampAtlas`는 브러시 stamp alpha를 format/revision별로 캐시한다.
`StrokeReplayCache`는 대형 캔버스에서 stroke id, brush revision, stroke revision, tile 좌표, buffer format 단위의 dab projection 결과를
재사용하기 위한 값 계약이다. `RenderContext`의 tile cache, brush atlas, stroke replay, SIMD/GPU, linear compositing 플래그는 아직 장치별
구현이 얇아도 상위 엔진이 어떤 backend 계획을 요구했는지 잃지 않게 한다.
색공간은 현재 source/target `ColorSpace`가 같은 경우에만 통과시키고, 다른 primaries/transfer/component
encoding/ICC/name/linear/HDR/chromaticity 조합은
`UnsupportedColorTransform`으로 실패시켜 암묵 변환을 금지한다.

`ColorSpace`는 이름만 저장하지 않는다. primaries는 sRGB, Display P3, Rec.2020, Custom을 구분하고, transfer function은 sRGB, linear, gamma
2.2,
PQ, HLG를 구분한다. component encoding은 8-bit, 16-bit, float16, float32를 표현한다. ICC profile bytes, linear flag, HDR flag,
component range,
reference white nits, RGB primary/white-point chromaticity가 같은 값 계약 안에 있다. `PaintColor`는 `Types::Scalar` component와
`ColorSpace`를 함께 들고,
HDR float space에서는 1.0을 넘는 component를 보존하며 SDR space에서는 `clampPaintColorToColorSpace`로 range를 제한한다.

`DocumentArchive`는 저장 포맷의 값 계약이다. `PaintDocument` 본문, brush source snapshot, color space profile, 외부 asset, history
stack을 한 묶음으로
보관한다. `serializeDocumentArchive`/`deserializeDocumentArchive`는 이 archive를 key/value payload로 왕복시켜 레이어 표면, layer
metadata, stroke command,
layer mask, child layer, stroke command, brush state material, brush source material/mask, ICC profile bytes, asset
bytes, history command와
before/after patch payload가 파일 컨테이너 구현 전에 먼저 보존되는지 고정한다.
모듈 경계에서는 `PaintDocument` 자체가 여전히 `Canvas`만 소유하고, `DocumentSerializer`만 저장 포맷 경계로서 `Brush`, `Color`, `History`의
persistent value를
읽을 수 있다.

앱/문서 조작 경계는 `DocumentAdapter`, `LayerListModel`, `PaintEngineController`가 맡는다. 세 타입은 Qt 객체가 아니라 복사 가능한 값 타입이며,
`DocumentAdapter`는 현재 `DocumentArchive`와 active canvas index를 들고 `new/load/save`, active layer 선택, 레이어 이름/가시성/opacity
변경,
stroke commit을 문서 모델에 반영하는 free function API를 제공한다. `commitStrokeToActiveDocumentLayer`는 `StrokeCommand`를 active
layer의 `DrawingSurface`에 적용하고 같은 command를 `Canvas::strokes`와 `HistoryStack`에도 남긴 뒤 `Canvas::surface` composite를 갱신한다.
`LayerListModel`은 `LayerStack`을 앱 UI가 바로 읽을 수 있는 flat row 목록으로 투영하고, `PaintEngineController`는 문서 생성/열기/저장,
레이어 추가/선택, stroke commit 뒤 layer list refresh까지 묶는 상위 앱 API이다.

`HistoryStack`은 부가 로그가 아니라 undo/redo의 중심 operation journal이다. `Command`는 command kind, scope, transaction id,
coalescing key, dirty bounds, target id와 함께 before/after `CommandPatch` payload를 가진다. payload는 inline bytes 또는 asset
reference로
표현할 수 있어 raster tile, layer metadata, mask, selection, transform 같은 변화가 나중에 실제 적용기로 연결될 수 있다. `recordHistoryCommand`는
redo
branch를 지우고 sequence를 부여하며, `undoHistoryCommand`/`redoHistoryCommand`는 command를 undo/redo stack 사이로 이동시켜 호출자가 같은 patch를
반대 방향으로
적용할 수 있게 한다. `HistorySnapshot`은 UI나 저장 경계가 undo/redo depth, cursor, next sequence를 안정적으로 읽기 위한 값 계약이다.

`SelectionState`는 rectangle/mask 기반 선택 영역과 inverted/feather metadata를 보관한다. 선택이 비활성화된 상태는 전체 픽셀 허용으로 해석되어 기존 stroke/fill
경로와 자연스럽게 이어진다. `TransformState`는 affine transform, interpolation, source/transformed bounds를 값으로 보관하고, 현재 구현은 selection
이동과
surface crop을 먼저 제공한다. `FillOperation`, `GradientOperation`, `EraserOperation`은 `DrawingSurface`에 선택 영역을 기준으로 적용되는 기본 편집
도구이며, 각 operation의 `enabled`가 false이면 적용 함수는 표면을 변경하지 않는다.
`FilterPipeline`은 blur/smudge node 목록을 순서대로 적용한다. 현재 blur는 선택 영역 내부 box blur, smudge는 좌측 샘플을 strength로 섞는 최소 CPU 계약이며,
각 `FilterNode::enabled`가 false이면 해당 노드는 건너뛴다. 차후 GPU/비파괴 필터 그래프로 승격될 수 있다. `ToolStateMachine`은
selection/transform/crop/fill/gradient/eraser/blur/smudge 같은 도구의 begin,
drag,
commit, cancel 상태 전이를 값 타입으로 고정한다. `ToolStateMachine::enabled`가 false이면 도구 상태 전이를 받지 않는다.

초기 브러시는 `Rasterizer`의 기본값인 검은색 원형 브러시를 사용한다. 원형 브러시는 픽셀 중심의 inside/outside 판정이 아니라 원 coverage를 mask alpha로 계산해
가장자리 픽셀을 anti-aliased alpha로 투영한다. 브러시 알파 이미지가 지정되면 `Rasterizer`는 먼저 `StrokeCurve`의 벡터 구간을 spacing/density 간격으로
순회하여 `BrushDab` 명령 시퀀스를 만든다. 각 dab은 position, scale, rotation, alpha, color, blendMode를 가진 작은 브러시 투영 명령이다. 그 다음 dab
위치에 브러시 알파 이미지 또는 원형 mask를 `RasterSample`로 투영한다.

브러시 알파 이미지는 dab의 scale, ellipse scale, rotation을 역변환한 뒤 source mask를 bilinear sampling으로 읽는다. 정수 픽셀에 nearest로 찍지 않고
subpixel 위치를 보존하며, 축소된 dab은 픽셀 영역을 샘플링해 작은 브러시가 격자 사이에서 사라지지 않도록 한다. `Rasterizer::hardness`는 bilinear mask alpha에 적용되는
커브이며, 낮을수록 soft mask 가장자리가 더 부드럽게 감쇠한다.

`StrokeInput`은 단순 점 목록이 아니라 시간, 압력, 속도, 기울기, 펜 회전, 장치 상태를 가진 샘플 시퀀스로 보존된다. `makeStrokeCurve`는 샘플의 시간 차이와 이동 거리로
velocity를 계산한다. dab의
scale은 pressure, rotation은 tilt 또는 곡선 접선, 간격은 spacing/density와 velocity spacing 계수, alpha는 flow에 의해 결정된다.

`StrokeGeometryReport`는 stroke를 렌더 결과가 아니라 document 좌표의 기하 객체로 해석한 파생 정보이다. `describeStrokeGeometry`는 원본 점 목록이나
`StrokeCurve`에서 sample/segment 수, bounds, path length, chord length, straightness, 암묵 closing chord 기준 signed area,
centroid와 길이 가중 centroid, segment별 heading/normal/velocity/pressure delta, sample별 normalized arc length, tangent,
signed turn,
curvature, pressure derivative, tilt magnitude, 전체 pressure/velocity/tilt 통계, inflection/cusp 수, principal axis와 spread를
계산한다.
이 계산은 선형대수와 polyline 지표만 사용하므로 외부 의존성을 추가하지 않는다. 유지보수 범위가 작은 순수 C++ 값 계산이며, 엔진의 고유 stroke model을 그대로 읽는
도메인 로직이기 때문이다.

입력 원본과 stroke command는 document coordinate로 저장한다. `CanvasViewport`는 document, view, device pixel 좌표 사이의 변환을 제공하고,
`PaintCanvasItem`은 QML mouse 위치를 먼저 document 좌표로 변환한 뒤 `InputStrokeBuilder`에 넘긴다. 렌더링은 `RasterProjection`을 통해
`projectBrushDabs` 호출 시점에만 viewport transform을 적용한다. 따라서 pan/zoom이 바뀌어도 raw stroke와 dab command는 그대로 두고 다시 투영할 수 있다.

`BrushDynamics`는 pressure, velocity, tilt, deterministic random 값을 dab 파라미터로 해석한다. 기존 호환 필드에서는 pressure가 size,
flow, opacity cap에 매핑된다. 압력이 높을수록 해당 값은 100%에 가까워지고, 압력이 낮을수록 0%에 가까워진다. pressure는 hardness에는 매핑되지 않는다.
velocity는 spacing, opacity, dry-out에 매핑된다. tilt는 rotation, ellipse scale, texture direction에 매핑된다.
각 입력 계열은 공개 bool 스위치로 켜고 끌 수 있다. `pressureInputEnabled`, `velocityInputEnabled`, `tiltInputEnabled`,
`randomInputEnabled`가
false이면 해당 입력값은 neutral 값으로 해석된다. 개별 매핑도 `pressureToSizeEnabled`, `pressureToOpacityEnabled`, `pressureToFlowEnabled`,
`velocityToSpacingEnabled`, `velocityToOpacityEnabled`, `velocityToDryOutEnabled`, `tiltToRotation`,
`tiltToEllipseEnabled`,
`tiltToTextureDirection`, `rotationJitterEnabled`, `grainJitterEnabled`로 독립 제어한다. 비활성화된 기능은 값을 저장하더라도 dab 생성 인자로
사용하지 않는다.
`pressureToSize`, `pressureToFlow`, `pressureToOpacity` 값이 1.0이면 pressure 0.0은 0%, pressure 1.0은 100%로 해석된다.
`BrushState::randomSeed`는 rotation jitter와 grain 값을 재현 가능하게 만든다. `PaintCanvasItem`의 기본 brush dynamics는 pressure를 size,
flow, opacity에 1.0 비율로 연결한다. hardness는 필압 인자가 아니라 `Rasterizer::hardness`와 `hardnessEnabled`로만 제어한다.

고급 dynamics는 `BrushDynamicsPropertyResponse`와 `BrushDynamicsResponseCurve`로 속성별 response curve를 가진다. size, flow,
opacity,
spacing, scatter, rotation offset, texture depth, wetness, dry-out, bristle spread가 각각 pressure/velocity/tilt/random
curve를 독립적으로
가질 수 있다. 각 curve는 normalized input 0%, 50%, 100% 지점의 `min`, `center`, `max`, `easing`, deterministic `jitter`를 저장한다.
한 속성에 여러 input curve가 켜지면 multiply 또는 add combine mode로 cross-input mapping을 만든다. 이 값들은 `BrushPresetSerializer`,
`DocumentSerializer`, `BrushSnapshot`, `StrokeCommand`의 dab payload에 저장되어 다시 열 수 있다.
입력 pressure curve 자체는 `PressureInput::curveMinimum`, `curveCenter`, `curveMaximum` 세 점으로 조절한다. 기본값 0.0, 0.5, 1.0은
기존 선형 정규화를 그대로 유지한다. min/center/max는 normalized pen pressure 0%, 50%, 100% 지점의 output을 뜻하며, center를 올리면 낮은
필압도 더 높은 output으로 빨리 올라가고 center를 낮추면 더 늦게 올라간다.

`Rasterizer`도 stroke 인자별 공개 bool 스위치를 가진다. `brushSpacingRatio`의 공개 범위는 0.0~1.0, 즉 0~100%이다.
`flowEnabled`, `opacityEnabled`, `hardnessEnabled`, `spacingEnabled`가 false이면
해당 stroke 인자는 저장되어 있어도 dab 생성 또는 brush mask 투영의 입력값으로 쓰지 않고 neutral 기본값으로 해석한다. flow와 opacity는 1.0, hardness는 1.0,
spacing은 0.0을 사용한다. 실제 dab 배치 단계에서는 0% spacing도 렌더링 안전 하한인 0.01 document unit으로 제한된다.

`BrushMaterial`은 회화적 표현층의 저장 가능한 계약이다. texture/grain alpha, paper grain, dual brush, scatter, wet paint/smudge/mixer 모델,
bristle shape/count를 한 값으로 묶고, `BrushPresetSerializer`는 이 preset을 독립 payload로 왕복시킨다. texture, paper grain, dual brush,
scatter, simulation, bristle은 모두 공개 `enabled` 스위치를 가진다. texture는 tip, stroke-follow, document, paper coordinate space를
선택할 수 있고
asset cache의 alpha mask를 사용할 수 있다. paper grain은 document/paper 기준으로 brush mask를 줄이며, dual brush는
multiply/add/subtract/difference
alpha composition을 제공한다. scale/rotation jitter는 deterministic dab 값으로 저장된다. 현재 렌더 경로는 활성화된 texture alpha와 grain,
dual brush composition, scatter position, wet/smudge/mix flow 감쇠를 deterministic dab 명령으로 반영한다. wet projection 경로는 source
canvas sampler를 받아 dab 위치와 stroke 방향 뒤쪽의
캔버스 픽셀을 샘플링하고, `smudgeStrength`, `pickup`, `deposit`, `mixStrength`, `wetness`로 끌고 온 색과 브러시 안료를 섞는다. Qt adapter는 현재
`RasterLayer`를 이 sampler로 감싸 넘기며, Stroke 모듈은 레이어 타입에 직접 의존하지 않는다. `BristleSimulation`의
shape/count/length/stiffness는 dab의 접촉 ellipse를 방향성 있게 늘리거나 눌러 flat/fan bristle 접촉 형상을 만든다. 아직 실제 유체 압력장이나 개별 bristle
입자 시뮬레이션은 아니며, 캔버스 샘플링 기반의 첫 물성 브러시 계약이다.

raw input과 rendered stroke는 분리한다. `StrokeCommand`는 `StrokePath::rawInput`에 사용자가 입력한 원본 사건열을 그대로 보존하고,
`StrokePath::renderedInput`과 `StrokePath::renderedCurve`에 smoothing/interpolation 이후의 파생 데이터를 둔다. 브러시 알고리즘, 해상도, export
조건이 바뀌어도 raw input에서 다시 재생할 수 있어야 한다.
`StrokePath::rawGeometry`와 `StrokePath::renderedGeometry`는 같은 분리를 기하 정보에도 적용한다. raw geometry는 사용자 입력의 원래 path를 설명하고,
rendered geometry는 안정화와 resampling 이후 실제 dab 배치에 쓰인 path를 설명한다. `LiveStrokeFrame`도 즉시 표시 중인 stroke에 대해
`rawGeometry`와 `displayedGeometry`를 함께 내보낸다.

`StrokeResampler`는 렌더 전용 path에서 Catmull-Rom 또는 linear interpolation을 수행한다. raw sample은 수정하지 않고, 보간된 sample에는 timestamp,
velocity, pressure, tilt, device state, 누적 `arcLength`가 들어간다. `StrokeCommand`와 `LiveStrokeBuffer`는 stabilizing 이후
resampling된 rendered input을 사용한다.

dab 배치는 입력 이벤트 개수가 아니라 누적 arc length를 기준으로 한다. 기본 간격은 `brushSize * spacingRatio`이며, `brushSize`가 아직 지정되지 않은 초기 경로에서는 기존
절대 `spacing` 값을 fallback으로 사용한다. segment 경계는 특별히 찍지 않고, 곡선 전체 길이 위에서 다음 dab 거리를 누적하여 계산한다.

`BrushState::randomSeed`는 jitter와 texture 계열 브러시가 같은 stroke를 항상 같은 결과로 재생하도록 저장된다. 현재는 rotation jitter가 deterministic
seed를 사용한다.

stroke 시작과 끝은 `warmupDistance`, `taperDistance`로 dab alpha를 감쇠할 수 있다. `taperMinimum`과
`warmupTaperShape`/`endTaperShape`는 linear, ease-in, ease-out, smooth-step 형태로 dab alpha shape를 제어한다. 이 값은 raw
sample을 바꾸지 않고 dab command 생성 단계에서만 적용된다.

`flow`는 opacity가 아니다. `flow`는 각 dab이 단위 거리마다 더하는 안료량이고, `opacity`는 한 스트로크가 도달할 수 있는 최대 농도이다. `StrokeCompositeBuffer`는 한
stroke 내부의 dab들을 premultiplied alpha로 source-over 누적하되 `RasterSample::opacityCap`을 넘지 않도록 제한한다. stroke가 끝나면 local buffer
전체가 `RasterLayer`에 source-over로 합성된다. 따라서 낮은 flow는 같은 stroke 안에서 여러 dab이 겹칠수록 천천히 진해지고, 낮은 opacity는 최종 농도를 제한한다.

`Stabilizer`는 기본적으로 입력 점의 양 끝을 보존하고 내부 점만 moving average 기반으로 안정화한다. 고급 모드에서는 `StabilizerMode::Line`으로
line smoothing을 적용할 수 있고, `cuspPreservationEnabled`/`cuspAngleRadians`로 날카로운 꺾임을 raw 위치에 남긴다.
`predictionEnabled`, `predictionHorizon`, `latencyCompensation`은 마지막 입력점을 최근 속도 방향으로 예측해 지연을 보정하며,
`adaptiveResamplingEnabled`와 min/max spacing, velocity scale은 빠른 구간을 더 조밀한 rendered input으로 만든다. `StrokeCurve`는 안정화된 샘플
배열을 document 좌표 곡선으로 보유한다. `RasterLayer`는 stroke 합성용 임시 픽셀 버퍼이고, 문서 구조에 저장될 때는 `Layer`의
`DrawingSurface`로 들어간다.

`LiveStrokeBuffer`는 pointer move 중의 즉시 표시용 파생 버퍼이다. `LiveStrokeFrame`은 raw input을 그대로 복사하고, 표시용 `displayedInput`은
smoothing을 적용하되 기본 모드에서는 마지막 입력 tip을 raw 위치와 시간에 둔다. `Stabilizer::previewDabsMatchCursor`가 true이면 예측/보정된 마지막
dab 위치를 `cursorPreviewPosition`으로 노출해 cursor preview와 실제 dab preview가 같은 좌표를 보게 한다. `PaintCanvasItem`은 committed
`RasterLayer`와 live `RasterLayer`를 분리해서 그린다. move 중에는 live layer만 계속 다시 그리며, release 시 live layer를 지우고 같은 raw
stroke를 `StrokeCommand`로 만들어 committed layer에 합성한다. 지우개 stroke의 live layer는 지우개 샘플을 불투명 마스크로 보관하고, 화면 합성 시
committed layer 위에 `DestinationOut`으로 적용해 release 전에도 최종 지움 결과와 같은 픽셀 변화를 보여준다.

각 dab은 document dirty bounds를 계산할 수 있고, `StrokeCommand`와 `LiveStrokeFrame`은 dab별 dirty bounds와 stroke 전체 document dirty
bounds를 함께 가진다. 렌더링 경계에서는 같은 dab bounds를 viewport projection으로 device dirty rect 목록으로 바꾸고 `DirtyRegion`이 전체 bounds를 만든다.
`PaintCanvasItem`은 live preview를 지울 때 이전 live dirty rect만 비우고, 새 live/committed stroke도 해당 dirty bounds만 `update(rect)`로
요청한다.

`PressureInput`은 raw pressure, enabled flag, min/max calibration, contact/hover, graph curve 상태를 받아 0~1 normalized
pressure로 해석하는 독립 값
객체이다. `InputNormalizer`는 태블릿 장치의 hover/contact, barrel button, eraser, pressure range, tilt calibration, rotation을 Qt와
무관한
`PointerEvent`로 정규화하며, pressure 값은 `PressureInput`을 통해 계산한다. `InputStrokeBuilder`는 mouse와 tablet contact만 stroke로 받아들이고,
hover와 touch gesture는 stroke를 시작하지 않는다.
touch gesture는 centroid, translation, scale, rotation, finger count를 가진 별도 pointer event surface로 보존되어 viewport gesture
같은 상위 경계에서 사용할 수 있다. `InputNormalizer`의 `pressureEnabled`, `tiltEnabled`, `rotationEnabled`, `hoverEnabled`,
`barrelButtonEnabled`, `eraserEnabled`, `touchGestureEnabled`가 false이면 해당 장치 기능은 neutral 값으로 정규화되어 stroke 입력 인자로 전달되지
않는다.
`PaintCanvasItem`은 현재 Qt 마우스 이벤트와 tablet 이벤트를 `PointerEvent`로 만들고, press/move는 raw sample만 `InputStrokeBuilder`에
가볍게 누적한다. live preview job은 입력 이벤트마다 직접 만들지 않고 `livePreviewFrameIntervalMs` frame tick에서 현재까지 쌓인 raw stroke snapshot을
묶어 만든다. live preview worker는 interactive preview에만 최소 1 document unit의 dab 간격을 적용해 과도하게 촘촘한 brush spacing에서도 frame 중
projection 작업량이 폭증하지 않게 하며, release 후 commit stroke는 사용자가 설정한 brush spacing을 그대로 보존한다. 이어지는 preview frame은 이전
live stroke의 안정된 앞부분을 유지하고 겹침을 둔 후미 구간만 지운 뒤 새 samples를 덧칠하므로, 긴 stroke 입력 중 live layer 전체를 매번 다시 칠하지 않는다.
projection/bounds API는 C++17 호환 `BrushDabSpan`으로 dab 배열 suffix를 받아 증분 preview가 tail dabs를 별도 벡터로 복사하지 않는다. worker의
dirty
bounds는 rect vector를 만들지 않고 직접 union으로 계산하며, sample vector reserve는 brush footprint를 기준으로 하되 과도한 선점이 생기지 않게 상한을 둔다.
wet/smudge처럼 source layer가 필요한 brush job은 raw stroke의 projected bounds를 넉넉히 inflate한 source patch만 worker request에 복사하고,
`RasterSourceSampler::origin`으로 cropped layer의 local 좌표와 canvas device 좌표를 분리한다.
release가 하나의
`StrokeInput`을 완료하면 즉시 레이어나 문서를 바꾸지 않고 pending commit request에 넣는다. 실제 commit job과 stroke count/document 상태 변경은 다음
commit frame에서 시작되고, worker result가 GUI thread에 적용될 때만 발생한다. stroke commit undo 기록은 전체 canvas snapshot이 아니라 worker가
반환한 dirty bounds 내부의 이전 pixel patch만 저장하므로, 작은 브러시 stroke가 큰 canvas 전체를 매번 복사하지 않는다. tablet press/move/release는 먼저
`TabletState`로 옮긴 뒤
`InputNormalizer`를 통해
`PointerDeviceKind::Tablet` 이벤트가 되며, pressure, tilt, rotation, eraser/barrel 상태를 stroke sample에 보존한다. 따라서 실제 펜 필압
jitter가
기본 brush dynamics의 size, flow, opacity 입력으로 전달된다. hardness는 tablet pressure와 무관하다. tablet tip contact는 `Qt::LeftButton`
플래그뿐 아니라 pressure가 0보다 큰
상태도 primary contact로 해석하므로, 장치/플랫폼이 tablet button 상태를 따로 채우지 않아도 필압 stroke가 시작되고 move 중에도 이어진다. Wacom처럼
`TabletPress`가 pressure 0과 button 없음으로 먼저 들어오고 이후 `TabletMove`에서 첫 pressure contact가 확인되는 경우에는 그 move가 stroke를 시작한다.
release는 hover로 분류하지 않아 contact가 0으로 끝나는 Wacom release도 stroke 완료로 들어간다. tablet stroke 중 또는 직후의 합성 mouse event는
pressure 1.0 sample로 섞이지 않게 무시한다. 단 QTabletEvent가 전혀 도착하지 않고 플랫폼이 펜을 synthesized mouse stream으로만 전달하는 경우에는 mouse
fallback을
차단하지 않아 적어도 그리기는 가능하게 한다. 이 fallback에서도 `QMouseEvent`의 `QEventPoint::pressure()`가 실제 값을 들고 있으면 tablet-like pointer로
변환해 같은 `PressureInput` 및 pressure dynamics 경로를 태운다. 해당 pressure point가 항상 1.0으로만 들어오는 플랫폼에서는 Qt mouse fallback만으로 필압을
복원하지 못한다.

`Stabilizer::smoothing`은 stroke 안정화 강도이며 0.0~1.0으로 clamp된다. line smoothing, cusp 보존, prediction, latency compensation,
adaptive resampling, dab preview 일치 플래그는 C++ 값 타입 공개 API로 제공된다. `PaintCanvasItem::stabilizerStrength`는 기본 smoothing 값을
QML에서 직접 바꾸는 사용자 설정용 API이다.

캔버스 이벤트의 무거운 계산은 `CanvasEventWork` 값 타입 job으로 분리한다. worker thread는 raw input, brush state, stabilizer, raster projection
snapshot만 받아 `LiveStrokeFrame`, `StrokeCommand`, `RasterSample`, dirty bounds를 계산한다. `PaintCanvasItem`의 `RasterLayer`,
`LiveStrokeBuffer`, QML property, `update(rect)` 호출은 GUI thread에서만 변경한다. live preview와 commit은 서로 다른 worker queue를 사용하고
각 queue는 순서를 보존하기 위해 1개 thread로 시작한다. 입력 단계는 모든 raw sample을 보존하고, 처리 단계는 timer frame마다 그 sample 묶음을
resampling/dab placement/projection으로 바꾼다. stroke 종료도 문서 이벤트를 직접 발생시키지 않고 pending commit queue에 request를 넣은 뒤 commit
frame에서
worker job으로 넘어간다. live preview는 실행 중인 job 1개와 최신 pending snapshot 1개만 유지해 move event가
많아져도 오래된 frame들이 queue에 쌓이지 않는다. 같은 live revision에서 끝난 frame은 최신 generation보다 조금 뒤처져도 화면에 반영하고, 이어서 최신 pending
snapshot을 다시 투영한다. release 시에는 live work만 무효화하고 live layer 픽셀은 commit 결과가 committed layer에 적용될 때까지 유지해 handoff 중
빈 프레임이 보이지 않게 한다. viewport/size/clear/release가 바뀌면 revision과 live generation으로 오래된 결과를 버린다.
dry brush의 live/commit request는 source `RasterLayer`를 복사하지 않는다. smudge, pickup, mixer처럼 source canvas sampling이 필요한
brush만
`sourceLayerEnabled`를 켜고 snapshot을 전달한다. worker 경로에서는 dab 배치와 device projection을 한 번만 수행하며, `PaintCanvasItem`은 stroke
event 중 QObject child나 QQuickItem child를 동적으로 붙이지 않는다.

## QML API

QML에서는 `registerIipeQmlTypes()`를 한 번 호출한 뒤 `iipe` 모듈을 가져온다. 기본 엔진 화면은 `Canvas` 타입이다. QML import alias는 대문자 식별자를
써야 하므로, namespace 표기가 필요하면 `import iipe 1.0 as Iipe` 뒤 `Iipe.Canvas`로 사용한다.

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
    brushSpacing: 0
    brushSpacingRatio: 0
    brushSpacingEnabled: true
    brushFlow: 1.0
    brushFlowEnabled: true
    brushOpacity: 1.0
    brushOpacityEnabled: true
    brushHardness: 1.0
    brushHardnessEnabled: true
    livePreviewEnabled: true
    livePreviewFrameIntervalMs: 8
    multithreadedEventsEnabled: true
}
```

`Canvas`는 엔진 중심 QML 진입점이다. viewport API는 `documentX`, `documentY`, `zoom`, `canvasDevicePixelRatio`,
`setDocumentViewport(x, y, zoom)`, `resetView()`, `panBy(dx, dy)`, `zoomAt(viewX, viewY, factor)`를 제공한다. 브러시 API는
`brushColor`, `brushSize`, `brushSpacing`, `brushSpacingRatio`, `brushSpacingEnabled`, `brushFlow`, `brushFlowEnabled`,
`brushOpacity`,
`brushOpacityEnabled`, `brushHardness`, `brushHardnessEnabled`,
`pressureCurveMinimum`, `pressureCurveCenter`, `pressureCurveMaximum`, `stabilizerStrength`,
`setBrush(size, color, flow, opacity)`를 제공한다. 편집/상태 API는 `clear()`, `livePreviewEnabled`, `livePreviewFrameIntervalMs`,
`multithreadedEventsEnabled`,
`liveStrokeActive`, `strokeCount`, `inputDevice`, `inputPressure`를 제공한다. `inputDevice`와 `inputPressure`는 마지막으로 수신한
pointer event가
mouse/tablet/touch 중 무엇이었고 pressure가 어떤 값으로 들어왔는지 예제와 디버깅 UI에서 읽기 위한 read-only 상태이다.
`canvasDevicePixelRatio`가 1보다 크면 내부 raster layer는 device pixel 크기로 유지하고, 화면 페인트 단계에서 QImage device pixel ratio를 적용한다.
따라서 QML pointer의 논리 좌표와 실제 stroke 표시 위치는 같은 지점에 남아야 한다. 이 계약은 `iiPaintEngineCanvasPointerAlignment` 테스트가 고정한다.

앱 통합층에서 `newCanvas/openRaster/saveToFile/undo/redo/toolMode`처럼 문서 조작과 도구 상태를 한 QML 객체에서 기대하는 경우에는 `CanvasAdapter`를 사용한다.
`CanvasAdapter`는 `PaintCanvasItem`을 상속하는 범용 어댑터이며 기존 `Canvas` API를 변경하지 않고 앱 친화적인 조작 명칭만 추가한다.

```qml
import QtQuick 2.15
import iipe 1.0 as Iipe

Iipe.CanvasAdapter {
    id: canvas
    anchors.fill: parent
    toolMode: "brush"

    Component.onCompleted: {
        newCanvas(1024, 768)
    }
}
```

`CanvasAdapter`는 `newCanvas(width, height)`, `openRaster(path)`, `saveToFile(path)`, `undo()`, `redo()`, `toolMode`,
`canUndo`, `canRedo`, `brushConfig`, `viewportConfig`, `runtimeConfig`, `stateSnapshot`을 제공한다. `openRaster`와
`saveToFile`은 local file path 또는 `file://` URL을 받으며, 별도 이미지 입출력 라이브러리를 추가하지 않고 이미 사용하는
Qt Gui의 `QImage` 포맷 처리를 사용한다. `toolMode`는 앱의 현재 도구 문자열을 보존하는 어댑터 상태이며, 엔진 내부 stroke pipeline의 기본 입력 경계는
계속 `PaintCanvasItem`에 남는다. `toolMode`가 `eraser`이면 `CanvasAdapter`는 `PaintCanvasItem::eraserMode`를 켜고 stroke sample을
destination-out 합성으로 적용해 대상 raster alpha를 낮춘다.
브러시 UI가 필요한 값은 `CanvasBrushConfig`로 묶어서 `CanvasAdapter::brushConfig`와 `setBrushConfig(config)`로 왕복한다. 이 config는 color,
size, flow, opacity, hardness, absolute spacing, spacing ratio, 각 stroke 인자 enabled 상태, pressure curve, stabilizer
strength를 담는다. 공개 기본값은 flow, opacity, hardness가 1.0(100%)이고 absolute spacing과 spacing ratio가 0.0(0%)이다.
viewport UI는 `CanvasViewportConfig`로 document origin, zoom, device pixel ratio, view size를 왕복하고,
runtime UI는 `CanvasRuntimeConfig`로 live preview, preview frame interval, multithreaded event 처리를 왕복한다.
`CanvasStateSnapshot`은 live stroke 여부, stroke count, 마지막 입력 장치/pressure, undo/redo 가능 여부, canvas size, tool mode를
read-only로
제공한다.
앱은 이 값 계약을 통해 브러시 패널과 preset UI를 만들 수 있지만, 내부 구현 타입인 `Rasterizer`, `BrushDynamics`, `BrushMaterial`, `StrokeCommand`를 직접
소유하지 않는다.

QML은 `Canvas` 또는 `CanvasAdapter`만 직접 다룬다. `InputStrokeBuilder`, `LiveStrokeBuffer`, `StrokeCommand`, `RasterProjection`,
`DirtyRegion`, `LayerStack` 같은 내부 구조는 C++ 엔진 경계 안에 남긴다.

## Example

`Example/Main.qml`은 LVRS의 `ApplicationWindow`, control component와 `iipe.Canvas`를 함께 쓰는 데모 페인팅 앱이다.
`iiPaintEngineExample`
target은 LVRS bootstrapped QML 앱으로 실행된다. macOS 빌드 산출물은 Finder에서 더블클릭 가능한 raw 실행 파일
`Example/bin/iiPaintEngineExample`에 놓인다. 이 실행 파일은 LVRS dylib 위치를 rpath로 가져 Finder/LaunchServices 환경에서도 실행된다. 앱은 현재 공개된
canvas viewport,
brush color, brush size, flow, opacity, hardness, spacing, 각 stroke 인자 enabled 토글, live preview, clear/reset view API와
마지막 입력 pressure
상태를 화면에서 바로 드러낸다. 필압 민감도는 min/center/max 그래프와 보조 슬라이더로 노출되고, 스태빌라이저 강도도 별도 슬라이더로 노출되어 사용자 맞춤형
필압/브러시 설정 UI의 공개 API를 검증한다.
예제의 `QC.Slider` 및 `BrushSlider` range는 qmlcache 컴파일이 통과하도록 항상 명시적인 `from`/`to` 속성으로 둔다.
`iiPaintEngineExampleDemoContract` 테스트는 예제 QML을 실제 엔진으로 로드하고 slider range, macOS raw 실행 파일 및 테스트 실행 파일의 LVRS rpath 산출
계약을 함께 검사한다.

## 설치

`install.sh`는 `build/`를 사용해 동적 라이브러리를 빌드하고 기본 prefix인 `~/.local/iiPaintEngine`에 설치한다. macOS host에서는 기본적으로
`macos,ios,android,wasm` 플랫폼 설치를 시도하며, 필수 Qt/LVRS/toolchain이 없는 cross platform은 자동 기본 설치에서는 건너뛰고 명시 요청 시에는
오류로 중단한다. host 플랫폼은 root prefix와 `platforms/<platform>` mirror에 함께 설치된다.

```sh
./install.sh
IIPAINTENGINE_INSTALL_PLATFORMS=macos ./install.sh
```

설치 후 CMake 소비자는 아래처럼 가져온다.

```cmake
find_package(iiPaintEngine CONFIG REQUIRED)
target_link_libraries(app PRIVATE iiPaintEngine::iiPaintEngine)
```

소비자 코드는 개별 모듈 헤더를 일일이 포함하지 않고 아래 단일 umbrella header로 공개 엔진 타입과 Qt adapter 타입을 사용할 수 있다.

```cpp
#include <iiPaintEngine>
```

macOS 산출물은 `~/.local/iiPaintEngine/lib/libiiPaintEngine.dylib`, Linux/Android 산출물은 각 prefix의 `lib/libiiPaintEngine.so`,
Windows 산출물은 `bin/iiPaintEngine.dll`이다. WASM 빌드는 Qt Core의 Emscripten runtime symbol을 위해 embind 링크 옵션을 포함한다.
플랫폼별 package config는 `~/.local/iiPaintEngine/platforms/<platform>` 아래에도 설치된다.

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
`iiPaintEngineAppDocumentApiContract` 테스트는 `PaintEngineController`/`DocumentAdapter`/`LayerListModel`을 통해 새 문서 생성,
레이어 조작, active layer stroke commit, history 기록, archive 저장/열기까지 앱/문서 API 경계에서 왕복되는지 검사한다.
`iiPaintEngineInstallLayoutContract` 테스트는 `install.sh`, CMake install/export 규칙, `iiPaintEngineConfig.cmake` 플랫폼
dispatch,
README 설치 문서가 같은 `~/.local/iiPaintEngine` 동적 라이브러리 설치 계약을 가리키는지 검사한다.
`iiPaintEnginePublicUmbrellaHeaderContract` 테스트는 외부 C++ 소비자가 `#include <iiPaintEngine>` 하나로 Core, Document, Canvas,
Layer, Stroke, Brush, Render, History, Input, Color, Selection, Transform, Filter, Tool, QtAdapter 공개 타입을 사용할 수 있는지
검사한다.
`iiPaintEnginePublicCxx17HeaderContract` 테스트는 C++17 소비자가 같은 공개 헤더를 포함하고 `BrushDabSpan` projection/bounds API를 사용할 수 있는지
검사한다.
`iiPaintEngineCanvasQmlApi` 테스트는 `registerIipeQmlTypes()`로 `iipe.Canvas`를 등록하고 QML에서 viewport, brush, live preview,
clear API와 마지막 입력 상태 read-only API를 하나의 객체로 사용할 수 있는지 검사한다.
`iiPaintEngineCanvasAdapterContract` 테스트는 `iipe.CanvasAdapter`가 범용 `newCanvas/openRaster/saveToFile/undo/redo/toolMode`
계약을 제공하고, Qt `QImage` 기반 raster open/save, adapter-level undo/redo, `CanvasBrushConfig`/`CanvasViewportConfig`/
`CanvasRuntimeConfig`/`CanvasStateSnapshot` 기반 facade가 같은 QML 타입에서 왕복되는지 검사한다.
`iiPaintEngineCanvasTabletPressureContract` 테스트는 Qt tablet event의 pressure jitter가 실제 canvas stroke의 농도, dab size, brush
dynamics로
전달되고, button flag 없이 pressure만 있는 tablet contact도 stroke로 인정되며, pressure 0의 tablet press 뒤 첫 pressure move가 stroke를
시작하고,
tablet 뒤 합성 mouse event가 필압 stroke를 덮지 않으며, QTabletEvent 없는 synthesized mouse fallback은 계속 그릴 수 있고
`QEventPoint::pressure()`를
가진 mouse fallback은 필압 dynamics를 타는지 검사한다.
`iiPaintEngineCanvasLivePreviewRealtimeContract` 테스트는 긴 stroke move burst 중에도 release 전에 최신 포인터 근처 live preview가 제한 시간
안에
그려지고, release 직후 commit worker 결과가 오기 전에도 해당 stroke가 사라지지 않는지 검사한다.
`iiPaintEnginePointerStrokeFlow` 테스트는 마우스 포인터만 스트로크를 완성하고, 벡터 스트로크 위에 브러시 알파 이미지가 flow/spacing에 따라 투영되는지 검사한다.
`iiPaintEnginePressureInputContract` 테스트는 `PressureInput`이 enabled flag, min/max calibration, contact, hover 상태를 유지한 채
기존
pressure normalization 결과를 보존하고 graph curve를 적용하는지 검사한다.
`iiPaintEngineTabletInputSurface` 테스트는 tablet hover, pressure normalization, barrel button, eraser, tilt calibration,
rotation, touch gesture event surface, pressure graph curve와 입력 기능별 enabled 스위치가 stroke 입력 경계에서 보존되거나 무시되어야 할 때 무시되는지
검사한다.
`iiPaintEngineHybridPaintingModel` 테스트는 샘플 velocity/tilt 보존, dab 배치, 브러시 투영, flow 누적과 opacity 상한 분리를 검사한다.
`iiPaintEngineStrokePhysicalContract` 테스트는 raw/rendered stroke 분리, 누적 arc length 기반 spacing, deterministic seed,
warm-up/taper, stroke dirty bounds를 검사한다.
`iiPaintEngineStrokeGeometryReportContract` 테스트는 stroke geometry report가 bounds, length, area, centroid, segment
heading/velocity,
sample turn/curvature, pressure/tilt 통계, principal axis를 계산하고 `StrokeCommand`와 `LiveStrokeFrame`에 raw/rendered geometry를
기록하는지
검사한다.
`iiPaintEngineStabilizerAdvancedContract` 테스트는 line smoothing, cusp 보존, prediction/latency compensation, adaptive
resampling, cursor preview와 dab preview의 일치, taper shape 제어를 검사한다.
`iiPaintEngineLiveStrokeRendering` 테스트는 pointer move 중 live buffer가 즉시 샘플을 만들고, 현재 tip을 raw 위치에 유지하며, release 뒤
committed layer로 넘어가는지 검사한다.
`iiPaintEngineBrushDynamicsMapping` 테스트는 pressure/velocity/tilt/random seed가 dab size, flow, opacity cap,
spacing, ellipse, texture direction, grain에 반영되는지 검사하며, pressure가 spacing 위치를 바꾸지 않는 계약도 함께 고정한다.
`iiPaintEngineBrushDynamicsResponseCurveContract` 테스트는 속성별 response curve의 min/center/max, easing, deterministic jitter,
pressure/velocity/tilt/random cross-input mapping, scatter/rotation/texture depth/wetness/dry-out/bristle spread 적용,
brush preset 왕복을 검사한다.
`iiPaintEngineBrushFeatureToggleContract` 테스트는 BrushDynamics와 Rasterizer의 공개 bool 스위치가 false일 때 pressure, velocity,
tilt,
random, 개별 매핑, flow, opacity, hardness, spacing이 dab 생성 및 brush mask 투영 인자로 쓰이지 않는지 검사하며, pressure가 hardness 공개
매핑을 갖지 않는 계약도 고정한다.
`iiPaintEngineBrushExpressionContract` 테스트는 texture/grain, dual brush, scatter, wet/smudge/mixer, bristle 값 계약과 공개
enabled 스위치, preset serialization, deterministic scatter 재현성을 검사한다.
`iiPaintEngineBrushTextureLayerContract` 테스트는 document/tip/stroke-follow/paper texture sampling, paper grain, dual brush
alpha composition, scale/rotation jitter, texture asset cache preset 왕복을 검사한다.
`iiPaintEngineWetBrushSimulationContract` 테스트는 source layer 픽셀을 샘플링해 smudge가 이전 캔버스 색을 끌고 오고,
pickup/deposit/mix가 브러시 안료와 캔버스 색을 섞으며, bristle shape/count/length/stiffness가 방향성 있는 접촉 ellipse를 만드는지 검사한다.
`iiPaintEngineHistoryUndoRedoContract` 테스트는 `Command`의 before/after patch payload, dirty bounds, sequence 부여, redo
branch clearing,
undo/redo stack 이동, history snapshot을 검사한다.
`iiPaintEngineEditingToolPipelineContract` 테스트는 rectangular selection, fill, linear gradient, eraser, crop, affine
transform,
blur/smudge filter pipeline, operation/filter/tool enabled 스위치, tool state begin/drag/commit/cancel 전이를 검사한다.
`iiPaintEngineStrokeResampler` 테스트는 Catmull-Rom rendered path가 raw input을 보존하면서 timestamp와 누적 arc length를 가진 보간 샘플을 만드는지
검사한다.
`iiPaintEngineStrokeCompositing` 테스트는 stroke-local buffer의 opacity cap, premultiplied source-over, layer commit 합성을
검사한다.
`iiPaintEngineLayerCompositingContract` 테스트는 layer stack의 multiply/screen/overlay blend mode, layer mask, clipping,
group child
composition, adjustment/alpha-lock 메타데이터 계약을 검사한다.
`iiPaintEngineRendererProjectionContract` 테스트는 `Renderer`가 CPU/GPU backend 선택, CPU fallback, layer stack projection, 색공간
호환성 오류를 처리하는지
검사한다.
`iiPaintEngineRenderCacheBackendContract` 테스트는 dirty region의 tile 분할, tile cache partial invalidation, brush stamp
atlas eviction, stroke replay cache key/revision/dirty invalidation, SIMD/GPU 실행 계획, 16-bit/float/HDR/wide gamut/ICC 기반
buffer
format과 linear compositing 선택을 검사한다.
`iiPaintEngineColorManagementContract` 테스트는 sRGB 8-bit, Display P3 linear float HDR, ICC profile, wide gamut/HDR 판별,
`PaintColor` HDR 값 보존과
SDR clamp 계약을 검사한다.
`iiPaintEngineBrushMaskSampling` 테스트는 기본 원형 브러시의 anti-aliased edge alpha, 브러시 마스크의 subpixel 위치, bilinear sampling,
rotation, soft hardness curve, 축소 dab의 픽셀 영역 샘플링을 검사한다.
`iiPaintEngineCoordinateDirtyRegion` 테스트는 view 입력이 document 좌표로 저장되고, 렌더링 때만 viewport projection이 적용되며, dab별 dirty
bounds가 stroke dirty region으로 묶이는지 검사한다.
`iiPaintEngineCanvasEventThreading` 테스트는 live/commit 캔버스 이벤트 계산이 Qt 객체 없이 값 타입 worker job으로 실행되고, 별도 thread에서 만든 sample과
dirty bounds를 GUI 적용 단계로 넘길 수 있는지 검사한다.
`iiPaintEngineCanvasEventLoopLoadContract` 테스트는 120개 move event 뒤에도 `PaintCanvasItem`의 QObject/QQuickItem child 수가 증가하지
않는지
검사한다. `iiPaintEngineCanvasInputBatchingContract` 테스트는 입력 이벤트 burst가 즉시 live render work로 바뀌지 않고 frame tick 뒤에 한 번의
preview 처리로 묶이는지 검사한다. `iiPaintEngineCanvasStrokePipelineSeparationContract` 테스트는 stroke release가 즉시 문서 commit/stroke
count 변경으로
이어지지 않고 deferred commit frame 뒤에만 반영되는지 검사한다. `iiPaintEngineCanvasEventThreading`은 dry brush worker request가 source
layer 복사 없이
projection하는 계약도 함께 검사한다.
