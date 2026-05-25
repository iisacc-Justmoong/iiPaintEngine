#include <array>
#include <cstdint>
#include <type_traits>

#include "Core/CoordinateSpace.h"
#include "Core/EngineConfig.h"
#include "Core/EngineError.h"
#include "Core/PaintPoint.h"
#include "Core/PaintRect.h"
#include "Core/PaintUuid.h"
#include "Core/Types.h"

namespace {

template <typename T>
constexpr bool coreValueType = std::is_aggregate_v<T>
        && std::is_copy_constructible_v<T>
        && std::is_standard_layout_v<T>;

static_assert(std::is_same_v<Types::Scalar, double>);
static_assert(std::is_same_v<Types::Pixel, std::int32_t>);
static_assert(std::is_same_v<Types::Byte, std::uint8_t>);

static_assert(coreValueType<PaintUuid>);
static_assert(sizeof(PaintUuid::bytes) == 16);

static_assert(coreValueType<CoordinateSpace>);
static_assert(DocumentCoordinateSpace::kind == CoordinateSpaceKind::Document);
static_assert(CanvasCoordinateSpace::kind == CoordinateSpaceKind::Canvas);
static_assert(ViewCoordinateSpace::kind == CoordinateSpaceKind::View);
static_assert(DevicePixelCoordinateSpace::kind == CoordinateSpaceKind::DevicePixel);

static_assert(coreValueType<DocumentPoint>);
static_assert(coreValueType<CanvasPoint>);
static_assert(coreValueType<ViewPoint>);
static_assert(coreValueType<DevicePixelPoint>);
static_assert(coreValueType<DocumentRect>);
static_assert(coreValueType<CanvasRect>);
static_assert(coreValueType<ViewRect>);
static_assert(coreValueType<DevicePixelRect>);

static_assert(std::is_same_v<DocumentPoint::Scalar, Types::Scalar>);
static_assert(std::is_same_v<CanvasPoint::Scalar, Types::Scalar>);
static_assert(std::is_same_v<ViewPoint::Scalar, Types::Scalar>);
static_assert(std::is_same_v<DevicePixelPoint::Scalar, Types::Pixel>);

static_assert(std::is_same_v<DocumentPoint, CanvasPoint>);
static_assert(!std::is_same_v<DocumentPoint, ViewPoint>);
static_assert(!std::is_same_v<DocumentPoint, DevicePixelPoint>);
static_assert(!std::is_assignable_v<DocumentPoint &, ViewPoint>);
static_assert(!std::is_assignable_v<ViewPoint &, DevicePixelPoint>);

static_assert(coreValueType<EngineError>);
static_assert(coreValueType<EngineConfig>);

} // namespace

int main()
{
    PaintUuid uuid{};
    DocumentPoint documentPoint{1.0, 2.0};
    CanvasPoint canvasPoint{1.0, 2.0};
    ViewPoint viewPoint{3.0, 4.0};
    DevicePixelPoint devicePoint{5, 6};
    DocumentRect documentRect{{0.0, 0.0}, 100.0, 200.0};
    CanvasRect canvasRect{{0.0, 0.0}, 100.0, 200.0};
    EngineError error{};
    EngineConfig config{};

    if (uuid.bytes != std::array<std::uint8_t, 16>{}) {
        return 1;
    }

    if (documentPoint.x != 1.0 || canvasPoint.x != 1.0 || viewPoint.y != 4.0 || devicePoint.x != 5) {
        return 1;
    }

    if (documentRect.width != 100.0
            || canvasRect.width != 100.0
            || error.code != EngineErrorCode::None
            || config.defaultDpi <= 0.0) {
        return 1;
    }

    return 0;
}
