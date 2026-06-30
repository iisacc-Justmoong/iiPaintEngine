//
// Created by Justmoong on 2026 May 24.
//

#include "PaintCanvasItem.h"

#include <QImage>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPointingDevice>
#include <QPointer>
#include <QRect>
#include <QTabletEvent>
#include <QTimerEvent>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "Layer/RasterLayer.h"
#include "Input/PressureInput.h"
#include "Render/DirtyRegion.h"
#include "Stroke/LiveStroke.h"
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

RasterLayer copyRasterLayerRect(const RasterLayer &layer, DevicePixelRect bounds)
{
    const DevicePixelRect layerRect{{0, 0}, layer.width, layer.height};
    const DevicePixelRect clipped = intersectDevicePixelRects(layerRect, bounds);
    RasterLayer copy = makeRasterLayer(clipped.width, clipped.height);
    if (isEmpty(clipped)) {
        return copy;
    }

    for (Types::Pixel y = 0; y < clipped.height; ++y) {
        const std::size_t sourceRow = static_cast<std::size_t>(clipped.origin.y + y)
                * static_cast<std::size_t>(layer.width);
        const std::size_t targetRow = static_cast<std::size_t>(y)
                * static_cast<std::size_t>(copy.width);
        for (Types::Pixel x = 0; x < clipped.width; ++x) {
            copy.pixels[targetRow + static_cast<std::size_t>(x)] =
                    layer.pixels[sourceRow + static_cast<std::size_t>(clipped.origin.x + x)];
        }
    }
    return copy;
}

