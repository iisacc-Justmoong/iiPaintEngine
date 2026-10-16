#include "MaskPaintSession.h"
#include "Brush/BrushResolve.h"
#include <algorithm>
#include <cmath>
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
        MaskPaintSession candidate;
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
void MaskPaintSession::resetPendingPixels()
{
    std::fill(m_pending.coverage.begin(),m_pending.coverage.end(),0);
    std::copy(m_mask.coverage.begin(),m_mask.coverage.end(),m_preview.coverage.begin());
    m_input={};m_stream={};m_dirty={};m_referenceArgb=0;
}
void MaskPaintSession::cancelStroke() {resetPendingPixels();m_error.clear();}

double MaskPaintSession::guidanceWeight(DevicePixelPoint point) const
{
    if (!m_activeOptions.autoMask) return 1;
    const auto candidate=m_guidance[indexAt(m_mask,point)];
    const auto alpha=static_cast<double>(candidate >> 24) / 255;
    if (alpha == 0) return 0; // Hidden RGB never contributes to the mask.
    double squareDistance=0;
    for (auto shift : {16U,8U,0U}) {
        const auto difference=(static_cast<double>((candidate >> shift) & 255)
                - static_cast<double>((m_referenceArgb >> shift) & 255)) / 255;
        squareDistance+=difference*difference;
    }
    const auto distance=std::sqrt(squareDistance/3);
    const auto tolerance=m_activeOptions.autoMaskTolerance;
    if (tolerance == 0) return distance == 0 ? alpha : 0;
    const auto t=unit(distance/tolerance);
    return alpha * (1-t*t*(3-2*t));
}
DevicePixelRect MaskPaintSession::applyDabs(std::span<const BrushDab> dabs)
{
    DevicePixelRect changed{};
    const auto &rasterizer=m_activeBrush.rasterizer;
    for (const auto &dab : dabs) {
        const auto bounds=intersectDevicePixelRects({{},m_mask.width,m_mask.height},
                deviceBoundsForBrushDab(dab,rasterizer,{}));
        const auto rx=std::max(0.01,rasterizer.radius*dab.scale*dab.ellipseScaleX);
        const auto ry=std::max(0.01,rasterizer.radius*dab.scale*dab.ellipseScaleY);
        const auto cosine=std::cos(dab.rotationRadians),sine=std::sin(dab.rotationRadians);
        // Float coverage uses the existing procedural round-tip geometry and
        // half-pixel edge model, without quantizing flow through ARGB8 samples.
        for (int y=bounds.origin.y;y<bounds.origin.y+bounds.height;++y) {
            for (int x=bounds.origin.x;x<bounds.origin.x+bounds.width;++x) {
                const auto dx=x-dab.position.x,dy=y-dab.position.y;
                const auto distance=std::hypot((dx*cosine+dy*sine)/rx,(-dx*sine+dy*cosine)/ry);
                const auto edge=unit((1-distance)*std::min(rx,ry)+.5);
                const auto feather=m_activeOptions.feather;
                const auto falloff=feather == 0 ? 1 : unit((1-distance)/feather);
                const auto alpha=unit(m_activeOptions.flow*edge*falloff*guidanceWeight({x,y}));
                if (alpha == 0) continue;
                const auto index=indexAt(m_mask,{x,y});
                const auto pending=m_pending.coverage[index];
                const auto cap=unit(dab.opacityCapScale);
                const auto accumulated=pending >= cap ? pending : std::min(cap,unit(pending+(1-pending)*alpha));
                m_pending.coverage[index]=static_cast<float>(accumulated);
                const auto effect=unit(m_activeOptions.strokeOpacity*accumulated);
                const auto committed=m_mask.coverage[index];
                const auto result=m_activeOptions.mode == MaskBrushMode::Erase
                        ? committed*(1-effect)
                        : committed+std::max(0.0,m_activeOptions.density-committed)*effect;
                const auto coverage=static_cast<float>(unit(result));
                if (coverage != m_preview.coverage[index]) {
                    m_preview.coverage[index]=coverage;
                    changed=uniteDevicePixelRects(changed,{{x,y},1,1});
                }
            }
        }
    }
    m_dirty=uniteDevicePixelRects(m_dirty,changed);return changed;
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
    }
}
bool MaskPaintSession::commitStroke()
{
    try {
        if (isEmpty(m_dirty)) {cancelStroke();return false;}
        auto patch=capturePatch(m_dirty);m_undo.push_back(std::move(patch));
        for (int y=0;y<m_dirty.height;++y) {
            const auto start=indexAt(m_mask,{m_dirty.origin.x,m_dirty.origin.y+y});
            std::copy_n(m_preview.coverage.begin()+start,m_dirty.width,m_mask.coverage.begin()+start);
        }
        m_redo.clear();resetPendingPixels();m_error.clear();return true;
    } catch (const std::exception &error) {m_error=error.what();return false;}
}
MaskPaintUpdate MaskPaintSession::processPointerEvent(const PointerEvent &event)
{
    try {
        if (!hasMask()) throw std::invalid_argument("No pixel mask exists.");
        if (event.phase == PointerEventPhase::Cancel) {cancelStroke();return {true,false,false,{},{}};}
        if (!validEvent(event)) throw std::invalid_argument("Pointer values must be finite and within supported coordinate/time limits.");
        auto input=m_input;const auto built=appendPointerEvent(input,event);
        if (!built.pointAvailable) {m_error.clear();return {true,false,strokeActive(),{},{}};}
        if (built.strokeStarted) {
            auto options=m_options;
            if (event.eraserActive || event.tool == PointerToolKind::Eraser || event.button == PointerButton::Eraser)
                options.mode=MaskBrushMode::Erase;
            std::uint32_t reference=0;
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
            auto resolved=resolveBrushPreset(preset,0xFFFFFFFFU);
            if (!resolved.brush) throw std::invalid_argument(resolved.errors.front());
            resetPendingPixels();m_activeOptions=options;m_activeBrush=std::move(*resolved.brush);
            m_activeBrush.rasterizer.brushSize=options.sizePixels;m_referenceArgb=reference;
        }
        auto stream=m_stream;const auto dabs=appendRasterDabs(stream,built.point,m_activeBrush,built.strokeCompleted);
        const auto changed=applyDabs(dabs);m_stream=stream;m_input=input;
        MaskPaintUpdate result{true,false,strokeActive(),changed,{}};
        if (built.strokeCompleted) {
            result.committed=commitStroke();result.strokeActive=strokeActive();
            if (!result.committed && !m_error.empty()) {result.accepted=false;result.error=m_error;return result;}
        }
        m_error.clear();return result;
    } catch (const std::exception &error) {m_error=error.what();return {false,false,strokeActive(),{},m_error};}
}
bool MaskPaintSession::stepHistory(std::vector<PixelPatch> &from,std::vector<PixelPatch> &to)
{
    cancelStroke();if (from.empty()) return false;
    try {auto inverse=capturePatch(from.back().bounds);to.push_back(std::move(inverse));applyPatch(from.back());from.pop_back();resetPendingPixels();return true;}
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
        std::fill(m_mask.coverage.begin(),m_mask.coverage.end(),coverage);m_redo.clear();resetPendingPixels();return true;
    } catch (const std::exception &error) {m_error=error.what();return false;}
}
