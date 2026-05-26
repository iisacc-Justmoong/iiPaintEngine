#include <QObject>
#include <QQuickItem>

#include <type_traits>

#include "Brush/BrushDynamics.h"
#include "Brush/BrushLibrary.h"
#include "Brush/BrushMaterial.h"
#include "Brush/BrushPreset.h"
#include "Brush/BrushPresetSerializer.h"
#include "Brush/BrushResolve.h"
#include "Brush/BrushShape.h"
#include "Brush/BrushSnapshot.h"
#include "Brush/BrushTip.h"
#include "Canvas/Canvas.h"
#include "Canvas/CanvasMetadata.h"
#include "Canvas/CanvasSession.h"
#include "Canvas/CanvasState.h"
#include "Canvas/CanvasViewport.h"
#include "Color/ColorSpace.h"
#include "Color/Gradient.h"
#include "Color/PaintColor.h"
#include "Color/Palette.h"
#include "Core/CoordinateSpace.h"
#include "Core/EngineConfig.h"
#include "Core/EngineError.h"
#include "Core/PaintPoint.h"
#include "Core/PaintRect.h"
#include "Core/PaintUuid.h"
#include "Document/DocumentMetadata.h"
#include "Document/DocumentSerializer.h"
#include "Document/DocumentSnapshot.h"
#include "Document/PaintDocument.h"
#include "History/Command.h"
#include "History/HistorySnapshot.h"
#include "History/HistoryStack.h"
#include "History/UndoRedoController.h"
#include "Input/InputNormalizer.h"
#include "Input/InputStrokeBuilder.h"
#include "Input/PointerEvent.h"
#include "Input/PressureInput.h"
#include "Input/TabletState.h"
#include "Layer/DrawingSurface.h"
#include "Layer/Layer.h"
#include "Layer/LayerMetadata.h"
#include "Layer/LayerStack.h"
#include "Layer/RasterLayer.h"
#include "Layer/StrokeLayer.h"
#include "Layer/TextLayer.h"
#include "Layer/VectorLayer.h"
#include "QtAdapter/DocumentAdapter.h"
#include "QtAdapter/LayerListModel.h"
#include "QtAdapter/PaintCanvasItem.h"
#include "QtAdapter/PaintEngineController.h"
#include "Render/Compositor.h"
#include "Render/CpuRenderer.h"
#include "Render/DirtyRegion.h"
#include "Render/GpuRenderer.h"
#include "Render/RenderCache.h"
#include "Render/RenderContext.h"
#include "Render/Renderer.h"
#include "Selection/Selection.h"
#include "Stroke/LiveStroke.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/Stroke.h"
#include "Stroke/StrokeCommand.h"
#include "Stroke/StrokeCurve.h"
#include "Stroke/StrokeInput.h"
#include "Stroke/StrokePoint.h"
#include "Stroke/StrokeRepository.h"
#include "Stroke/StrokeResampler.h"
#include "Filter/FilterPipeline.h"
#include "Tool/RasterEditTool.h"
#include "Tool/ToolStateMachine.h"
#include "Transform/Transform.h"