DevicePixelRect sourceLayerBoundsForStrokeInput(const StrokeInput &input,
                                                const Rasterizer &rasterizer,
                                                const RasterProjection &projection,
                                                DevicePixelRect layerBounds)
{
    if (input.points.empty()) {
        return {};
    }

    const Types::Scalar scale = std::max<Types::Scalar>(0.01, projection.scale);
    Types::Scalar left = 0.0;
    Types::Scalar top = 0.0;
    Types::Scalar right = 0.0;
    Types::Scalar bottom = 0.0;
    bool hasPoint = false;
    for (const StrokePoint &point : input.points) {
        const Types::Scalar x = static_cast<Types::Scalar>(projection.deviceOrigin.x)
                + (point.position.x - projection.documentOrigin.x) * scale;
        const Types::Scalar y = static_cast<Types::Scalar>(projection.deviceOrigin.y)
                + (point.position.y - projection.documentOrigin.y) * scale;
        if (!hasPoint) {
            left = right = x;
            top = bottom = y;
            hasPoint = true;
        } else {
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }

    Types::Scalar brushDiameter = std::max<Types::Scalar>(
            rasterizer.brushSize,
            static_cast<Types::Scalar>(std::max<Types::Pixel>(1, rasterizer.radius * 2)));
    brushDiameter = std::max<Types::Scalar>(brushDiameter,
                                            static_cast<Types::Scalar>(rasterizer.brushWidth));
    brushDiameter = std::max<Types::Scalar>(brushDiameter,
                                            static_cast<Types::Scalar>(rasterizer.brushHeight));
    const Types::Scalar padding = std::max<Types::Scalar>(8.0, brushDiameter * scale * 3.0);
    const Types::Pixel cropLeft = static_cast<Types::Pixel>(std::floor(left - padding));
    const Types::Pixel cropTop = static_cast<Types::Pixel>(std::floor(top - padding));
    const Types::Pixel cropRight = static_cast<Types::Pixel>(std::ceil(right + padding));
    const Types::Pixel cropBottom = static_cast<Types::Pixel>(std::ceil(bottom + padding));
    const DevicePixelRect sourceBounds{
            {cropLeft, cropTop},
            std::max<Types::Pixel>(0, cropRight - cropLeft + 1),
            std::max<Types::Pixel>(0, cropBottom - cropTop + 1),
    };
    return intersectDevicePixelRects(layerBounds, sourceBounds);
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
    m_liveEventThreadPool.setMaxThreadCount(1);
    m_liveEventThreadPool.setExpiryTimeout(-1);
    m_commitEventThreadPool.setMaxThreadCount(1);
    m_commitEventThreadPool.setExpiryTimeout(-1);
    m_rasterizer.spacing = 0.0;
    m_rasterizer.spacingRatio = 0.0;
    m_rasterizer.flow = 1.0;
}

PaintCanvasItem::~PaintCanvasItem()
{
    cancelLiveStrokePreviewFrame();
    cancelStrokeCommitFrame();
    m_liveEventThreadPool.waitForDone();
    m_commitEventThreadPool.waitForDone();
}

qreal PaintCanvasItem::documentX() const
{
    return m_documentOrigin.x;
}

void PaintCanvasItem::setDocumentX(qreal value)
{
    if (m_documentOrigin.x == value) {
        return;
    }

    m_documentOrigin.x = static_cast<Types::Scalar>(value);
    invalidatePendingCanvasEventWork();
    updateViewportGeometry();
    emit viewportChanged();
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

    m_documentOrigin.y = static_cast<Types::Scalar>(value);
    invalidatePendingCanvasEventWork();
    updateViewportGeometry();
    emit viewportChanged();
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

    m_zoom = nextZoom;
    invalidatePendingCanvasEventWork();
    updateViewportGeometry();
    emit viewportChanged();
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

    m_devicePixelRatio = nextRatio;
    invalidatePendingCanvasEventWork();
    updateViewportGeometry();
    emit viewportChanged();
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

qreal PaintCanvasItem::stabilizerStrength() const
{
    return m_stabilizer.smoothing;
}

void PaintCanvasItem::setStabilizerStrength(qreal value)
{
    const Types::Scalar nextStrength = unitSettingOrDefault(static_cast<Types::Scalar>(value), 0.0);
    if (m_stabilizer.smoothing == nextStrength) {
        return;
    }

    m_stabilizer.smoothing = nextStrength;
    invalidatePendingCanvasEventWork();
    if (m_strokeBuilder.active && m_livePreviewEnabled) {
        requestLiveStrokePreviewFrame();
    }
    emit strokeSettingsChanged();
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

    const bool wasLiveStrokeActive = liveStrokeActive();
    m_livePreviewEnabled = enabled;
    if (!m_livePreviewEnabled) {
        clearLiveStrokePreview();
    } else if (m_strokeBuilder.active) {
        requestLiveStrokePreviewFrame();
    }
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    emit livePreviewEnabledChanged();
}

int PaintCanvasItem::livePreviewFrameIntervalMs() const
{
    return m_livePreviewFrameIntervalMs;
}

void PaintCanvasItem::setLivePreviewFrameIntervalMs(int value)
{
    const int nextInterval = std::clamp(value, 0, 1000);
    if (m_livePreviewFrameIntervalMs == nextInterval) {
        return;
    }

    m_livePreviewFrameIntervalMs = nextInterval;
    if (m_livePreviewFrameTimer.isActive()) {
        m_livePreviewFrameTimer.start(m_livePreviewFrameIntervalMs, Qt::PreciseTimer, this);
    }
    emit livePreviewFrameIntervalMsChanged();
}

bool PaintCanvasItem::multithreadedEventsEnabled() const
{
    return m_multithreadedEventsEnabled;
}

void PaintCanvasItem::setMultithreadedEventsEnabled(bool enabled)
{
    if (m_multithreadedEventsEnabled == enabled) {
        return;
    }

    m_multithreadedEventsEnabled = enabled;
    if (!m_multithreadedEventsEnabled) {
        cancelLiveStrokePreviewFrame();
        ++m_livePreviewGeneration;
        ++m_livePreviewRevision;
        m_livePreviewWorkPending = false;
        m_liveEventThreadPool.waitForDone();
        m_commitEventThreadPool.waitForDone();
    }
    emit multithreadedEventsEnabledChanged();
}

bool PaintCanvasItem::liveStrokeActive() const
{
    return m_liveStrokeBuffer.active;
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
    if (m_liveStrokeBuffer.active) {
        drawLiveRasterLayer(painter, m_liveRasterLayer, m_devicePixelRatio, m_liveStrokePreviewDestinationOut);
    }
}

void PaintCanvasItem::clear()
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    ensureRasterLayerSize();
    recordRasterChange();
    invalidatePendingCanvasEventWork();
    clearRasterLayer(m_rasterLayer);
    clearLiveStrokePreview();
    resetInputStrokeBuilder(m_strokeBuilder);
    m_pendingCommitStrokeWorkRequests.clear();
    m_nextStrokeSeed = 1;
    m_committedStrokeCount = 0;
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    emit strokeCountChanged();
    update();
}

void PaintCanvasItem::setDocumentViewport(qreal documentX, qreal documentY, qreal zoom)
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    invalidatePendingCanvasEventWork();
    ensureRasterLayerSize();
    clearLiveStrokePreview();
    resetInputStrokeBuilder(m_strokeBuilder);
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
    const Types::Scalar nextZoom = std::max<Types::Scalar>(0.01, m_zoom * static_cast<Types::Scalar>(factor));
    const DocumentPoint anchorBefore = documentPointFromViewPoint(m_viewport, ViewPoint{viewX, viewY});
    const Types::Scalar scale = std::max<Types::Scalar>(0.01, nextZoom);
    m_documentOrigin = {
            anchorBefore.x - static_cast<Types::Scalar>(viewX) / scale,
            anchorBefore.y - static_cast<Types::Scalar>(viewY) / scale,
    };
    m_zoom = nextZoom;
    invalidatePendingCanvasEventWork();
    updateViewportGeometry();
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

bool PaintCanvasItem::saveRasterCanvasToFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }

    ensureRasterLayerSize();
    const QImage image = imageFromRasterLayer(m_rasterLayer);
    if (image.isNull()) {
        return false;
    }
    return image.save(filePath);
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

