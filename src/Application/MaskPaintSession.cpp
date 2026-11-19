#include "MaskPaintSession.h"
#include "Brush/BrushResolve.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
double unit(double value) {return std::clamp(value,0.0,1.0);}
bool validEvent(const PointerEvent &event)
{
    return std::isfinite(event.documentPosition.x) && std::isfinite(event.documentPosition.y)
            && std::abs(event.documentPosition.x) <= 1e9 && std::abs(event.documentPosition.y) <= 1e9
            && std::isfinite(event.time) && std::abs(event.time) <= 1e12
            && std::isfinite(event.pressure) && std::isfinite(event.tiltX) && std::isfinite(event.tiltY)
            && std::isfinite(event.rotationRadians) && std::isfinite(event.tangentialPressure)
            && std::ranges::all_of(event.custom,[](double value) {return std::isfinite(value);});
}
std::size_t indexAt(const PixelMask &mask,DevicePixelPoint point)
{
    return static_cast<std::size_t>(point.y) * mask.width + point.x;
}

// Copy a clipped rectangle between the equally sized mask planes without touching other coverage.
void copyMaskRegion(const PixelMask &source, PixelMask &destination, DevicePixelRect bounds) noexcept
{
    for (int y = 0; y < bounds.height; ++y) {
        const auto start = indexAt(source, {bounds.origin.x, bounds.origin.y + y});
        std::copy_n(source.coverage.data() + start, bounds.width, destination.coverage.data() + start);
    }
}

// 왼쪽부터 처리한 한 행의 변경 구간만 합쳐 전체 사각형을 갱신한다.
struct DirtyRow {
    void include(int x) noexcept
    {
        first = std::min(first, x);
        last = x;
    }

    void mergeInto(DevicePixelRect &bounds, int y) const noexcept
    {
        if (last >= first) bounds = uniteDevicePixelRects(bounds, {{first, y}, last - first + 1, 1});
    }

    int first = std::numeric_limits<int>::max();
    int last = -1;
};

// Auto Mask 전용 RGB RMS 가중치다. 허용치가 0이면 RGB 일치와 원본 알파만 사용한다.
double guidanceWeight(std::uint32_t candidate, std::uint32_t reference, double tolerance)
{
    const auto alpha = static_cast<double>(candidate >> 24) / 255;
    if (alpha == 0) return 0; // Hidden RGB never contributes to the mask.
    if (tolerance == 0) return ((candidate ^ reference) & 0x00ffffffU) == 0 ? alpha : 0;

    double squareDistance = 0;
    for (auto shift : {16U, 8U, 0U}) {
        const auto difference = (static_cast<double>((candidate >> shift) & 255)
                              - static_cast<double>((reference >> shift) & 255)) / 255;
        squareDistance += difference * difference;
    }
    const auto t = unit(std::sqrt(squareDistance / 3) / tolerance);
    return alpha * (1 - t * t * (3 - 2 * t));
}
}

bool MaskPaintSession::createMask(Types::Pixel width,Types::Pixel height,float clearCoverage)
{
    try {return replaceMask(makePixelMask(width,height,clearCoverage));}
    catch (const std::exception &error) {m_error=error.what();return false;}
}
bool MaskPaintSession::replaceMask(const PixelMask &mask)
{
    try {
        if (!isValidPixelMask(mask)) throw std::invalid_argument("Invalid pixel mask dimensions, pixel count or coverage.");
        MaskPaintSession candidate(m_limits);
        candidate.m_mask=mask;candidate.m_preview=mask;candidate.m_pending=makePixelMask(mask.width,mask.height);
        candidate.m_options=m_options;*this=std::move(candidate);return true;
    } catch (const std::exception &error) {m_error=error.what();return false;}
}
bool MaskPaintSession::hasMask() const {return m_mask.width > 0 && m_mask.height > 0;}
const PixelMask &MaskPaintSession::mask() const {return m_mask;}
const PixelMask &MaskPaintSession::previewMask() const {return m_preview;}
const LocalMaskBrushOptions &MaskPaintSession::brushOptions() const {return m_options;}
bool MaskPaintSession::setBrushOptions(const LocalMaskBrushOptions &options)
{
    if (!isValidLocalMaskBrushOptions(options)) {m_error="Local mask brush options must have supported finite normalized values.";return false;}
    m_options=options;m_error.clear();return true;
}
bool MaskPaintSession::setColorGuidance(std::span<const std::uint32_t> argb)
{
    if (!hasMask() || argb.size() != m_mask.coverage.size()) {m_error="Guidance must contain one ARGB32 pixel per mask pixel.";return false;}
    if (strokeActive()) {m_error="Source color guidance cannot change during a stroke.";return false;}
    try {std::vector<std::uint32_t> pixels(argb.begin(),argb.end());m_guidance=std::move(pixels);m_error.clear();return true;}
    catch (const std::exception &error) {m_error=error.what();return false;}
}
bool MaskPaintSession::clearColorGuidance()
{
    if (strokeActive()) {m_error="Source color guidance cannot change during a stroke.";return false;}
    m_guidance.clear();m_error.clear();return true;
}
bool MaskPaintSession::hasColorGuidance() const {return hasMask() && m_guidance.size() == m_mask.coverage.size();}
bool MaskPaintSession::strokeActive() const {return m_stream.active;}
const std::string &MaskPaintSession::lastError() const {return m_error;}
void MaskPaintSession::resetPendingPixels(bool restorePreview) noexcept
{
    if (!m_pending.coverage.empty()) {
        if (restorePreview) copyMaskRegion(m_mask, m_preview, m_dirty);
        for (int y = 0; y < m_pendingDirty.height; ++y) {
            const auto start = indexAt(m_pending, {m_pendingDirty.origin.x, m_pendingDirty.origin.y + y});
            std::fill_n(m_pending.coverage.data() + start, m_pendingDirty.width, 0.F);
        }
    }
    m_input = {};
    m_stream = {};
    m_dirty = {};
    m_pendingDirty = {};
    m_referenceArgb = 0;
}
void MaskPaintSession::cancelStroke() {resetPendingPixels(true);m_error.clear();}

