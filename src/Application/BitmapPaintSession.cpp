#include "BitmapPaintSession.h"
#include "Brush/BrushPresetSerializer.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>

void BitmapPaintSession::initializePixels()
{
    m_pending = makeStrokeCompositeBuffer(m_file.width(), m_file.height());
    m_preview = m_file.pixels();
}

void BitmapPaintSession::synchronizePreview(DevicePixelRect bounds) noexcept
{
    const auto &committed = m_file.pixels().pixels;
    for (int y = 0; y < bounds.height; ++y) {
        const auto start = static_cast<std::size_t>(bounds.origin.y + y) * m_preview.width + bounds.origin.x;
        std::copy_n(committed.data() + start, bounds.width, m_preview.pixels.data() + start);
    }
}

void BitmapPaintSession::resetStrokePixels(bool restorePreview) noexcept
{
    if (!isEmpty(m_dirty)) {
        if (restorePreview) synchronizePreview(m_dirty);
        for (int y = 0; y < m_dirty.height; ++y) {
            const auto start = static_cast<std::size_t>(m_dirty.origin.y + y) * m_pending.width + m_dirty.origin.x;
            std::fill_n(m_pending.pixels.data() + start, m_dirty.width, PremultipliedPixel{});
        }
    }
    m_dirty = {};
    m_stream = {};
    m_input = {};
}

bool BitmapPaintSession::createFile(const std::filesystem::path &path,int width,int height,
                                   std::string_view format,std::uint32_t clearArgb)
{
    try {
        if (width <= 0 || height <= 0 || width > maxBitmapDimension || height > maxBitmapDimension
                || static_cast<std::uint64_t>(width) * height > maxBitmapPixelCount) {
            m_error = "Invalid bitmap dimensions."; return false;
        }
        BitmapPaintSession candidate(m_file.fileIO(), m_limits, m_historyLimits);
        candidate.m_pending = makeStrokeCompositeBuffer(width,height);
        candidate.m_preview = makeRasterLayer(width,height,clearArgb);
        if (!candidate.m_file.create(path,width,height,format,clearArgb)) { m_error = candidate.m_file.lastError(); return false; }
        candidate.m_preset = m_preset; candidate.m_color = m_color; candidate.m_eraser = m_eraser;
        *this = std::move(candidate); return true;
    } catch (const std::exception &e) { m_error = e.what(); return false; }
}
bool BitmapPaintSession::openFile(const std::filesystem::path &path)
{
    try {
        BitmapPaintSession candidate(m_file.fileIO(), m_limits, m_historyLimits);
        if (!candidate.m_file.open(path)) { m_error = candidate.m_file.lastError(); return false; }
        candidate.initializePixels(); candidate.m_preset = m_preset; candidate.m_color = m_color; candidate.m_eraser = m_eraser;
        *this = std::move(candidate); return true;
    } catch (const std::exception &e) { m_error = e.what(); return false; }
}
bool BitmapPaintSession::save(const BitmapFileWriteOptions &options)
{
    if (!m_file.save(options)) { m_error = m_file.lastError(); return false; }
    m_error.clear(); return true;
}
bool BitmapPaintSession::saveAs(const std::filesystem::path &path,std::string_view format,const BitmapFileWriteOptions &options)
{
    if (!m_file.saveAs(path,format,options)) { m_error = m_file.lastError(); return false; }
    m_error.clear(); return true;
}
void BitmapPaintSession::closeFile()
{
    m_file.close(); m_pending = {}; m_preview = {}; m_input = {}; m_stream = {};
    m_dirty = {};
    m_history.reset();
    m_historyCursor = 0;
    m_historyBytes = 0;
    m_error.clear();
}
const BitmapFile &BitmapPaintSession::file() const { return m_file; }
const RasterLayer &BitmapPaintSession::previewPixels() const { return m_preview; }
const BrushPreset &BitmapPaintSession::brushPreset() const { return m_preset; }
bool BitmapPaintSession::setBrushPreset(const BrushPreset &preset,bool preserveProtectedTexture)
{
    const auto incoming = resolveBrushPreset(preset,m_color,m_nextSeed);
    if (!incoming.brush) { m_error = incoming.errors.front(); return false; }
    auto selected = selectBrushPreset(m_preset,preset,preserveProtectedTexture);
    const auto resolved = resolveBrushPreset(selected,m_color,m_nextSeed);
    if (!resolved.brush) { m_error = resolved.errors.front(); return false; }
    m_preset = std::move(selected); ++m_presetRevision; m_error.clear(); return true;
}
bool BitmapPaintSession::setBrushPresetData(std::string_view payload,bool preserveProtectedTexture)
{
    auto decoded = readBrushPreset(std::string(payload));
    if (!decoded.preset) { m_error = decoded.errors.front(); return false; }
    return setBrushPreset(*decoded.preset,preserveProtectedTexture);
}
std::string BitmapPaintSession::brushPresetData() const { return serializeBrushPreset(m_preset); }
void BitmapPaintSession::setBrushColor(std::uint32_t argb) { m_color = argb; }
void BitmapPaintSession::setEraserMode(bool enabled) { m_eraser = enabled; }
bool BitmapPaintSession::strokeActive() const { return m_stream.active; }
const std::string &BitmapPaintSession::lastError() const { return m_error; }
void BitmapPaintSession::cancelStroke() { resetStrokePixels(true); m_error.clear(); }