void PaintCanvasItem::timerEvent(QTimerEvent *event)
{
    if (m_livePreviewFrameTimer.isActive()
            && event->timerId() == m_livePreviewFrameTimer.timerId()) {
        m_livePreviewFrameTimer.stop();
        processLiveStrokePreviewFrame();
        event->accept();
        return;
    }

    if (m_commitStrokeFrameTimer.isActive()
            && event->timerId() == m_commitStrokeFrameTimer.timerId()) {
        m_commitStrokeFrameTimer.stop();
        processStrokeCommitFrame();
        event->accept();
        return;
    }

    QQuickPaintedItem::timerEvent(event);
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
        resized = true;
    }

    if (resized) {
        invalidatePendingCanvasEventWork();
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
    if (result.strokeCompleted) {
        preserveLiveStrokePreviewForCommit();
        enqueueStrokeCommit(result.stroke);
    } else if (m_strokeBuilder.active && m_livePreviewEnabled) {
        requestLiveStrokePreviewFrame();
    }
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
    if (result.strokeCompleted) {
        preserveLiveStrokePreviewForCommit();
        enqueueStrokeCommit(result.stroke);
    } else if (m_strokeBuilder.active && m_livePreviewEnabled) {
        requestLiveStrokePreviewFrame();
    }
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

void PaintCanvasItem::requestLiveStrokePreviewFrame()
{
    if (!m_livePreviewEnabled || !m_strokeBuilder.active || m_livePreviewFrameTimer.isActive()) {
        return;
    }

    m_livePreviewFrameTimer.start(m_livePreviewFrameIntervalMs, Qt::PreciseTimer, this);
}

void PaintCanvasItem::processLiveStrokePreviewFrame()
{
    if (!m_livePreviewEnabled || !m_strokeBuilder.active) {
        return;
    }

    updateLiveStrokePreview();
}

void PaintCanvasItem::cancelLiveStrokePreviewFrame()
{
    if (m_livePreviewFrameTimer.isActive()) {
        m_livePreviewFrameTimer.stop();
    }
}

void PaintCanvasItem::enqueueStrokeCommit(const StrokeInput &stroke)
{
    m_pendingCommitStrokeWorkRequests.push_back(currentCommitStrokeWorkRequest(stroke));
    ++m_nextStrokeSeed;
    requestStrokeCommitFrame();
}

void PaintCanvasItem::requestStrokeCommitFrame()
{
    if (m_pendingCommitStrokeWorkRequests.empty() || m_commitStrokeFrameTimer.isActive()) {
        return;
    }

    m_commitStrokeFrameTimer.start(0, Qt::PreciseTimer, this);
}

void PaintCanvasItem::processStrokeCommitFrame()
{
    if (m_pendingCommitStrokeWorkRequests.empty()) {
        return;
    }

    const CanvasCommitStrokeWorkRequest request = m_pendingCommitStrokeWorkRequests.front();
    m_pendingCommitStrokeWorkRequests.pop_front();
    startCommitStrokeWork(request);
    requestStrokeCommitFrame();
}

void PaintCanvasItem::cancelStrokeCommitFrame()
{
    if (m_commitStrokeFrameTimer.isActive()) {
        m_commitStrokeFrameTimer.stop();
    }
}

void PaintCanvasItem::updateLiveStrokePreview()
{
    const CanvasLiveStrokeWorkRequest request = currentLiveStrokeWorkRequest();
    const std::uint64_t generation = ++m_livePreviewGeneration;
    const std::uint64_t revision = m_livePreviewRevision;
    if (!m_multithreadedEventsEnabled) {
        applyLiveStrokeWorkResult(generation, revision, runCanvasLiveStrokeWork(request));
        return;
    }

    if (m_livePreviewWorkActive) {
        m_pendingLiveStrokeWorkRequest = request;
        m_pendingLivePreviewGeneration = generation;
        m_pendingLivePreviewRevision = revision;
        m_livePreviewWorkPending = true;
        return;
    }

    startLiveStrokePreviewWork(request, generation, revision);
}

void PaintCanvasItem::startLiveStrokePreviewWork(const CanvasLiveStrokeWorkRequest &request,
                                                 std::uint64_t generation,
                                                 std::uint64_t revision)
{
    m_livePreviewWorkActive = true;
    const QPointer<PaintCanvasItem> self(this);
    m_liveEventThreadPool.start([self, request, generation, revision]() {
        const auto result = std::make_shared<CanvasLiveStrokeWorkResult>(runCanvasLiveStrokeWork(request));
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(),
                                  [self, generation, revision, result]() {
                                      if (!self) {
                                          return;
                                      }
                                      self->applyLiveStrokeWorkResult(generation, revision, *result);
                                  },
                                  Qt::QueuedConnection);
    });
}

