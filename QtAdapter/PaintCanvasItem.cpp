//
// Created by Justmoong on 2026 May 24.
//

#include "PaintCanvasItem.h"

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPointingDevice>
#include <QRect>
#include <QTabletEvent>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <utility>
#include <vector>

#include "Layer/RasterLayer.h"
#include "Input/PressureInput.h"
#include "Render/DirtyRegion.h"
#include "Stroke/Rasterizer.h"

namespace {

Types::Scalar safePixelRatio(Types::Scalar value)
{
    return std::max<Types::Scalar>(0.01, value);
}

Types::Scalar unitSettingOrDefault(Types::Scalar value, Types::Scalar fallback)
{
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, 0.0, 1.0);
}

void drawRasterLayer(QPainter *painter, const RasterLayer &layer, Types::Scalar devicePixelRatio)
{
    if (layer.width <= 0 || layer.height <= 0 || layer.pixels.empty()) {
        return;
    }

    QImage image(reinterpret_cast<const uchar *>(layer.pixels.data()),
                 layer.width,
                 layer.height,
                 static_cast<qsizetype>(layer.width) * 4,
                 QImage::Format_ARGB32);
    image.setDevicePixelRatio(safePixelRatio(devicePixelRatio));
    painter->drawImage(QPointF{0.0, 0.0}, image);
}

void drawLiveRasterLayer(QPainter *painter,
                         const RasterLayer &layer,
                         Types::Scalar devicePixelRatio,
                         bool destinationOut)
{
    if (!destinationOut) {
        drawRasterLayer(painter, layer, devicePixelRatio);
        return;
    }

    painter->save();
    painter->setCompositionMode(QPainter::CompositionMode_DestinationOut);
    drawRasterLayer(painter, layer, devicePixelRatio);
    painter->restore();
}

void clearRasterLayer(RasterLayer &layer)
{
    std::fill(layer.pixels.begin(), layer.pixels.end(), 0x00000000U);
}

void clearRasterLayerRect(RasterLayer &layer, DevicePixelRect dirtyBounds)
{
    const DevicePixelRect layerRect{{0, 0}, layer.width, layer.height};
    const DevicePixelRect clipped = intersectDevicePixelRects(layerRect, dirtyBounds);
    if (isEmpty(clipped)) {
        return;
    }

    for (Types::Pixel y = clipped.origin.y; y < clipped.origin.y + clipped.height; ++y) {
        const std::size_t rowStart = static_cast<std::size_t>(y) * static_cast<std::size_t>(layer.width);
        for (Types::Pixel x = clipped.origin.x; x < clipped.origin.x + clipped.width; ++x) {
            layer.pixels[rowStart + static_cast<std::size_t>(x)] = 0x00000000U;
        }
    }
}

std::uint32_t sampleRasterLayerArgb(const void *context, DevicePixelPoint position)
{
    const auto *layer = static_cast<const RasterLayer *>(context);
    if (layer == nullptr) {
        return 0x00000000U;
    }

    return rasterLayerPixelAt(*layer, position);
}

RasterSourceSampler sourceSamplerForLayer(const RasterLayer &layer)
{
    RasterSourceSampler sampler;
    sampler.context = &layer;
    sampler.sampleArgb = &sampleRasterLayerArgb;
    sampler.width = layer.width;
    sampler.height = layer.height;
    return sampler;
}

QImage imageFromRasterLayer(const RasterLayer &layer)
{
    if (layer.width <= 0 || layer.height <= 0) {
        return {};
    }

    QImage image(layer.width, layer.height, QImage::Format_ARGB32);
    for (Types::Pixel y = 0; y < layer.height; ++y) {
        for (Types::Pixel x = 0; x < layer.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(layer.width)
                    + static_cast<std::size_t>(x);
            image.setPixel(x, y, static_cast<QRgb>(layer.pixels[index]));
        }
    }
    return image;
}

bool samplesContainDestinationOut(const std::vector<RasterSample> &samples)
{
    return std::any_of(samples.begin(), samples.end(), [](const RasterSample &sample) {
        return sample.blendMode == RasterBlendMode::DestinationOut;
    });
}

std::vector<RasterSample> destinationOutSamplesAsSourceMask(const std::vector<RasterSample> &samples)
{
    std::vector<RasterSample> maskSamples;
    maskSamples.reserve(samples.size());
    for (RasterSample sample : samples) {
        if (sample.blendMode == RasterBlendMode::DestinationOut) {
            sample.blendMode = RasterBlendMode::SourceOver;
            sample.argb = 0xFF000000U & sample.argb;
            sample.opacityCap = 0xFFU;
        }
        maskSamples.push_back(sample);
    }
    return maskSamples;
}

Types::Pixel itemPixelSize(qreal value, Types::Scalar devicePixelRatio)
{
    return std::max<Types::Pixel>(
            0,
            static_cast<Types::Pixel>(std::ceil(static_cast<Types::Scalar>(value)
                                                * safePixelRatio(devicePixelRatio))));
}

PointerButton pointerButtonFromMouseButton(Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        return PointerButton::Primary;
    }

    if (button == Qt::RightButton) {
        return PointerButton::Secondary;
    }

    if (button == Qt::MiddleButton) {
        return PointerButton::Middle;
    }

    return PointerButton::None;
}

QRect qRectFromDeviceRect(DevicePixelRect rect, Types::Scalar devicePixelRatio)
{
    const Types::Scalar ratio = safePixelRatio(devicePixelRatio);
    const auto left = static_cast<int>(std::floor(static_cast<Types::Scalar>(rect.origin.x) / ratio));
    const auto top = static_cast<int>(std::floor(static_cast<Types::Scalar>(rect.origin.y) / ratio));
    const auto right = static_cast<int>(std::ceil(static_cast<Types::Scalar>(rect.origin.x + rect.width) / ratio));
    const auto bottom = static_cast<int>(std::ceil(static_cast<Types::Scalar>(rect.origin.y + rect.height) / ratio));
    return QRect(left, top, std::max(0, right - left), std::max(0, bottom - top));
}

