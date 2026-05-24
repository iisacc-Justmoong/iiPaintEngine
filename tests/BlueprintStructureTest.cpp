#include <QObject>
#include <QQuickItem>

#include <type_traits>

#include "Brush/BrushDynamics.h"
#include "Brush/BrushLibrary.h"
#include "Brush/BrushPreset.h"
#include "Brush/BrushResolve.h"
#include "Brush/BrushShape.h"
#include "Brush/BrushSnapshot.h"
#include "Brush/BrushTip.h"
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
#include "Input/TabletState.h"
#include "Layer/Layer.h"
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
#include "Render/RenderContext.h"
#include "Render/Renderer.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/Stroke.h"
#include "Stroke/StrokeCurve.h"
#include "Stroke/StrokeInput.h"
#include "Stroke/StrokePoint.h"

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
static_assert(blueprintStruct<PaintPoint>);
static_assert(blueprintStruct<PaintRect>);
static_assert(blueprintStruct<CoordinateSpace>);

static_assert(blueprintStruct<PaintDocument>);
static_assert(blueprintStruct<DocumentMetadata>);
static_assert(blueprintStruct<DocumentSnapshot>);
static_assert(blueprintStruct<DocumentSerializer>);

static_assert(blueprintStruct<Layer>);
static_assert(blueprintStruct<LayerStack>);
static_assert(blueprintStruct<RasterLayer>);
static_assert(blueprintStruct<StrokeLayer>);
static_assert(blueprintStruct<TextLayer>);
static_assert(blueprintStruct<VectorLayer>);

static_assert(blueprintStruct<Stroke>);
static_assert(blueprintStruct<StrokePoint>);
static_assert(blueprintStruct<StrokeInput>);
static_assert(blueprintStruct<Stabilizer>);
static_assert(blueprintStruct<StrokeCurve>);
static_assert(blueprintStruct<Rasterizer>);

static_assert(blueprintStruct<BrushPreset>);
static_assert(blueprintStruct<BrushDynamics>);
static_assert(blueprintStruct<BrushShape>);
static_assert(blueprintStruct<BrushTip>);
static_assert(blueprintStruct<BrushLibrary>);
static_assert(blueprintStruct<BrushSnapshot>);
static_assert(blueprintStruct<BrushResolve>);

static_assert(blueprintStruct<Renderer>);
static_assert(blueprintStruct<RenderContext>);
static_assert(blueprintStruct<DirtyRegion>);
static_assert(blueprintStruct<Compositor>);
static_assert(blueprintStruct<CpuRenderer>);
static_assert(blueprintStruct<GpuRenderer>);

static_assert(blueprintStruct<Command>);
static_assert(blueprintStruct<HistoryStack>);
static_assert(blueprintStruct<UndoRedoController>);
static_assert(blueprintStruct<HistorySnapshot>);

static_assert(blueprintStruct<PointerEvent>);
static_assert(blueprintStruct<TabletState>);
static_assert(blueprintStruct<InputNormalizer>);
static_assert(blueprintStruct<InputStrokeBuilder>);

static_assert(blueprintStruct<PaintColor>);
static_assert(blueprintStruct<Palette>);
static_assert(blueprintStruct<ColorSpace>);
static_assert(blueprintStruct<Gradient>);

static_assert(blueprintStruct<PaintEngineController>);
static_assert(blueprintStruct<DocumentAdapter>);
static_assert(blueprintStruct<LayerListModel>);
static_assert(publiclyInheritsQuickItem<PaintCanvasItem>);
static_assert(publiclyInheritsQObject<PaintCanvasItem>);

} // namespace

int main()
{
    PaintDocument document{};
    LayerStack layers{};
    Stroke stroke{};
    BrushPreset brush{};
    RenderContext renderContext{};
    HistoryStack history{};
    PointerEvent pointerEvent{};
    PaintColor color{};

    PaintCanvasItem canvasItem;
    PaintEngineController controller{};
    DocumentAdapter documentAdapter{};
    LayerListModel layerListModel{};

    QObject *canvasObject = &canvasItem;
    if (canvasObject == nullptr) {
        return 1;
    }

    (void) document;
    (void) layers;
    (void) stroke;
    (void) brush;
    (void) renderContext;
    (void) history;
    (void) pointerEvent;
    (void) color;
    (void) controller;
    (void) documentAdapter;
    (void) layerListModel;

    return 0;
}