std::size_t BitmapPaintSession::historyEntryBudget() const noexcept
{
    return m_historyLimits.maximumEntries == 0 ? 0 : m_historyLimits.maximumBytes;
}

EngineErrorCode BitmapPaintSession::applyEdit(std::expected<BitmapPixelDelta, EngineErrorCode> delta)
{
    if (!delta) {
        m_error = delta.error() == EngineErrorCode::BudgetExceeded
                ? "Bitmap edit exceeds the history budget." : "Invalid bitmap edit bounds.";
        return delta.error();
    }
    if (delta->empty()) return EngineErrorCode::None;

    const auto bytes = delta->retainedBytes();
    if (!m_history) m_history.emplace();
    auto &history = *m_history;
    history.push_back(std::move(*delta));
    if (!m_file.applyPixelDelta(history.back())) {
        history.pop_back();
        m_error = "Bitmap pixel delta does not match the open file.";
        return EngineErrorCode::InvalidState;
    }

    // After the pixel write succeeds, retire the redo branch and evict from the oldest end.
    const auto firstRedo = history.begin() + static_cast<std::ptrdiff_t>(m_historyCursor);
    const auto newEntry = std::prev(history.end());
    for (auto entry = firstRedo; entry != newEntry; ++entry) m_historyBytes -= entry->retainedBytes();
    history.erase(firstRedo, newEntry);
    while (m_historyCursor >= m_historyLimits.maximumEntries || bytes > m_historyLimits.maximumBytes - m_historyBytes) {
        m_historyBytes -= history.front().retainedBytes();
        history.pop_front();
        --m_historyCursor;
    }
    ++m_historyCursor;
    m_historyBytes += bytes;
    return EngineErrorCode::None;
}