DevicePixelRect deviceRectFromDocumentRect(DocumentRect rect, const RasterProjection &projection)
{
    if (rect.width <= 0.0 || rect.height <= 0.0) {
        return {};
    }

    const Types::Scalar scale = std::max<Types::Scalar>(0.01, projection.scale);
    const Types::Scalar left = static_cast<Types::Scalar>(projection.deviceOrigin.x)
            + (rect.origin.x - projection.documentOrigin.x) * scale;
    const Types::Scalar top = static_cast<Types::Scalar>(projection.deviceOrigin.y)
            + (rect.origin.y - projection.documentOrigin.y) * scale;
    const Types::Scalar right = static_cast<Types::Scalar>(projection.deviceOrigin.x)
            + (rect.origin.x + rect.width - projection.documentOrigin.x) * scale;
    const Types::Scalar bottom = static_cast<Types::Scalar>(projection.deviceOrigin.y)
            + (rect.origin.y + rect.height - projection.documentOrigin.y) * scale;
    const Types::Pixel pixelLeft = static_cast<Types::Pixel>(std::floor(left));
    const Types::Pixel pixelTop = static_cast<Types::Pixel>(std::floor(top));
    const Types::Pixel pixelRight = static_cast<Types::Pixel>(std::ceil(right));
    const Types::Pixel pixelBottom = static_cast<Types::Pixel>(std::ceil(bottom));
    return DevicePixelRect{
            {pixelLeft, pixelTop},
            std::max<Types::Pixel>(0, pixelRight - pixelLeft + 1),
            std::max<Types::Pixel>(0, pixelBottom - pixelTop + 1),
    };
}

std::uint32_t argbFromColor(const QColor &color)
{
    const QColor resolved = color.isValid() ? color : QColor{Qt::black};
    return static_cast<std::uint32_t>(resolved.rgba());
}

bool isTabletEraser(const QPointingDevice *device)
{
    return device != nullptr && device->pointerType() == QPointingDevice::PointerType::Eraser;
}

bool isTabletPointerDevice(const QPointingDevice *device)
{
    if (device == nullptr) {
        return false;
    }

    const QPointingDevice::PointerType pointerType = device->pointerType();
    return pointerType == QPointingDevice::PointerType::Pen
            || pointerType == QPointingDevice::PointerType::Eraser
            || pointerType == QPointingDevice::PointerType::Cursor;
}

Types::Scalar eventPointPressure(const QMouseEvent *event)
{
    if (event->points().isEmpty()) {
        return 1.0;
    }

    return std::clamp(static_cast<Types::Scalar>(event->points().first().pressure()), 0.0, 1.0);
}

Types::Scalar eventPointRotation(const QMouseEvent *event)
{
    if (event->points().isEmpty()) {
        return 0.0;
    }

    return static_cast<Types::Scalar>(event->points().first().rotation());
}

bool hasVariableMousePressure(const QMouseEvent *event)
{
    if (event->points().isEmpty()) {
        return false;
    }

    return pressureInputHasVariablePressure(eventPointPressure(event));
}

bool isPressureAwareMouseEvent(const QMouseEvent *event)
{
    return isTabletPointerDevice(event->pointingDevice())
            || event->source() != Qt::MouseEventNotSynthesized
            || hasVariableMousePressure(event);
}

BrushDynamics pressureSensitiveDynamics()
{
    BrushDynamics dynamics;
    dynamics.pressureToSize = 1.0;
    dynamics.pressureToFlow = 1.0;
    dynamics.pressureToOpacity = 1.0;
    return dynamics;
}

} // namespace

PaintCanvasItem::PaintCanvasItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setAntialiasing(false);
    m_rasterizer.spacing = 0.0;
    m_rasterizer.spacingRatio = 0.0;
    m_rasterizer.flow = 1.0;
}

PaintCanvasItem::~PaintCanvasItem() = default;

qreal PaintCanvasItem::documentX() const
{
    return m_documentOrigin.x;
}

void PaintCanvasItem::setDocumentX(qreal value)
{
    if (m_documentOrigin.x == value) {
        return;
    }

    const bool wasLiveStrokeActive = liveStrokeActive();
    m_documentOrigin.x = static_cast<Types::Scalar>(value);
    cancelActiveRasterStroke();
    updateViewportGeometry();
    emit viewportChanged();
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    update();
}

qreal PaintCanvasItem::documentY() const
{
    return m_documentOrigin.y;
}

void PaintCanvasItem::setDocumentY(qreal value)
{
    if (m_documentOrigin.y == value) {
        return;
    }

    const bool wasLiveStrokeActive = liveStrokeActive();
    m_documentOrigin.y = static_cast<Types::Scalar>(value);
    cancelActiveRasterStroke();
    updateViewportGeometry();
    emit viewportChanged();
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    update();
}

qreal PaintCanvasItem::zoom() const
{
    return m_zoom;
}

void PaintCanvasItem::setZoom(qreal value)
{
    const Types::Scalar nextZoom = std::max<Types::Scalar>(0.01, static_cast<Types::Scalar>(value));
    if (m_zoom == nextZoom) {
        return;
    }

    const bool wasLiveStrokeActive = liveStrokeActive();
    m_zoom = nextZoom;
    cancelActiveRasterStroke();
    updateViewportGeometry();
    emit viewportChanged();
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    update();
}

qreal PaintCanvasItem::canvasDevicePixelRatio() const
{
    return m_devicePixelRatio;
}

void PaintCanvasItem::setCanvasDevicePixelRatio(qreal value)
{
    const Types::Scalar nextRatio = std::max<Types::Scalar>(0.01, static_cast<Types::Scalar>(value));
    if (m_devicePixelRatio == nextRatio) {
        return;
    }

    const bool wasLiveStrokeActive = liveStrokeActive();
    m_devicePixelRatio = nextRatio;
    cancelActiveRasterStroke();
    updateViewportGeometry();
    emit viewportChanged();
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    update();
}

QColor PaintCanvasItem::brushColor() const
{
    return QColor::fromRgba(m_rasterizer.argb);
}

void PaintCanvasItem::setBrushColor(const QColor &color)
{
    const std::uint32_t nextColor = argbFromColor(color);
    if (m_rasterizer.argb == nextColor) {
        return;
    }

    m_rasterizer.argb = nextColor;
    emit brushChanged();
}

qreal PaintCanvasItem::brushSize() const
{
    if (m_rasterizer.brushSize > 0.0) {
        return m_rasterizer.brushSize;
    }

    return static_cast<qreal>(m_rasterizer.radius) * 2.0;
}

