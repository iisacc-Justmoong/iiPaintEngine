#include "BitmapPaintSession.h"
#include "Brush/BrushPresetSerializer.h"
#include <algorithm>
#include <cmath>

void BitmapPaintSession::initializePixels()
{
    m_pending = makeStrokeCompositeBuffer(m_file.width(),m_file.height());
    m_preview = m_file.pixels(); m_input = {}; m_stream = {}; m_dirty = {};
}
bool BitmapPaintSession::createFile(const std::filesystem::path &path,int width,int height,
                                   std::string_view format,std::uint32_t clearArgb)
{
    try {
        if (width <= 0 || height <= 0 || width > maxBitmapDimension || height > maxBitmapDimension
                || static_cast<std::uint64_t>(width) * height > maxBitmapPixelCount) {
            m_error = "Invalid bitmap dimensions."; return false;
        }
        BitmapPaintSession candidate;
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
        BitmapPaintSession candidate;
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
    m_dirty = {}; m_undo.clear(); m_redo.clear(); m_error.clear();
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
void BitmapPaintSession::cancelStroke() { initializePixels(); m_error.clear(); }

BitmapPaintSession::PixelPatch BitmapPaintSession::capturePatch(DevicePixelRect bounds) const
{
    PixelPatch patch{bounds,{}};
    patch.pixels.reserve(static_cast<std::size_t>(bounds.width) * bounds.height);
    for (int y = 0;y < bounds.height;++y) {
        const auto start = static_cast<std::size_t>(bounds.origin.y+y) * m_file.width() + bounds.origin.x;
        const auto &pixels = m_file.pixels().pixels;
        patch.pixels.insert(patch.pixels.end(),pixels.begin()+start,pixels.begin()+start+bounds.width);
    }
    return patch;
}
bool BitmapPaintSession::commitStroke()
{
    if (isEmpty(m_dirty)) { initializePixels(); return false; }
    auto before = capturePatch(m_dirty);
    std::vector<std::uint32_t> after;
    after.reserve(before.pixels.size());
    for (int y = 0;y < m_dirty.height;++y) {
        const auto start = static_cast<std::size_t>(m_dirty.origin.y+y) * m_file.width() + m_dirty.origin.x;
        after.insert(after.end(),m_preview.pixels.begin()+start,m_preview.pixels.begin()+start+m_dirty.width);
    }
    m_undo.push_back(std::move(before));
    if (!m_file.replacePixelPatch(m_dirty,after)) { m_undo.pop_back(); m_error = m_file.lastError(); return false; }
    m_redo.clear();
    if (m_activeBrush.material.erodible.enabled && m_activePresetRevision == m_presetRevision)
        m_preset.material.erodible.wear = m_stream.tipWear;
    ++m_nextSeed; initializePixels(); return true;
}
BitmapPaintUpdate BitmapPaintSession::appendPoint(const StrokePoint &point,bool finish)
{
    auto stream = m_stream;
    const auto dabs = appendRasterDabs(stream,point,m_activeBrush,finish);
    RasterSourceSampler sampler{&m_file.pixels(),[](const void *context,DevicePixelPoint p) {
        return rasterLayerPixelAt(*static_cast<const RasterLayer *>(context),p);
    },{},m_file.width(),m_file.height()};
    const auto samples = projectBrushDabs(dabs,m_activeBrush.rasterizer,{},sampler,m_activeBrush.material);
    const auto bounds = intersectDevicePixelRects({{},m_file.width(),m_file.height()},
            deviceBoundsForBrushDabsUnion(dabs,m_activeBrush.rasterizer,{}));
    accumulateStrokeSamples(m_pending,samples);
    copyStrokePreviewSamplePixels(m_preview,m_file.pixels(),m_pending,samples,
                                 m_activeBrush.rasterizer.blendMode == RasterBlendMode::DestinationOut);
    if (!samples.empty()) m_dirty = uniteDevicePixelRects(m_dirty,bounds);
    m_stream = std::move(stream);
    BitmapPaintUpdate result{true,false,m_stream.active,bounds,{}};
    if (finish) {
        result.committed = commitStroke(); result.strokeActive = false;
        if (!result.committed && !m_error.empty()) {
            result.accepted = false; result.error = m_error; return result;
        }
    }
    m_error.clear(); return result;
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
        if (built.strokeCancelled) { cancelStroke(); return {true,false,false,{},{}}; }
        if (built.strokeStarted) {
            auto resolved = resolveBrushPreset(m_preset,m_color,m_nextSeed);
            if (!resolved.brush) throw std::invalid_argument(resolved.errors.front());
            initializePixels(); m_activeBrush = std::move(*resolved.brush); m_activePresetRevision = m_presetRevision;
            if (m_eraser || event.eraserActive || event.tool == PointerToolKind::Eraser) {
                m_activeBrush.rasterizer.blendMode = RasterBlendMode::DestinationOut;
                m_activeBrush.rasterizer.argb = 0xFF000000U;
            }
        }
        auto result = built.pointAvailable ? appendPoint(built.point,built.strokeCompleted)
                                           : BitmapPaintUpdate{true,false,m_stream.active,{},{}};
        m_input = input; return result;
    } catch (const std::exception &e) { m_error = e.what(); return {false,false,m_stream.active,{},m_error}; }
}
BitmapPaintUpdate BitmapPaintSession::advanceTime(double seconds)
{
    if (!m_stream.active || !m_activeBrush.rasterizer.airbrushEnabled) {
        m_error = "No active time-based brush."; return {false,false,m_stream.active,{},m_error};
    }
    try {
        if (!std::isfinite(seconds) || std::abs(seconds) > 1e12)
            throw std::invalid_argument("Time is nonfinite or outside the supported range.");
        auto point = m_stream.previousPoint; point.time = seconds;
        return appendPoint(point,false);
    } catch (const std::exception &e) { m_error = e.what(); return {false,false,m_stream.active,{},m_error}; }
}
bool BitmapPaintSession::stepHistory(std::vector<PixelPatch> &from,std::vector<PixelPatch> &to)
{
    cancelStroke();
    if (from.empty()) return false;
    auto inverse = capturePatch(from.back().bounds);
    to.push_back(std::move(inverse));
    if (!m_file.replacePixelPatch(from.back().bounds,from.back().pixels)) { to.pop_back(); m_error = m_file.lastError(); return false; }
    from.pop_back(); initializePixels(); m_error.clear(); return true;
}
bool BitmapPaintSession::undo() { return stepHistory(m_undo,m_redo); }
bool BitmapPaintSession::redo() { return stepHistory(m_redo,m_undo); }
bool BitmapPaintSession::canUndo() const { return !m_undo.empty(); }
bool BitmapPaintSession::canRedo() const { return !m_redo.empty(); }
bool BitmapPaintSession::clear(std::uint32_t argb)
{
    if (!m_file.isOpen()) { m_error = "No bitmap file is open."; return false; }
    cancelStroke();
    auto patch = capturePatch({{},m_file.width(),m_file.height()});
    m_undo.push_back(std::move(patch));
    if (!m_file.clear(argb)) { m_undo.pop_back(); m_error = m_file.lastError(); return false; }
    m_redo.clear(); initializePixels(); m_error.clear(); return true;
}