void PaintCanvasItem::startPendingLiveStrokePreviewWork()
{
    if (m_livePreviewWorkActive || !m_livePreviewWorkPending) {
        return;
    }

    if (!m_multithreadedEventsEnabled) {
        m_livePreviewWorkPending = false;
        return;
    }

    const CanvasLiveStrokeWorkRequest request = m_pendingLiveStrokeWorkRequest;
    const std::uint64_t generation = m_pendingLivePreviewGeneration;
    const std::uint64_t revision = m_pendingLivePreviewRevision;
    m_livePreviewWorkPending = false;

    if (revision != m_livePreviewRevision || !m_livePreviewEnabled || !m_strokeBuilder.active) {
        return;
    }

    startLiveStrokePreviewWork(request, generation, revision);
}

void PaintCanvasItem::applyLiveStrokeWorkResult(std::uint64_t generation,
                                                std::uint64_t revision,
                                                const CanvasLiveStrokeWorkResult &result)
{
    m_livePreviewWorkActive = false;
    if (revision != m_livePreviewRevision
            || generation > m_livePreviewGeneration
            || !m_livePreviewEnabled
            || !m_strokeBuilder.active) {
        startPendingLiveStrokePreviewWork();
        return;
    }

    const bool wasLiveStrokeActive = liveStrokeActive();
    const DevicePixelRect previousDirtyBounds = m_liveStrokeDeviceDirtyBounds;
    const DevicePixelRect replacedDirtyBounds = result.incrementalPreview
            ? liveStrokeTailDeviceDirtyBounds(result.incrementalPreviewStartDistance)
            : previousDirtyBounds;
    clearRasterLayerRect(m_liveRasterLayer, replacedDirtyBounds);

    m_liveStrokeBuffer.frame = result.frame;
    m_liveStrokeBuffer.active = result.frame.active;
    m_liveStrokeDeviceDirtyBounds = result.frame.active ? result.fullDirtyBounds : DevicePixelRect{};
    m_liveStrokeRenderedDistance = result.frame.active ? result.renderedStrokeDistance : 0.0;
    m_liveStrokePreviewRasterizer = result.previewRasterizer;
    m_liveStrokePreviewDestinationOut = samplesContainDestinationOut(result.samples);
    if (m_liveStrokeBuffer.active) {
        if (m_liveStrokePreviewDestinationOut) {
            paintRasterSamples(m_liveRasterLayer, destinationOutSamplesAsSourceMask(result.samples));
        } else {
            paintRasterSamples(m_liveRasterLayer, result.samples);
        }
    }

    requestTextureUpdate(uniteDevicePixelRects(replacedDirtyBounds, result.dirtyBounds));
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    startPendingLiveStrokePreviewWork();
}