void PaintCanvasItem::setBrushSize(qreal value)
{
    const Types::Scalar nextSize = std::max<Types::Scalar>(1.0, static_cast<Types::Scalar>(value));
    if (m_rasterizer.brushSize == nextSize) {
        return;
    }

    m_rasterizer.brushSize = nextSize;
    m_rasterizer.radius = std::max<Types::Pixel>(1, static_cast<Types::Pixel>(std::lround(nextSize * 0.5)));
    emit brushChanged();
}

qreal PaintCanvasItem::brushSpacing() const
{
    return m_rasterizer.spacing;
}

void PaintCanvasItem::setBrushSpacing(qreal value)
{
    const Types::Scalar nextSpacing = std::max<Types::Scalar>(0.0, static_cast<Types::Scalar>(value));
    if (m_rasterizer.spacing == nextSpacing) {
        return;
    }

    m_rasterizer.spacing = nextSpacing;
    emit brushChanged();
}

qreal PaintCanvasItem::brushSpacingRatio() const
{
    return m_rasterizer.spacingRatio;
}

void PaintCanvasItem::setBrushSpacingRatio(qreal value)
{
    const Types::Scalar nextRatio = std::clamp(static_cast<Types::Scalar>(value), 0.0, 1.0);
    if (m_rasterizer.spacingRatio == nextRatio) {
        return;
    }

    m_rasterizer.spacingRatio = nextRatio;
    emit brushChanged();
}

bool PaintCanvasItem::brushSpacingEnabled() const
{
    return m_rasterizer.spacingEnabled;
}

void PaintCanvasItem::setBrushSpacingEnabled(bool enabled)
{
    if (m_rasterizer.spacingEnabled == enabled) {
        return;
    }

    m_rasterizer.spacingEnabled = enabled;
    emit brushChanged();
}

qreal PaintCanvasItem::brushFlow() const
{
    return m_rasterizer.flow;
}

void PaintCanvasItem::setBrushFlow(qreal value)
{
    const Types::Scalar nextFlow = std::clamp(static_cast<Types::Scalar>(value), 0.0, 1.0);
    if (m_rasterizer.flow == nextFlow) {
        return;
    }

    m_rasterizer.flow = nextFlow;
    emit brushChanged();
}

bool PaintCanvasItem::brushFlowEnabled() const
{
    return m_rasterizer.flowEnabled;
}

void PaintCanvasItem::setBrushFlowEnabled(bool enabled)
{
    if (m_rasterizer.flowEnabled == enabled) {
        return;
    }

    m_rasterizer.flowEnabled = enabled;
    emit brushChanged();
}

qreal PaintCanvasItem::brushOpacity() const
{
    return m_rasterizer.opacity;
}

void PaintCanvasItem::setBrushOpacity(qreal value)
{
    const Types::Scalar nextOpacity = std::clamp(static_cast<Types::Scalar>(value), 0.0, 1.0);
    if (m_rasterizer.opacity == nextOpacity) {
        return;
    }

    m_rasterizer.opacity = nextOpacity;
    emit brushChanged();
}

bool PaintCanvasItem::brushOpacityEnabled() const
{
    return m_rasterizer.opacityEnabled;
}

void PaintCanvasItem::setBrushOpacityEnabled(bool enabled)
{
    if (m_rasterizer.opacityEnabled == enabled) {
        return;
    }

    m_rasterizer.opacityEnabled = enabled;
    emit brushChanged();
}

qreal PaintCanvasItem::brushHardness() const
{
    return m_rasterizer.hardness;
}

void PaintCanvasItem::setBrushHardness(qreal value)
{
    const Types::Scalar nextHardness = std::clamp(static_cast<Types::Scalar>(value), 0.01, 1.0);
    if (m_rasterizer.hardness == nextHardness) {
        return;
    }

    m_rasterizer.hardness = nextHardness;
    emit brushChanged();
}

bool PaintCanvasItem::brushHardnessEnabled() const
{
    return m_rasterizer.hardnessEnabled;
}

void PaintCanvasItem::setBrushHardnessEnabled(bool enabled)
{
    if (m_rasterizer.hardnessEnabled == enabled) {
        return;
    }

    m_rasterizer.hardnessEnabled = enabled;
    emit brushChanged();
}

bool PaintCanvasItem::eraserMode() const
{
    return m_eraserMode;
}

void PaintCanvasItem::setEraserMode(bool enabled)
{
    if (m_eraserMode == enabled) {
        return;
    }

    m_eraserMode = enabled;
    emit brushChanged();
}

qreal PaintCanvasItem::pressureCurveMinimum() const
{
    return m_inputNormalizer.pressureCurveMinimum;
}

void PaintCanvasItem::setPressureCurveMinimum(qreal value)
{
    const Types::Scalar nextMinimum = unitSettingOrDefault(static_cast<Types::Scalar>(value), 0.0);
    const Types::Scalar nextCenter = std::max(m_inputNormalizer.pressureCurveCenter, nextMinimum);
    const Types::Scalar nextMaximum = std::max(m_inputNormalizer.pressureCurveMaximum, nextMinimum);
    if (m_inputNormalizer.pressureCurveMinimum == nextMinimum
            && m_inputNormalizer.pressureCurveCenter == nextCenter
            && m_inputNormalizer.pressureCurveMaximum == nextMaximum) {
        return;
    }

    m_inputNormalizer.pressureCurveMinimum = nextMinimum;
    m_inputNormalizer.pressureCurveCenter = nextCenter;
    m_inputNormalizer.pressureCurveMaximum = nextMaximum;
    emit strokeSettingsChanged();
}

qreal PaintCanvasItem::pressureCurveCenter() const
{
    return m_inputNormalizer.pressureCurveCenter;
}

void PaintCanvasItem::setPressureCurveCenter(qreal value)
{
    const Types::Scalar nextCenter = std::clamp(
            unitSettingOrDefault(static_cast<Types::Scalar>(value), 0.5),
            m_inputNormalizer.pressureCurveMinimum,
            m_inputNormalizer.pressureCurveMaximum);
    if (m_inputNormalizer.pressureCurveCenter == nextCenter) {
        return;
    }

    m_inputNormalizer.pressureCurveCenter = nextCenter;
    emit strokeSettingsChanged();
}

qreal PaintCanvasItem::pressureCurveMaximum() const
{
    return m_inputNormalizer.pressureCurveMaximum;
}