void BitmapPaintSession::finishStroke(BitmapPaintUpdate &result)
{
    result.strokeActive = false;
    if (isEmpty(m_dirty)) {
        resetStrokePixels(false);
        return;
    }
    try {
        result.code = applyEdit(BitmapPixelDelta::between(m_file.pixels(), m_preview, m_dirty, historyEntryBudget()));
    } catch (const std::exception &error) {
        result.code = EngineErrorCode::InvalidState;
        m_error = error.what();
    }
    if (result.code != EngineErrorCode::None) {
        result.accepted = false;
        result.dirtyBounds = uniteDevicePixelRects(result.dirtyBounds, m_dirty);
        resetStrokePixels(true);
        result.error = m_error;
        return;
    }
    if (m_activeBrush.material.erodible.enabled && m_activePresetRevision == m_presetRevision)
        m_preset.material.erodible.wear = m_stream.tipWear;
    ++m_nextSeed;
    resetStrokePixels(false);
    result.committed = true;
}
BitmapPaintUpdate BitmapPaintSession::appendPoint(const StrokePoint &point, bool finish, BrushState *startingBrush)
{
    auto stream = startingBrush ? RasterDabStream{} : m_stream;
    const auto &brush = startingBrush ? *startingBrush : m_activeBrush;
    const auto dabs = appendRasterDabsWithLimit(stream, point, brush, m_limits.maxDabsPerEvent, finish);
    RasterSourceSampler sampler{&m_file.pixels(),[](const void *context,DevicePixelPoint p) {
        return rasterLayerPixelAt(*static_cast<const RasterLayer *>(context),p);
    },{},m_file.width(),m_file.height()};
    const DevicePixelRect canvas{{}, m_file.width(), m_file.height()};
    auto samples = projectClippedBrushDabs(BrushDabSpan{dabs}, brush.rasterizer, canvas,
                                          m_limits.maxProjectedPixels, sampler, brush.material);
    const bool erase = brush.rasterizer.blendMode == RasterBlendMode::DestinationOut;
    DevicePixelRect replacedPreview{};
    if (startingBrush) {
        replacedPreview = m_dirty;
        resetStrokePixels(true);
        m_activeBrush = std::move(*startingBrush);
        m_activePresetRevision = m_presetRevision;
    }
    DevicePixelRect bounds{};
    for (auto &sample : samples) {
        if (erase) sample.blendMode = RasterBlendMode::SourceOver;
        bounds = uniteDevicePixelRects(bounds, {sample.position, 1, 1});
    }
    accumulateStrokeSamples(m_pending,samples);
    copyStrokePreviewSamplePixels(m_preview,m_file.pixels(),m_pending,samples,erase);
    m_dirty = uniteDevicePixelRects(m_dirty,bounds);
    m_stream = stream;
    m_error.clear();
    BitmapPaintUpdate result{true,false,m_stream.active,uniteDevicePixelRects(replacedPreview,bounds),{}};
    if (finish) finishStroke(result);
    return result;
}
BitmapPaintUpdate BitmapPaintSession::processPointerEvent(const PointerEvent &event)
{
    try {
        if (!m_file.isOpen()) throw std::invalid_argument("No bitmap file is open.");
        if (!std::isfinite(event.documentPosition.x) || !std::isfinite(event.documentPosition.y)
                || std::abs(event.documentPosition.x) > 1e9 || std::abs(event.documentPosition.y) > 1e9
                || std::abs(event.time) > 1e12
                || !std::isfinite(event.time) || !std::isfinite(event.pressure)
                || !std::isfinite(event.tiltX) || !std::isfinite(event.tiltY)
                || !std::isfinite(event.rotationRadians) || !std::isfinite(event.tangentialPressure)
                || std::ranges::any_of(event.custom,[](double v) { return !std::isfinite(v); }))
            throw std::invalid_argument("Pointer values are nonfinite or outside supported coordinate/time limits.");
        auto input = m_input;
        const auto built = appendPointerEvent(input,event);
        if (built.strokeCancelled) {
            const auto restored = m_dirty;
            cancelStroke();
            return {true, false, false, restored, {}};
        }
        BrushResolve resolved;
        if (built.strokeStarted) {
            resolved = resolveBrushPreset(m_preset,m_color,m_nextSeed);
            if (!resolved.brush) throw std::invalid_argument(resolved.errors.front());
            if (m_eraser || event.eraserActive || event.tool == PointerToolKind::Eraser) {
                resolved.brush->rasterizer.blendMode = RasterBlendMode::DestinationOut;
                resolved.brush->rasterizer.argb = 0xFF000000U;
            }
        }
        auto result = built.pointAvailable ? appendPoint(built.point,built.strokeCompleted,
                                                        resolved.brush ? &*resolved.brush : nullptr)
                                           : BitmapPaintUpdate{true,false,m_stream.active,{},{}};
        m_input = input; return result;
    } catch (const std::length_error &error) {
        m_error = error.what();
        return {false, false, m_stream.active, {}, m_error, EngineErrorCode::BudgetExceeded};
    } catch (const std::exception &e) { m_error = e.what(); return {false,false,m_stream.active,{},m_error}; }
}
BitmapPaintUpdate BitmapPaintSession::advanceTime(double seconds)
{
    const auto &r=m_activeBrush.rasterizer;
    const bool catchUp=r.smoothingEnabled && r.smoothing>0
            && r.smoothingMode==BrushSmoothingMode::PulledString && r.smoothingCatchUp;
    if (!m_stream.active || (!r.airbrushEnabled && !catchUp)) {
        m_error = "No active time-based brush or smoothing catch-up."; return {false,false,m_stream.active,{},m_error};
    }
    try {
        if (!std::isfinite(seconds) || std::abs(seconds) > 1e12)
            throw std::invalid_argument("Time is nonfinite or outside the supported range.");
        auto point = m_stream.previousPoint; point.time = seconds;
        if (catchUp) point.position = m_stream.smoothingTarget;
        return appendPoint(point,false);
    } catch (const std::length_error &error) {
        m_error = error.what();
        return {false, false, m_stream.active, {}, m_error, EngineErrorCode::BudgetExceeded};
    } catch (const std::exception &e) { m_error = e.what(); return {false,false,m_stream.active,{},m_error}; }
}
bool BitmapPaintSession::stepHistory(bool forward)
{
    cancelStroke();
    if (forward ? !canRedo() : !canUndo()) return false;
    const auto index = forward ? m_historyCursor : m_historyCursor - 1;
    const auto &delta = (*m_history)[index];
    if (!m_file.applyPixelDelta(delta)) {
        m_error = "Bitmap pixel delta does not match the open file.";
        return false;
    }
    m_historyCursor = forward ? index + 1 : index;
    synchronizePreview(delta.bounds());
    return true;
}
bool BitmapPaintSession::undo() { return stepHistory(false); }
bool BitmapPaintSession::redo() { return stepHistory(true); }
bool BitmapPaintSession::canUndo() const { return historyStatistics().undoEntries != 0; }
bool BitmapPaintSession::canRedo() const { return historyStatistics().redoEntries != 0; }

RasterHistoryStatistics BitmapPaintSession::historyStatistics() const noexcept
{
    if (!m_history || m_history->empty()) return {};
    return {m_historyCursor, m_history->size() - m_historyCursor, m_historyBytes};
}

bool BitmapPaintSession::clear(std::uint32_t argb)
{
    if (!m_file.isOpen()) { m_error = "No bitmap file is open."; return false; }
    try {
        if (applyEdit(BitmapPixelDelta::forFill(m_file.pixels(), argb, historyEntryBudget())) != EngineErrorCode::None) {
            return false;
        }
        resetStrokePixels(false);
        std::fill(m_preview.pixels.begin(), m_preview.pixels.end(), argb);
        m_error.clear();
        return true;
    } catch (const std::exception &error) { m_error = error.what(); return false; }
}