DevicePixelRect MaskPaintSession::applyDabs(std::span<const BrushDab> dabs)
{
    const auto &options = m_activeOptions;
    if (dabs.empty() || options.flow == 0) return {};

    // 획의 Auto Mask 설정으로 루프를 한 번 선택한다. 겹친 dab의 합성 순서는 유지한다.
    const auto rasterize = [&]<bool WithGuidance>() {
        DevicePixelRect changed{};
        const auto &rasterizer = m_activeBrush.rasterizer;
        const bool erase = options.mode == MaskBrushMode::Erase;
        const auto feather = options.feather;
        // 커버리지의 0 경계에서 double 거리 반올림이 float 픽셀 변경으로 증폭되는 구간이다.
        constexpr double edgePrecision = 8 * std::numeric_limits<double>::epsilon()
                                        / std::numeric_limits<float>::epsilon();
        constexpr int blockWidth = 32;
        std::array<double, blockWidth> distances;
        for (const auto &dab : dabs) {
            const auto bounds = clippedDeviceBoundsForBrushDab(dab, rasterizer, {{}, m_mask.width, m_mask.height});
            if (isEmpty(bounds)) continue;
            const auto rx = std::max(.01, rasterizer.radius * dab.scale * dab.ellipseScaleX);
            const auto ry = std::max(.01, rasterizer.radius * dab.scale * dab.ellipseScaleY);
            const auto radius = std::min(rx, ry);
            const auto cutoff = feather == 0 ? 1 + .5 / radius : 1.;
            const auto cutoffTolerance = cutoff * edgePrecision;
            const auto cosine = std::cos(dab.rotationRadians), sine = std::sin(dab.rotationRadians);
            const auto cap = unit(dab.opacityCapScale);
            const auto right = bounds.origin.x + bounds.width;
            for (int y = bounds.origin.y; y < bounds.origin.y + bounds.height; ++y) {
                const auto dy = y - dab.position.y;
                const auto rowU = dy * sine, rowV = dy * cosine;
                const auto rowStart = indexAt(m_mask, {0, y});
                auto *const pendingRow = m_pending.coverage.data() + rowStart;
                auto *const previewRow = m_preview.coverage.data() + rowStart;
                const auto *const committedRow = m_mask.coverage.data() + rowStart;
                const std::uint32_t *guidanceRow = nullptr;
                if constexpr (WithGuidance) guidanceRow = m_guidance.data() + rowStart;
                DirtyRow newPending, changedPreview;

                for (int left = bounds.origin.x; left < right;) {
                    const auto count = std::min(blockWidth, right - left);
                    // 검증된 좌표와 최소 반경에서 제곱 거리는 double 범위 안이다.
                    // 행의 독립적인 거리 계산을 모아 컴파일러의 SIMD 경로를 허용한다.
                    for (int i = 0; i < count; ++i) {
                        const auto dx = left + i - dab.position.x;
                        const auto u = (dx * cosine + rowU) / rx;
                        const auto v = (-dx * sine + rowV) / ry;
                        distances[i] = std::sqrt(u * u + v * v);
                    }
                    for (int i = 0; i < count; ++i) {
                        const auto x = left + i;
                        auto distance = distances[i];
                        // 접선 근처의 0 판정과 미세 커버리지는 기존 정밀 거리식으로 계산한다.
                        if (std::abs(distance - cutoff) <= cutoffTolerance) [[unlikely]] {
                            const auto dx = x - dab.position.x;
                            distance = std::hypot((dx * cosine + dy * sine) / rx, (-dx * sine + dy * cosine) / ry);
                        }
                        const auto fromEdge = 1 - distance;
                        const auto edge = unit(fromEdge * radius + .5);
                        const auto falloff = feather == 0 ? 1 : unit(fromEdge / feather);
                        auto alpha = options.flow * edge * falloff;
                        if constexpr (WithGuidance) {
                            if (alpha == 0) continue;
                            alpha = unit(alpha * guidanceWeight(guidanceRow[x], m_referenceArgb, options.autoMaskTolerance));
                        }
                        if (alpha == 0) continue;

                        const auto pending = pendingRow[x];
                        const auto accumulated = pending >= cap ? pending : std::min(cap, unit(pending + (1 - pending) * alpha));
                        const auto nextPending = static_cast<float>(accumulated);
                        if (nextPending != pending) {
                            pendingRow[x] = nextPending;
                            if (pending == 0.F) newPending.include(x);
                        }
                        const auto effect = unit(options.strokeOpacity * accumulated);
                        const auto committed = committedRow[x];
                        const auto result = erase ? committed * (1 - effect)
                            : committed + std::max(0., options.density - committed) * effect;
                        const auto coverage = static_cast<float>(unit(result));
                        if (coverage != previewRow[x]) {
                            previewRow[x] = coverage;
                            changedPreview.include(x);
                        }
                    }
                    left += count;
                }
                newPending.mergeInto(m_pendingDirty, y);
                changedPreview.mergeInto(changed, y);
            }
        }
        return changed;
    };
    const auto changed = options.autoMask ? rasterize.operator()<true>() : rasterize.operator()<false>();
    m_dirty = uniteDevicePixelRects(m_dirty, changed);
    return changed;
}
MaskPaintSession::PixelPatch MaskPaintSession::capturePatch(DevicePixelRect bounds) const
{
    PixelPatch patch{bounds,{}};patch.coverage.reserve(static_cast<std::size_t>(bounds.width)*bounds.height);
    for (int y=0;y<bounds.height;++y) {
        const auto start=indexAt(m_mask,{bounds.origin.x,bounds.origin.y+y});
        patch.coverage.insert(patch.coverage.end(),m_mask.coverage.begin()+start,m_mask.coverage.begin()+start+bounds.width);
    }
    return patch;
}
void MaskPaintSession::applyPatch(const PixelPatch &patch)
{
    for (int y=0;y<patch.bounds.height;++y) {
        const auto start=indexAt(m_mask,{patch.bounds.origin.x,patch.bounds.origin.y+y});
        const auto offset=static_cast<std::size_t>(y)*patch.bounds.width;
        std::copy_n(patch.coverage.begin()+offset,patch.bounds.width,m_mask.coverage.begin()+start);
        std::copy_n(patch.coverage.begin()+offset,patch.bounds.width,m_preview.coverage.begin()+start);
    }
}
bool MaskPaintSession::commitStroke()
{
    try {
        if (isEmpty(m_dirty)) {cancelStroke();return false;}
        auto patch=capturePatch(m_dirty);m_undo.push_back(std::move(patch));
        copyMaskRegion(m_preview, m_mask, m_dirty);
        m_redo.clear();resetPendingPixels(false);m_error.clear();return true;
    } catch (const std::exception &error) {m_error=error.what();return false;}
}
MaskPaintUpdate MaskPaintSession::processPointerEvent(const PointerEvent &event)
{
    try {
        if (!hasMask()) throw std::invalid_argument("No pixel mask exists.");
        if (event.phase == PointerEventPhase::Cancel) {
            const auto restored = m_dirty;
            cancelStroke();
            return {true, false, false, restored, {}};
        }
        if (!validEvent(event)) throw std::invalid_argument("Pointer values must be finite and within supported coordinate/time limits.");
        auto input=m_input;const auto built=appendPointerEvent(input,event);
        if (!built.pointAvailable) {m_error.clear();return {true,false,strokeActive(),{},{}};}
        auto options = m_options;
        std::uint32_t reference = 0;
        BrushResolve resolved;
        if (built.strokeStarted) {
            if (event.eraserActive || event.tool == PointerToolKind::Eraser || event.button == PointerButton::Eraser)
                options.mode=MaskBrushMode::Erase;
            if (options.autoMask) {
                if (!hasColorGuidance()) throw std::invalid_argument("Auto-mask requires source color guidance.");
                const auto x=std::floor(event.documentPosition.x+.5),y=std::floor(event.documentPosition.y+.5);
                if (x < 0 || y < 0 || x >= m_mask.width || y >= m_mask.height)
                    throw std::invalid_argument("Auto-mask stroke must start inside the source image.");
                reference=m_guidance[indexAt(m_mask,{static_cast<Types::Pixel>(x),static_cast<Types::Pixel>(y)})];
                if ((reference >> 24) == 0) throw std::invalid_argument("Auto-mask stroke cannot start on a fully transparent source pixel.");
            }
            BrushPreset preset;preset.size=static_cast<float>(options.sizePixels);preset.hardness=static_cast<float>(1-options.feather);
            preset.flow=1;preset.opacity=1;preset.density=1; // Local coverage density never affects placement.
            preset.dynamics.pressureToSize=options.pressureAffectsSize ? 1 : 0;
            preset.dynamics.pressureToOpacity=options.pressureAffectsOpacity ? 1 : 0;
            resolved=resolveBrushPreset(preset,0xFFFFFFFFU);
            if (!resolved.brush) throw std::invalid_argument(resolved.errors.front());
            resolved.brush->rasterizer.brushSize=options.sizePixels;
        }
        auto stream = built.strokeStarted ? RasterDabStream{} : m_stream;
        const auto &brush = resolved.brush ? *resolved.brush : m_activeBrush;
        const auto dabs = appendRasterDabsWithLimit(stream, built.point, brush, m_limits.maxDabsPerEvent,
                                                    built.strokeCompleted);
        checkedProjectedPixelCount(BrushDabSpan{dabs}, brush.rasterizer, {{},m_mask.width,m_mask.height},
                                   m_limits.maxProjectedPixels);
        DevicePixelRect replacedPreview{};
        if (resolved.brush) {
            replacedPreview = m_dirty;
            resetPendingPixels(true);
            m_activeOptions = options;
            m_activeBrush = std::move(*resolved.brush);
            m_referenceArgb = reference;
        }
        const auto changed=applyDabs(dabs);m_stream=stream;m_input=input;
        MaskPaintUpdate result{true,false,strokeActive(),uniteDevicePixelRects(replacedPreview, changed),{}};
        if (built.strokeCompleted) {
            result.committed=commitStroke();result.strokeActive=strokeActive();
            if (!result.committed && !m_error.empty()) {result.accepted=false;result.error=m_error;return result;}
        }
        m_error.clear();return result;
    } catch (const std::length_error &error) {
        m_error = error.what();
        return {false, false, strokeActive(), {}, m_error, EngineErrorCode::BudgetExceeded};
    } catch (const std::exception &error) {m_error=error.what();return {false,false,strokeActive(),{},m_error};}
}
bool MaskPaintSession::stepHistory(std::vector<PixelPatch> &from,std::vector<PixelPatch> &to)
{
    cancelStroke();
    if (from.empty()) return false;
    try {
        auto inverse = capturePatch(from.back().bounds);
        to.push_back(std::move(inverse));
        applyPatch(from.back());
        from.pop_back();
        return true;
    }
    catch (const std::exception &error) {m_error=error.what();return false;}
}
bool MaskPaintSession::undo() {return stepHistory(m_undo,m_redo);}
bool MaskPaintSession::redo() {return stepHistory(m_redo,m_undo);}
bool MaskPaintSession::canUndo() const {return !m_undo.empty();}
bool MaskPaintSession::canRedo() const {return !m_redo.empty();}
bool MaskPaintSession::clear(float coverage)
{
    if (!hasMask() || !std::isfinite(coverage) || coverage < 0 || coverage > 1) {m_error="Clear requires a mask and finite normalized coverage.";return false;}
    try {
        cancelStroke();
        if (std::ranges::all_of(m_mask.coverage,[&](float value) {return value == coverage;})) return false;
        auto before=capturePatch({{},m_mask.width,m_mask.height});m_undo.push_back(std::move(before));
        std::fill(m_mask.coverage.begin(),m_mask.coverage.end(),coverage);
        std::fill(m_preview.coverage.begin(),m_preview.coverage.end(),coverage);
        m_redo.clear();return true;
    } catch (const std::exception &error) {m_error=error.what();return false;}
}