void PaintCanvasItem::setPressureCurveMaximum(qreal value)
{
    const Types::Scalar nextMaximum = unitSettingOrDefault(static_cast<Types::Scalar>(value), 1.0);
    const Types::Scalar nextMinimum = std::min(m_inputNormalizer.pressureCurveMinimum, nextMaximum);
    const Types::Scalar nextCenter = std::clamp(m_inputNormalizer.pressureCurveCenter,
                                                nextMinimum,
                                                nextMaximum);
    if (m_inputNormalizer.pressureCurveMinimum == nextMinimum
            && m_inputNormalizer.pressureCurveCenter == nextCenter
            && m_inputNormalizer.pressureCurveMaximum == nextMaximum) {
        return;
    }

    m_inputNormalizer.pressureCurveMinimum = nextMinimum;
    m_inputNormalizer.pressureCurveCenter = nextCenter;
    m_inputNormalizer.pressureCurveMaximum = nextMaximum;
    emit strokeSettingsChanged();
}

bool PaintCanvasItem::pressureToOpacityEnabled() const
{
    return m_pressureToOpacityEnabled;
}

void PaintCanvasItem::setPressureToOpacityEnabled(bool enabled)
{
    if (m_pressureToOpacityEnabled == enabled) {
        return;
    }

    m_pressureToOpacityEnabled = enabled;
    emit brushChanged();
}

bool PaintCanvasItem::livePreviewEnabled() const
{
    return m_livePreviewEnabled;
}

void PaintCanvasItem::setLivePreviewEnabled(bool enabled)
{
    if (m_livePreviewEnabled == enabled) {
        return;
    }

    m_livePreviewEnabled = enabled;
    update();
    emit livePreviewEnabledChanged();
}

bool PaintCanvasItem::liveStrokeActive() const
{
    return m_strokeBuilder.active;
}

int PaintCanvasItem::strokeCount() const
{
    return m_committedStrokeCount;
}

QString PaintCanvasItem::inputDevice() const
{
    switch (m_lastInputDevice) {
        case PointerDeviceKind::Tablet:
            return QStringLiteral("tablet");
        case PointerDeviceKind::Touch:
            return QStringLiteral("touch");
        case PointerDeviceKind::Mouse:
            return QStringLiteral("mouse");
    }

    return QStringLiteral("mouse");
}

qreal PaintCanvasItem::inputPressure() const
{
    return m_lastInputPressure;
}

void PaintCanvasItem::paint(QPainter *painter)
{
    ensureRasterLayerSize();
    drawRasterLayer(painter, m_rasterLayer, m_devicePixelRatio);
    if (m_livePreviewEnabled && liveStrokeActive()) {
        drawLiveRasterLayer(painter, m_liveRasterLayer, m_devicePixelRatio, m_liveStrokePreviewDestinationOut);
    }
}

void PaintCanvasItem::clear()
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    ensureRasterLayerSize();
    recordRasterChange();
    cancelActiveRasterStroke();
    clearRasterLayer(m_rasterLayer);
    clearPendingRasterStroke();
    m_nextStrokeSeed = 1;
    m_committedStrokeCount = 0;
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    emit strokeCountChanged();
    update();
}

void PaintCanvasItem::setDocumentViewport(qreal documentX, qreal documentY, qreal zoom)
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    cancelActiveRasterStroke();
    ensureRasterLayerSize();
    clearPendingRasterStroke();
    m_documentOrigin = {static_cast<Types::Scalar>(documentX), static_cast<Types::Scalar>(documentY)};
    m_zoom = std::max<Types::Scalar>(0.01, static_cast<Types::Scalar>(zoom));
    updateViewportGeometry();
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    emit viewportChanged();
    update();
}

void PaintCanvasItem::resetView()
{
    setDocumentViewport(0.0, 0.0, 1.0);
}

void PaintCanvasItem::panBy(qreal documentDx, qreal documentDy)
{
    setDocumentViewport(m_documentOrigin.x + static_cast<Types::Scalar>(documentDx),
                        m_documentOrigin.y + static_cast<Types::Scalar>(documentDy),
                        m_zoom);
}

void PaintCanvasItem::zoomAt(qreal viewX, qreal viewY, qreal factor)
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    const Types::Scalar nextZoom = std::max<Types::Scalar>(0.01, m_zoom * static_cast<Types::Scalar>(factor));
    const DocumentPoint anchorBefore = documentPointFromViewPoint(m_viewport, ViewPoint{viewX, viewY});
    const Types::Scalar scale = std::max<Types::Scalar>(0.01, nextZoom);
    m_documentOrigin = {
            anchorBefore.x - static_cast<Types::Scalar>(viewX) / scale,
            anchorBefore.y - static_cast<Types::Scalar>(viewY) / scale,
    };
    m_zoom = nextZoom;
    cancelActiveRasterStroke();
    updateViewportGeometry();
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    emit viewportChanged();
    update();
}

void PaintCanvasItem::setBrush(qreal size, const QColor &color, qreal flow, qreal opacity)
{
    setBrushSize(size);
    setBrushColor(color);
    setBrushFlow(flow);
    setBrushOpacity(opacity);
}

bool PaintCanvasItem::resetRasterCanvas(Types::Pixel width,
                                        Types::Pixel height,
                                        std::uint32_t clearArgb)
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    ensureRasterLayerSize();
    recordRasterChange();

    RasterSnapshot snapshot;
    snapshot.width = width;
    snapshot.height = height;
    snapshot.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), clearArgb);
    snapshot.nextStrokeSeed = 1;
    snapshot.committedStrokeCount = 0;
    restoreRasterSnapshot(snapshot);
    return true;
}

bool PaintCanvasItem::replaceRasterCanvas(const QImage &image)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return false;
    }

    ensureRasterLayerSize();
    recordRasterChange();

    const QImage converted = image.convertToFormat(QImage::Format_ARGB32);
    RasterSnapshot snapshot;
    snapshot.width = converted.width();
    snapshot.height = converted.height();
    snapshot.pixels.reserve(static_cast<std::size_t>(snapshot.width)
                            * static_cast<std::size_t>(snapshot.height));
    for (Types::Pixel y = 0; y < snapshot.height; ++y) {
        for (Types::Pixel x = 0; x < snapshot.width; ++x) {
            snapshot.pixels.push_back(static_cast<std::uint32_t>(converted.pixel(x, y)));
        }
    }
    snapshot.nextStrokeSeed = 1;
    snapshot.committedStrokeCount = 0;
    restoreRasterSnapshot(snapshot);
    return true;
}