namespace {

template <typename T>
constexpr bool publiclyInheritsQObject = std::is_convertible_v<T *, QObject *>;

template <typename T>
constexpr bool publiclyInheritsQuickItem = std::is_convertible_v<T *, QQuickItem *>;

template <typename T>
constexpr bool blueprintStruct = !publiclyInheritsQObject<T>
        && std::is_aggregate_v<T>
        && std::is_copy_constructible_v<T>;

static_assert(blueprintStruct<EngineConfig>);
static_assert(blueprintStruct<EngineError>);
static_assert(blueprintStruct<PaintUuid>);
static_assert(blueprintStruct<DocumentPoint>);
static_assert(blueprintStruct<CanvasPoint>);
static_assert(blueprintStruct<ViewPoint>);
static_assert(blueprintStruct<DevicePixelPoint>);
static_assert(blueprintStruct<DocumentRect>);
static_assert(blueprintStruct<CanvasRect>);
static_assert(blueprintStruct<ViewRect>);
static_assert(blueprintStruct<DevicePixelRect>);
static_assert(blueprintStruct<CoordinateSpace>);

static_assert(blueprintStruct<PaintDocument>);
static_assert(blueprintStruct<DocumentMetadata>);
static_assert(blueprintStruct<DocumentAsset>);
static_assert(blueprintStruct<DocumentArchive>);
static_assert(blueprintStruct<DocumentSnapshot>);
static_assert(blueprintStruct<DocumentSerializer>);

static_assert(blueprintStruct<Canvas>);
static_assert(blueprintStruct<CanvasMetadata>);
static_assert(blueprintStruct<CanvasState>);
static_assert(blueprintStruct<CanvasViewport>);
static_assert(blueprintStruct<CanvasSession>);

static_assert(blueprintStruct<Layer>);
static_assert(blueprintStruct<LayerMetadata>);
static_assert(blueprintStruct<LayerMask>);
static_assert(blueprintStruct<LayerStack>);
static_assert(blueprintStruct<DrawingSurface>);
static_assert(blueprintStruct<RasterLayer>);
static_assert(blueprintStruct<PremultipliedPixel>);
static_assert(blueprintStruct<StrokeCompositeBuffer>);
static_assert(blueprintStruct<StrokeLayer>);
static_assert(blueprintStruct<TextLayer>);
static_assert(blueprintStruct<VectorLayer>);

static_assert(blueprintStruct<Stroke>);
static_assert(blueprintStruct<StrokePoint>);
static_assert(blueprintStruct<StrokeInput>);
static_assert(blueprintStruct<Stabilizer>);
static_assert(blueprintStruct<StrokeCurve>);
static_assert(blueprintStruct<BrushDab>);
static_assert(blueprintStruct<StrokePath>);
static_assert(blueprintStruct<BrushState>);
static_assert(blueprintStruct<StrokeCommand>);
static_assert(blueprintStruct<LiveStrokeFrame>);
static_assert(blueprintStruct<LiveStrokeBuffer>);
static_assert(blueprintStruct<StrokeRepository>);
static_assert(blueprintStruct<StrokeResampler>);
static_assert(blueprintStruct<Rasterizer>);

static_assert(blueprintStruct<BrushPreset>);
static_assert(blueprintStruct<BrushTextureAssetCache>);
static_assert(blueprintStruct<BrushTexture>);
static_assert(blueprintStruct<DualBrush>);
static_assert(blueprintStruct<BrushScatter>);
static_assert(blueprintStruct<BrushSimulation>);
static_assert(blueprintStruct<BristleSimulation>);
static_assert(blueprintStruct<BrushMaterial>);
static_assert(blueprintStruct<BrushPresetSerializer>);
static_assert(blueprintStruct<BrushDynamicsResponseCurve>);
static_assert(blueprintStruct<BrushDynamicsPropertyResponse>);
static_assert(blueprintStruct<BrushDynamics>);
static_assert(blueprintStruct<BrushDynamicsInput>);
static_assert(blueprintStruct<BrushDynamicsResult>);
static_assert(blueprintStruct<BrushShape>);
static_assert(blueprintStruct<BrushTip>);
static_assert(blueprintStruct<BrushLibrary>);
static_assert(blueprintStruct<BrushSnapshot>);
static_assert(blueprintStruct<BrushResolve>);

static_assert(blueprintStruct<Renderer>);
static_assert(blueprintStruct<RenderContext>);
static_assert(blueprintStruct<RenderExecutionPlan>);
static_assert(blueprintStruct<DirtyRegion>);
static_assert(blueprintStruct<RenderTileKey>);
static_assert(blueprintStruct<RenderTile>);
static_assert(blueprintStruct<RenderTileCache>);
static_assert(blueprintStruct<BrushStampAtlasKey>);
static_assert(blueprintStruct<BrushStampAtlasEntry>);
static_assert(blueprintStruct<BrushStampAtlas>);
static_assert(blueprintStruct<StrokeReplayCacheKey>);
static_assert(blueprintStruct<StrokeReplayCacheEntry>);
static_assert(blueprintStruct<StrokeReplayCache>);
static_assert(blueprintStruct<Compositor>);
static_assert(blueprintStruct<CpuRenderer>);
static_assert(blueprintStruct<GpuRenderer>);

static_assert(blueprintStruct<Command>);
static_assert(blueprintStruct<CommandPayload>);
static_assert(blueprintStruct<CommandPatch>);
static_assert(blueprintStruct<HistoryStack>);
static_assert(blueprintStruct<HistoryStepResult>);
static_assert(blueprintStruct<UndoRedoController>);
static_assert(blueprintStruct<HistorySnapshot>);

static_assert(blueprintStruct<PointerEvent>);
static_assert(blueprintStruct<PressureInput>);
static_assert(blueprintStruct<TouchGestureState>);
static_assert(blueprintStruct<TabletTiltCalibration>);
static_assert(blueprintStruct<TabletState>);
static_assert(blueprintStruct<InputNormalizer>);
static_assert(blueprintStruct<InputStrokeBuilder>);

static_assert(blueprintStruct<PaintColor>);
static_assert(blueprintStruct<Palette>);
static_assert(blueprintStruct<ColorChromaticity>);
static_assert(blueprintStruct<ColorSpace>);
static_assert(blueprintStruct<Gradient>);

static_assert(blueprintStruct<SelectionMask>);
static_assert(blueprintStruct<SelectionState>);
static_assert(blueprintStruct<AffineTransform>);
static_assert(blueprintStruct<TransformState>);
static_assert(blueprintStruct<FilterNode>);
static_assert(blueprintStruct<FilterPipeline>);
static_assert(blueprintStruct<FillOperation>);
static_assert(blueprintStruct<GradientOperation>);
static_assert(blueprintStruct<EraserOperation>);
static_assert(blueprintStruct<ToolState>);
static_assert(blueprintStruct<ToolStateMachine>);

static_assert(blueprintStruct<PaintEngineController>);
static_assert(blueprintStruct<DocumentAdapter>);
static_assert(blueprintStruct<LayerListModel>);
static_assert(publiclyInheritsQuickItem<PaintCanvasItem>);
static_assert(publiclyInheritsQObject<PaintCanvasItem>);

} // namespace

int main()
{
    PaintDocument document{};
    Canvas canvasValue{};
    CanvasSession canvas{};
    DrawingSurface surface{};
    LayerStack layers{};
    StrokeRepository strokeRepository{};
    Stroke stroke{};
    BrushPreset brush{};
    RenderContext renderContext{};
    HistoryStack history{};
    PointerEvent pointerEvent{};
    PaintColor color{};
    SelectionState selection{};
    ToolStateMachine tools{};

    PaintCanvasItem canvasItem;
    PaintEngineController controller{};
    DocumentAdapter documentAdapter{};
    LayerListModel layerListModel{};

    QObject *canvasObject = &canvasItem;
    if (canvasObject == nullptr) {
        return 1;
    }

    (void) document;
    (void) canvasValue;
    (void) canvas;
    (void) surface;
    (void) layers;
    (void) strokeRepository;
    (void) stroke;
    (void) brush;
    (void) renderContext;
    (void) history;
    (void) pointerEvent;
    (void) color;
    (void) selection;
    (void) tools;
    (void) controller;
    (void) documentAdapter;
    (void) layerListModel;

    return 0;
}
