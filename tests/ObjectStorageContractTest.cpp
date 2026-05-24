#include <QObject>

#include <type_traits>

#include "Brush/BrushDynamics.h"
#include "Brush/BrushPreset.h"
#include "Brush/BrushResolve.h"
#include "Brush/BrushShape.h"
#include "Canvas/DrawingSurface.h"
#include "../Layer/Layer.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/Stroke.h"
#include "Stroke/StrokePoint.h"

namespace {

template <typename T>
constexpr bool publiclyInheritsQObject = std::is_convertible_v<T *, QObject *>;

template <typename T>
constexpr bool simpleStruct = !publiclyInheritsQObject<T>
        && std::is_aggregate_v<T>
        && std::is_copy_constructible_v<T>;

static_assert(simpleStruct<BrushSnapshot>);
static_assert(simpleStruct<BrushDynamics>);
static_assert(simpleStruct<BrushShape>);
static_assert(simpleStruct<BrushResolve>);
static_assert(simpleStruct<Rasterizer>);
static_assert(simpleStruct<Stabilizer>);
static_assert(simpleStruct<Stroke>);
static_assert(simpleStruct<StrokePoint>);

static_assert(publiclyInheritsQObject<DrawingSurface>);
static_assert(publiclyInheritsQObject<Layer>);

} // namespace

int main()
{
    BrushSnapshot brushSnapshot{};
    BrushDynamics brushDynamics{};
    BrushShape brushShape{};
    BrushResolve brushResolve{};
    Rasterizer rasterizer{};
    Stabilizer stabilizer{};
    Stroke stroke{};
    StrokePoint strokePoint{};

    DrawingSurface drawingSurface;
    Layer layer;

    QObject *canvasObjects[] = {
            &drawingSurface,
            &layer,
    };

    for (QObject *object : canvasObjects) {
        if (object == nullptr) {
            return 1;
        }
    }

    auto copiedStrokePoint = strokePoint;
    auto copiedStroke = stroke;

    (void) brushSnapshot;
    (void) brushDynamics;
    (void) brushShape;
    (void) brushResolve;
    (void) rasterizer;
    (void) stabilizer;
    (void) copiedStrokePoint;
    (void) copiedStroke;

    return 0;
}