QImage PaintCanvasItem::rasterCanvasImage()
{
    ensureRasterLayerSize();
    return imageFromRasterLayer(m_rasterLayer);
}

bool PaintCanvasItem::undoRasterChange()
{
    if (m_undoRasterSnapshots.empty()) {
        return false;
    }

    RasterHistoryEntry previous = std::move(m_undoRasterSnapshots.back());
    m_undoRasterSnapshots.pop_back();
    m_redoRasterSnapshots.push_back(previous.fullCanvas
            ? captureRasterHistoryEntry()
            : captureRasterHistoryEntry(previous.patchSnapshot.bounds));
    restoreRasterHistoryEntry(previous);
    return true;
}

bool PaintCanvasItem::redoRasterChange()
{
    if (m_redoRasterSnapshots.empty()) {
        return false;
    }

    RasterHistoryEntry next = std::move(m_redoRasterSnapshots.back());
    m_redoRasterSnapshots.pop_back();
    m_undoRasterSnapshots.push_back(next.fullCanvas
            ? captureRasterHistoryEntry()
            : captureRasterHistoryEntry(next.patchSnapshot.bounds));
    restoreRasterHistoryEntry(next);
    return true;
}

bool PaintCanvasItem::canUndoRasterChange() const
{
    return !m_undoRasterSnapshots.empty();
}

bool PaintCanvasItem::canRedoRasterChange() const
{
    return !m_redoRasterSnapshots.empty();
}

bool PaintCanvasItem::event(QEvent *event)
{
    switch (event->type()) {
        case QEvent::TabletPress:
            handleTabletPointerEvent(static_cast<QTabletEvent *>(event), PointerEventPhase::Press);
            event->accept();
            return true;
        case QEvent::TabletMove:
            handleTabletPointerEvent(static_cast<QTabletEvent *>(event), PointerEventPhase::Move);
            event->accept();
            return true;
        case QEvent::TabletRelease:
            handleTabletPointerEvent(static_cast<QTabletEvent *>(event), PointerEventPhase::Release);
            event->accept();
            return true;
        default:
            break;
    }

    return QQuickPaintedItem::event(event);
}

void PaintCanvasItem::mousePressEvent(QMouseEvent *event)
{
    if (shouldIgnoreMousePointerEvent(event)) {
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    handleMousePointerEvent(event, PointerEventPhase::Press);
    event->accept();
}

void PaintCanvasItem::mouseMoveEvent(QMouseEvent *event)
{
    if (shouldIgnoreMousePointerEvent(event)) {
        event->accept();
        return;
    }

    handleMousePointerEvent(event, PointerEventPhase::Move);
    event->accept();
}

void PaintCanvasItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (shouldIgnoreMousePointerEvent(event)) {
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    handleMousePointerEvent(event, PointerEventPhase::Release);
    event->accept();
}

void PaintCanvasItem::ensureRasterLayerSize()
{
    const Types::Pixel nextWidth = itemPixelSize(width(), m_devicePixelRatio);
    const Types::Pixel nextHeight = itemPixelSize(height(), m_devicePixelRatio);
    bool resized = false;
    if (m_rasterLayer.width != nextWidth || m_rasterLayer.height != nextHeight) {
        m_rasterLayer = makeRasterLayer(nextWidth, nextHeight);
        resized = true;
    }

    if (m_liveRasterLayer.width != nextWidth || m_liveRasterLayer.height != nextHeight) {
        m_liveRasterLayer = makeRasterLayer(nextWidth, nextHeight);
        m_pendingRasterBuffer = makeStrokeCompositeBuffer(nextWidth, nextHeight);
        resized = true;
    }

    if (resized) {
        cancelActiveRasterStroke();
    }
    updateViewportGeometry();
}

void PaintCanvasItem::updateViewportGeometry()
{
    const Types::Scalar viewWidth = std::max<Types::Scalar>(0.0, static_cast<Types::Scalar>(width()));
    const Types::Scalar viewHeight = std::max<Types::Scalar>(0.0, static_cast<Types::Scalar>(height()));
    const Types::Scalar zoom = std::max<Types::Scalar>(0.01, m_zoom);
    m_viewport.documentRect = {
            m_documentOrigin,
            viewWidth / zoom,
            viewHeight / zoom,
    };
    m_viewport.viewRect = {{0.0, 0.0}, viewWidth, viewHeight};
    m_viewport.devicePixelRect = {{0, 0}, m_rasterLayer.width, m_rasterLayer.height};
    m_viewport.zoom = zoom;
    m_viewport.devicePixelRatio = std::max<Types::Scalar>(0.01, m_devicePixelRatio);
}

bool PaintCanvasItem::shouldIgnoreMousePointerEvent(QMouseEvent *event)
{
    if (m_tabletPointerActive || m_suppressMouseAfterTablet) {
        if (event->type() == QEvent::MouseButtonRelease && !event->buttons().testFlag(Qt::LeftButton)) {
            m_suppressMouseAfterTablet = false;
        }
        return true;
    }

    return false;
}

void PaintCanvasItem::handleMousePointerEvent(QMouseEvent *event, PointerEventPhase phase)
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    ensureRasterLayerSize();
    const PointerEvent pointerEvent = makeDocumentPointerEvent(event, phase);
    noteInputState(pointerEvent);
    const InputStrokeBuildResult result = appendPointerEvent(m_strokeBuilder, pointerEvent);
    applyPointerBuildResult(result);
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
}

void PaintCanvasItem::handleTabletPointerEvent(QTabletEvent *event, PointerEventPhase phase)
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    ensureRasterLayerSize();
    const PointerEvent pointerEvent = makeDocumentPointerEvent(event, phase);
    noteTabletPointerEvent(pointerEvent);
    noteInputState(pointerEvent);
    const InputStrokeBuildResult result = appendPointerEvent(m_strokeBuilder, pointerEvent);
    applyPointerBuildResult(result);
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
}