void PaintCanvasItem::preserveLiveStrokePreviewForCommit()
{
    cancelLiveStrokePreviewFrame();
    ++m_livePreviewGeneration;
    ++m_livePreviewRevision;
    m_livePreviewWorkPending = false;
}

void PaintCanvasItem::clearLiveStrokePreview()
{
    cancelLiveStrokePreviewFrame();
    ++m_livePreviewGeneration;
    ++m_livePreviewRevision;
    m_livePreviewWorkPending = false;
    clearLiveStrokePreviewPixels();
}

void PaintCanvasItem::clearLiveStrokePreviewPixels()
{
    const DevicePixelRect previousDirtyBounds = m_liveStrokeDeviceDirtyBounds;
    clearLiveStrokeBuffer(m_liveStrokeBuffer);
    clearRasterLayerRect(m_liveRasterLayer, previousDirtyBounds);
    m_liveStrokeDeviceDirtyBounds = {};
    m_liveStrokeRenderedDistance = 0.0;
    m_liveStrokePreviewRasterizer = {};
    m_liveStrokePreviewDestinationOut = false;
    requestTextureUpdate(previousDirtyBounds);
}

Types::Scalar PaintCanvasItem::liveStrokeIncrementalStartDistance(const BrushState &brush) const
{
    if (!m_liveStrokeBuffer.active
            || m_liveStrokeBuffer.frame.dabs.empty()
            || m_liveStrokeRenderedDistance <= 0.0) {
        return 0.0;
    }

    const Types::Scalar brushDiameter = brush.rasterizer.brushSize > 0.0
            ? brush.rasterizer.brushSize
            : std::max<Types::Scalar>(1.0, static_cast<Types::Scalar>(brush.rasterizer.radius) * 2.0);
    const Types::Scalar overlapDistance = std::max<Types::Scalar>(4.0, brushDiameter * 2.0);
    return std::max<Types::Scalar>(0.0, m_liveStrokeRenderedDistance - overlapDistance);
}

DevicePixelRect PaintCanvasItem::liveStrokeTailDeviceDirtyBounds(Types::Scalar startDistance) const
{
    if (!m_liveStrokeBuffer.active || m_liveStrokeBuffer.frame.dabs.empty()) {
        return {};
    }

    const auto &dabs = m_liveStrokeBuffer.frame.dabs;
    const auto firstDab = std::lower_bound(dabs.begin(),
                                           dabs.end(),
                                           startDistance,
                                           [](const BrushDab &dab, Types::Scalar distance) {
                                               return dab.strokeDistance + 0.000001 < distance;
                                           });
    if (firstDab == dabs.end()) {
        return {};
    }

    const std::span<const BrushDab> tailDabs{&*firstDab, static_cast<std::size_t>(dabs.end() - firstDab)};
    return makeDirtyRegion(deviceBoundsForBrushDabs(tailDabs,
                                                   m_liveStrokePreviewRasterizer,
                                                   currentRasterProjection())).bounds;
}

void PaintCanvasItem::startCommitStrokeWork(const CanvasCommitStrokeWorkRequest &request)
{
    const std::uint64_t revision = m_canvasEventRevision;
    if (!m_multithreadedEventsEnabled) {
        applyCommitStrokeWorkResult(revision, runCanvasCommitStrokeWork(request));
        return;
    }

    const QPointer<PaintCanvasItem> self(this);
    m_commitEventThreadPool.start([self, request, revision]() {
        const auto result = std::make_shared<CanvasCommitStrokeWorkResult>(runCanvasCommitStrokeWork(request));
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(),
                                  [self, revision, result]() {
                                      if (!self) {
                                          return;
                                      }
                                      self->applyCommitStrokeWorkResult(revision, *result);
                                  },
                                  Qt::QueuedConnection);
    });
}