PointerEvent PaintCanvasItem::makeDocumentPointerEvent(QMouseEvent *event, PointerEventPhase phase) const
{
    const QPointF position = event->position();
    const bool pressureAware = isPressureAwareMouseEvent(event);
    const bool eraserActive = pressureAware && isTabletEraser(event->pointingDevice());
    const Types::Scalar pressure = pressureAware
            ? resolvePressureInput(PressureInput{
                    true,
                    0.0,
                    1.0,
                    eventPointPressure(event),
                    true,
                    false,
                    m_inputNormalizer.pressureCurveMinimum,
                    m_inputNormalizer.pressureCurveCenter,
                    m_inputNormalizer.pressureCurveMaximum,
            })
            : 1.0;
    const bool pressureContact = pressureAware && pressureInputHasContact(pressure);
    const bool primaryDown = event->buttons().testFlag(Qt::LeftButton)
            || (phase == PointerEventPhase::Press && event->button() == Qt::LeftButton)
            || pressureContact
            || eraserActive;
    const DocumentPoint documentPosition = documentPointFromViewPoint(
            m_viewport,
            ViewPoint{position.x(), position.y()});

    PointerEvent pointerEvent;
    pointerEvent.device = pressureAware ? PointerDeviceKind::Tablet : PointerDeviceKind::Mouse;
    pointerEvent.phase = phase;
    pointerEvent.documentPosition = documentPosition;
    pointerEvent.pressure = pressure;
    pointerEvent.time = static_cast<Types::Scalar>(event->timestamp());
    pointerEvent.button = eraserActive
            ? PointerButton::Eraser
            : (pressureContact ? PointerButton::Primary : pointerButtonFromMouseButton(event->button()));
    pointerEvent.primaryButtonDown = primaryDown;
    pointerEvent.tool = eraserActive
            ? PointerToolKind::Eraser
            : (pressureAware ? PointerToolKind::Pen : PointerToolKind::Mouse);
    pointerEvent.eraserActive = eraserActive;
    pointerEvent.rotationRadians = pressureAware ? eventPointRotation(event) : 0.0;
    if (primaryDown) {
        pointerEvent.deviceState |= PointerDeviceStatePrimaryButton;
    }
    if (eraserActive) {
        pointerEvent.deviceState |= PointerDeviceStateEraser;
    }
    return pointerEvent;
}

PointerEvent PaintCanvasItem::makeDocumentPointerEvent(QTabletEvent *event, PointerEventPhase phase) const
{
    return normalizeTabletPointerEvent(m_inputNormalizer, makeTabletState(event, phase), phase);
}

void PaintCanvasItem::noteTabletPointerEvent(const PointerEvent &event)
{
    const bool contact = event.primaryButtonDown || event.pressure > 0.0 || event.eraserActive;
    if ((event.phase == PointerEventPhase::Press || event.phase == PointerEventPhase::Move) && contact) {
        m_tabletPointerActive = true;
        m_suppressMouseAfterTablet = true;
        return;
    }

    if (event.phase == PointerEventPhase::Release || event.phase == PointerEventPhase::Cancel) {
        const bool suppressMouseFallback = m_tabletPointerActive || contact;
        m_tabletPointerActive = false;
        if (suppressMouseFallback) {
            m_suppressMouseAfterTablet = true;
        }
    }
}

TabletState PaintCanvasItem::makeTabletState(QTabletEvent *event, PointerEventPhase phase) const
{
    const QPointF position = event->position();
    const bool eraserActive = isTabletEraser(event->pointingDevice());
    const Types::Scalar pressure = std::clamp(static_cast<Types::Scalar>(event->pressure()), 0.0, 1.0);
    const bool pressureContact = pressure > 0.0;
    const bool primaryDown = event->buttons().testFlag(Qt::LeftButton)
            || (phase == PointerEventPhase::Press && event->button() == Qt::LeftButton)
            || pressureContact
            || eraserActive;
    const DocumentPoint documentPosition = documentPointFromViewPoint(
            m_viewport,
            ViewPoint{position.x(), position.y()});

    TabletState tablet;
    tablet.documentPosition = documentPosition;
    tablet.pressure = pressure;
    tablet.tiltX = std::clamp(static_cast<Types::Scalar>(event->xTilt()) / 60.0, -1.0, 1.0);
    tablet.tiltY = std::clamp(static_cast<Types::Scalar>(event->yTilt()) / 60.0, -1.0, 1.0);
    tablet.rotationRadians = static_cast<Types::Scalar>(event->rotation()) * 3.14159265358979323846 / 180.0;
    tablet.time = static_cast<Types::Scalar>(event->timestamp());
    tablet.inProximity = true;
    tablet.contact = primaryDown;
    tablet.primaryButtonDown = primaryDown;
    tablet.hovering = phase == PointerEventPhase::Move && !primaryDown && pressure <= 0.0;
    tablet.barrelButtonDown = event->buttons().testFlag(Qt::RightButton) || event->button() == Qt::RightButton;
    tablet.eraser = eraserActive;
    tablet.tool = eraserActive ? TabletToolKind::Eraser : TabletToolKind::Pen;
    return tablet;
}

RasterProjection PaintCanvasItem::currentRasterProjection() const
{
    return RasterProjection{
            m_viewport.documentRect.origin,
            m_viewport.devicePixelRect.origin,
            m_viewport.zoom * m_viewport.devicePixelRatio,
    };
}

DevicePixelRect PaintCanvasItem::layerBounds() const
{
    return DevicePixelRect{{0, 0}, m_rasterLayer.width, m_rasterLayer.height};
}

void PaintCanvasItem::requestTextureUpdate(DevicePixelRect dirtyBounds)
{
    const DevicePixelRect clipped = intersectDevicePixelRects(layerBounds(), dirtyBounds);
    if (isEmpty(clipped)) {
        return;
    }

    update(qRectFromDeviceRect(clipped, m_devicePixelRatio));
}

void PaintCanvasItem::applyPointerBuildResult(const InputStrokeBuildResult &result)
{
    if (result.strokeCancelled) {
        cancelActiveRasterStroke();
        return;
    }

    if (result.strokeStarted) {
        clearPendingRasterStroke();
        resetRasterDabStream(m_rasterDabStream);
        m_activeBrush = currentBrushState();
        m_liveStrokePreviewDestinationOut =
                m_activeBrush.rasterizer.blendMode == RasterBlendMode::DestinationOut;
    }

    if (result.pointAvailable) {
        appendPointerPointToRaster(result.point, result.strokeCompleted);
    }

    if (result.strokeCompleted) {
        commitPendingRasterStroke();
    }
}

void PaintCanvasItem::appendPointerPointToRaster(const StrokePoint &point, bool finishStroke)
{
    const std::vector<BrushDab> dabs = appendRasterDabs(m_rasterDabStream,
                                                        point,
                                                        m_activeBrush,
                                                        finishStroke);
    if (dabs.empty()) {
        return;
    }

    const RasterProjection projection = currentRasterProjection();
    std::vector<RasterSample> samples;
    const bool needsSource = m_activeBrush.material.simulation.enabled
            && m_activeBrush.material.simulation.model != BrushSimulationModel::Dry;
    if (needsSource) {
        const RasterSourceSampler sourceSampler = sourceSamplerForLayer(m_rasterLayer);
        samples = projectBrushDabs(dabs,
                                   m_activeBrush.rasterizer,
                                   projection,
                                   sourceSampler,
                                   m_activeBrush.material);
    } else {
        samples = projectBrushDabs(dabs,
                                   m_activeBrush.rasterizer,
                                   projection,
                                   m_activeBrush.material);
    }

    if (samples.empty()) {
        return;
    }

    const std::vector<RasterSample> accumulatedSamples = m_liveStrokePreviewDestinationOut
            ? destinationOutSamplesAsSourceMask(samples)
            : samples;
    accumulateStrokeSamples(m_pendingRasterBuffer, accumulatedSamples);

    const DevicePixelRect dirtyBounds = deviceBoundsForBrushDabsUnion(dabs,
                                                                      m_activeBrush.rasterizer,
                                                                      projection);
    syncPendingRasterLayer(dirtyBounds);
    m_liveStrokeDeviceDirtyBounds = uniteDevicePixelRects(m_liveStrokeDeviceDirtyBounds, dirtyBounds);
    if (m_livePreviewEnabled) {
        requestTextureUpdate(dirtyBounds);
    }
}

void PaintCanvasItem::commitPendingRasterStroke()
{
    const DevicePixelRect dirtyBounds = m_liveStrokeDeviceDirtyBounds;
    if (isEmpty(dirtyBounds)) {
        clearPendingRasterStroke();
        return;
    }

    recordRasterChange(dirtyBounds);
    if (m_liveStrokePreviewDestinationOut) {
        eraseStrokeBufferFromLayer(m_rasterLayer, m_pendingRasterBuffer);
    } else {
        compositeStrokeBufferOntoLayer(m_rasterLayer, m_pendingRasterBuffer);
    }

    clearPendingRasterStroke();
    requestTextureUpdate(dirtyBounds);
    ++m_nextStrokeSeed;
    ++m_committedStrokeCount;
    emit strokeCountChanged();
}

void PaintCanvasItem::clearPendingRasterStroke()
{
    const DevicePixelRect previousDirtyBounds = m_liveStrokeDeviceDirtyBounds;
    clearRasterLayerRect(m_liveRasterLayer, previousDirtyBounds);
    m_pendingRasterBuffer = makeStrokeCompositeBuffer(m_rasterLayer.width, m_rasterLayer.height);
    resetRasterDabStream(m_rasterDabStream);
    m_liveStrokeDeviceDirtyBounds = {};
    m_liveStrokePreviewDestinationOut = false;
    if (!isEmpty(previousDirtyBounds)) {
        requestTextureUpdate(previousDirtyBounds);
    }
}

void PaintCanvasItem::syncPendingRasterLayer(DevicePixelRect dirtyBounds)
{
    const DevicePixelRect clipped = intersectDevicePixelRects(layerBounds(), dirtyBounds);
    if (isEmpty(clipped)) {
        return;
    }

    for (Types::Pixel y = clipped.origin.y; y < clipped.origin.y + clipped.height; ++y) {
        const std::size_t rowStart = static_cast<std::size_t>(y)
                * static_cast<std::size_t>(m_liveRasterLayer.width);
        for (Types::Pixel x = clipped.origin.x; x < clipped.origin.x + clipped.width; ++x) {
            m_liveRasterLayer.pixels[rowStart + static_cast<std::size_t>(x)] =
                    strokeCompositePixelAt(m_pendingRasterBuffer, {x, y});
        }
    }
}

void PaintCanvasItem::emitLiveStrokeActiveChangedIfNeeded(bool previousActive)
{
    if (previousActive != liveStrokeActive()) {
        emit liveStrokeActiveChanged();
    }
}

void PaintCanvasItem::noteInputState(const PointerEvent &event)
{
    const Types::Scalar pressure = std::clamp(event.pressure, 0.0, 1.0);
    if (m_lastInputDevice == event.device
            && std::abs(m_lastInputPressure - pressure) < 0.001) {
        return;
    }

    m_lastInputDevice = event.device;
    m_lastInputPressure = pressure;
    emit inputStateChanged();
}

BrushState PaintCanvasItem::currentBrushState() const
{
    Rasterizer rasterizer = m_rasterizer;
    if (m_eraserMode) {
        rasterizer.argb = 0xFF000000U;
        rasterizer.blendMode = RasterBlendMode::DestinationOut;
    }
    BrushDynamics dynamics = pressureSensitiveDynamics();
    dynamics.pressureToOpacityEnabled = m_pressureToOpacityEnabled;
    return BrushState{rasterizer, dynamics, BrushMaterial{}, m_nextStrokeSeed};
}

void PaintCanvasItem::cancelActiveRasterStroke()
{
    resetInputStrokeBuilder(m_strokeBuilder);
    clearPendingRasterStroke();
}

PaintCanvasItem::RasterSnapshot PaintCanvasItem::captureRasterSnapshot() const
{
    RasterSnapshot snapshot;
    snapshot.width = m_rasterLayer.width;
    snapshot.height = m_rasterLayer.height;
    snapshot.pixels = m_rasterLayer.pixels;
    snapshot.nextStrokeSeed = m_nextStrokeSeed;
    snapshot.committedStrokeCount = m_committedStrokeCount;
    return snapshot;
}