void PaintCanvasItem::applyCommitStrokeWorkResult(std::uint64_t revision,
                                                  const CanvasCommitStrokeWorkResult &result)
{
    if (revision != m_canvasEventRevision) {
        return;
    }

    recordRasterChange(result.dirtyBounds);
    paintRasterSamples(m_rasterLayer, result.samples);
    const bool wasLiveStrokeActive = liveStrokeActive();
    const DevicePixelRect previousLiveDirtyBounds = m_liveStrokeDeviceDirtyBounds;
    clearLiveStrokePreviewPixels();
    requestTextureUpdate(uniteDevicePixelRects(result.dirtyBounds, previousLiveDirtyBounds));
    ++m_committedStrokeCount;
    emit strokeCountChanged();
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
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
    return BrushState{rasterizer, pressureSensitiveDynamics(), StrokeResampler{}, BrushMaterial{}, m_nextStrokeSeed};
}

CanvasLiveStrokeWorkRequest PaintCanvasItem::currentLiveStrokeWorkRequest() const
{
    CanvasLiveStrokeWorkRequest request;
    request.rawInput = activeStrokeInput(m_strokeBuilder);
    request.brush = currentBrushState();
    request.stabilizer = m_stabilizer;
    request.projection = currentRasterProjection();
    request.sourceLayerEnabled = brushNeedsSourceLayer(request.brush);
    if (request.sourceLayerEnabled) {
        const DevicePixelRect sourceBounds = sourceLayerBoundsForStrokeInput(request.rawInput,
                                                                            request.brush.rasterizer,
                                                                            request.projection,
                                                                            layerBounds());
        request.sourceLayerOrigin = sourceBounds.origin;
        request.sourceLayer = copyRasterLayerRect(m_rasterLayer, sourceBounds);
    }
    if (!request.sourceLayerEnabled && request.brush.rasterizer.blendMode == RasterBlendMode::SourceOver) {
        request.incrementalPreviewStartDistance = liveStrokeIncrementalStartDistance(request.brush);
        request.incrementalPreviewEnabled = request.incrementalPreviewStartDistance > 0.0;
    }
    return request;
}

CanvasCommitStrokeWorkRequest PaintCanvasItem::currentCommitStrokeWorkRequest(const StrokeInput &stroke) const
{
    CanvasCommitStrokeWorkRequest request;
    request.rawInput = stroke;
    request.brush = currentBrushState();
    request.stabilizer = m_stabilizer;
    request.projection = currentRasterProjection();
    request.sourceLayerEnabled = brushNeedsSourceLayer(request.brush);
    if (request.sourceLayerEnabled) {
        const DevicePixelRect sourceBounds = sourceLayerBoundsForStrokeInput(request.rawInput,
                                                                            request.brush.rasterizer,
                                                                            request.projection,
                                                                            layerBounds());
        request.sourceLayerOrigin = sourceBounds.origin;
        request.sourceLayer = copyRasterLayerRect(m_rasterLayer, sourceBounds);
    }
    return request;
}

void PaintCanvasItem::invalidatePendingCanvasEventWork()
{
    cancelLiveStrokePreviewFrame();
    cancelStrokeCommitFrame();
    m_pendingCommitStrokeWorkRequests.clear();
    ++m_canvasEventRevision;
    ++m_livePreviewGeneration;
    ++m_livePreviewRevision;
    m_livePreviewWorkPending = false;
    m_liveStrokeRenderedDistance = 0.0;
    m_commitEventThreadPool.clear();
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

    invalidatePendingCanvasEventWork();
    setWidth(snapshot.width);
    setHeight(snapshot.height);

    m_rasterLayer = makeRasterLayer(snapshot.width, snapshot.height);
    const auto expectedPixelCount = static_cast<std::size_t>(snapshot.width)
            * static_cast<std::size_t>(snapshot.height);
    if (snapshot.pixels.size() == expectedPixelCount) {
        m_rasterLayer.pixels = snapshot.pixels;
    }
    m_liveRasterLayer = makeRasterLayer(snapshot.width, snapshot.height);
    clearLiveStrokeBuffer(m_liveStrokeBuffer);
    m_liveStrokeDeviceDirtyBounds = {};
    resetInputStrokeBuilder(m_strokeBuilder);
    m_pendingCommitStrokeWorkRequests.clear();
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

    invalidatePendingCanvasEventWork();
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
    clearLiveStrokeBuffer(m_liveStrokeBuffer);
    m_liveStrokeDeviceDirtyBounds = {};
    resetInputStrokeBuilder(m_strokeBuilder);
    m_pendingCommitStrokeWorkRequests.clear();
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