PaintCanvasItem::RasterPatchSnapshot PaintCanvasItem::captureRasterPatchSnapshot(DevicePixelRect dirtyBounds) const
{
    RasterPatchSnapshot snapshot;
    snapshot.width = m_rasterLayer.width;
    snapshot.height = m_rasterLayer.height;
    snapshot.bounds = intersectDevicePixelRects(layerBounds(), dirtyBounds);
    snapshot.nextStrokeSeed = m_nextStrokeSeed;
    snapshot.committedStrokeCount = m_committedStrokeCount;

    if (isEmpty(snapshot.bounds)) {
        return snapshot;
    }

    snapshot.pixels.reserve(static_cast<std::size_t>(snapshot.bounds.width)
                            * static_cast<std::size_t>(snapshot.bounds.height));
    for (Types::Pixel y = snapshot.bounds.origin.y;
         y < snapshot.bounds.origin.y + snapshot.bounds.height;
         ++y) {
        const std::size_t rowStart = static_cast<std::size_t>(y)
                * static_cast<std::size_t>(m_rasterLayer.width);
        for (Types::Pixel x = snapshot.bounds.origin.x;
             x < snapshot.bounds.origin.x + snapshot.bounds.width;
             ++x) {
            snapshot.pixels.push_back(m_rasterLayer.pixels[rowStart + static_cast<std::size_t>(x)]);
        }
    }
    return snapshot;
}

PaintCanvasItem::RasterHistoryEntry PaintCanvasItem::captureRasterHistoryEntry() const
{
    RasterHistoryEntry entry;
    entry.fullCanvas = true;
    entry.fullSnapshot = captureRasterSnapshot();
    return entry;
}

PaintCanvasItem::RasterHistoryEntry PaintCanvasItem::captureRasterHistoryEntry(DevicePixelRect dirtyBounds) const
{
    RasterHistoryEntry entry;
    entry.fullCanvas = false;
    entry.patchSnapshot = captureRasterPatchSnapshot(dirtyBounds);
    return entry;
}

void PaintCanvasItem::recordRasterChange()
{
    m_undoRasterSnapshots.push_back(captureRasterHistoryEntry());
    m_redoRasterSnapshots.clear();
}

void PaintCanvasItem::recordRasterChange(DevicePixelRect dirtyBounds)
{
    RasterHistoryEntry entry = captureRasterHistoryEntry(dirtyBounds);
    if (isEmpty(entry.patchSnapshot.bounds)) {
        return;
    }
    m_undoRasterSnapshots.push_back(std::move(entry));
    m_redoRasterSnapshots.clear();
}

void PaintCanvasItem::restoreRasterSnapshot(const RasterSnapshot &snapshot)
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    const int previousStrokeCount = m_committedStrokeCount;

    cancelActiveRasterStroke();
    setWidth(snapshot.width);
    setHeight(snapshot.height);

    m_rasterLayer = makeRasterLayer(snapshot.width, snapshot.height);
    const auto expectedPixelCount = static_cast<std::size_t>(snapshot.width)
            * static_cast<std::size_t>(snapshot.height);
    if (snapshot.pixels.size() == expectedPixelCount) {
        m_rasterLayer.pixels = snapshot.pixels;
    }
    m_liveRasterLayer = makeRasterLayer(snapshot.width, snapshot.height);
    m_pendingRasterBuffer = makeStrokeCompositeBuffer(snapshot.width, snapshot.height);
    m_liveStrokeDeviceDirtyBounds = {};
    resetInputStrokeBuilder(m_strokeBuilder);
    resetRasterDabStream(m_rasterDabStream);
    m_nextStrokeSeed = snapshot.nextStrokeSeed;
    m_committedStrokeCount = snapshot.committedStrokeCount;

    updateViewportGeometry();
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    if (previousStrokeCount != m_committedStrokeCount) {
        emit strokeCountChanged();
    }
    emit viewportChanged();
    update();
}

void PaintCanvasItem::restoreRasterPatchSnapshot(const RasterPatchSnapshot &snapshot)
{
    if (snapshot.width != m_rasterLayer.width
            || snapshot.height != m_rasterLayer.height
            || isEmpty(snapshot.bounds)) {
        return;
    }

    const auto expectedPixelCount = static_cast<std::size_t>(snapshot.bounds.width)
            * static_cast<std::size_t>(snapshot.bounds.height);
    if (snapshot.pixels.size() != expectedPixelCount) {
        return;
    }

    const bool wasLiveStrokeActive = liveStrokeActive();
    const int previousStrokeCount = m_committedStrokeCount;

    cancelActiveRasterStroke();
    for (Types::Pixel y = 0; y < snapshot.bounds.height; ++y) {
        const std::size_t sourceRowStart = static_cast<std::size_t>(y)
                * static_cast<std::size_t>(snapshot.bounds.width);
        const std::size_t destinationRowStart = static_cast<std::size_t>(snapshot.bounds.origin.y + y)
                * static_cast<std::size_t>(m_rasterLayer.width)
                + static_cast<std::size_t>(snapshot.bounds.origin.x);
        std::copy(snapshot.pixels.begin() + static_cast<std::ptrdiff_t>(sourceRowStart),
                  snapshot.pixels.begin() + static_cast<std::ptrdiff_t>(sourceRowStart + snapshot.bounds.width),
                  m_rasterLayer.pixels.begin() + static_cast<std::ptrdiff_t>(destinationRowStart));
    }

    m_liveRasterLayer = makeRasterLayer(snapshot.width, snapshot.height);
    m_pendingRasterBuffer = makeStrokeCompositeBuffer(snapshot.width, snapshot.height);
    m_liveStrokeDeviceDirtyBounds = {};
    resetInputStrokeBuilder(m_strokeBuilder);
    resetRasterDabStream(m_rasterDabStream);
    m_nextStrokeSeed = snapshot.nextStrokeSeed;
    m_committedStrokeCount = snapshot.committedStrokeCount;
    m_liveStrokePreviewDestinationOut = false;

    updateViewportGeometry();
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    if (previousStrokeCount != m_committedStrokeCount) {
        emit strokeCountChanged();
    }
    requestTextureUpdate(snapshot.bounds);
}

void PaintCanvasItem::restoreRasterHistoryEntry(const RasterHistoryEntry &entry)
{
    if (entry.fullCanvas) {
        restoreRasterSnapshot(entry.fullSnapshot);
        return;
    }

    restoreRasterPatchSnapshot(entry.patchSnapshot);
}
