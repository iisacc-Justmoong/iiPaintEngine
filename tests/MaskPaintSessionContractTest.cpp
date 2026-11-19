#include <iiPaintEngine>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <utility>

// Seed otherwise untouched storage to detect even redundant writes of the same pixel values.
struct MaskPaintSessionTestAccess {
    static PixelMask &pending(MaskPaintSession &session) noexcept { return session.m_pending; }
    static PixelMask &preview(MaskPaintSession &session) noexcept { return session.m_preview; }
};

#define CHECK(c) do {if(!(c)){std::cerr<<__LINE__<<": " #c "\n";return 1;}}while(false)
namespace {
PointerEvent event(PointerEventPhase phase,double x,double y,double time=0)
{
    PointerEvent value;
    value.phase=phase;value.documentPosition={x,y};value.time=time;
    value.button=phase==PointerEventPhase::Press?PointerButton::Primary:PointerButton::None;
    value.primaryButtonDown=phase!=PointerEventPhase::Release;return value;
}
float at(const PixelMask& mask,int x,int y){return mask.coverage[static_cast<std::size_t>(y)*mask.width+x];}
bool near(double actual,double expected,double epsilon=1e-6){return std::abs(actual-expected)<epsilon;}
bool click(MaskPaintSession& session,double x,double y,double time)
{
    return session.processPointerEvent(event(PointerEventPhase::Press,x,y,time)).accepted
        &&session.processPointerEvent(event(PointerEventPhase::Release,x,y,time)).committed;
}

// Test membership in the half-open rectangle returned by a mask paint event.
bool inside(DevicePixelRect bounds, int x, int y)
{
    return x >= bounds.origin.x && x < bounds.origin.x + bounds.width
            && y >= bounds.origin.y && y < bounds.origin.y + bounds.height;
}

// Compare the full bounds returned for an observed coverage change.
bool sameBounds(DevicePixelRect actual, DevicePixelRect expected)
{
    return actual.origin.x == expected.origin.x && actual.origin.y == expected.origin.y
            && actual.width == expected.width && actual.height == expected.height;
}

// Check both sides of the guard, including every interior pixel of the expected write rectangle.
bool matchesRange(const PixelMask &mask, DevicePixelRect bounds, float insideValue, float outsideValue)
{
    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            if (at(mask, x, y) != (inside(bounds, x, y) ? insideValue : outsideValue)) return false;
        }
    }
    return true;
}

// Exercise physical writes through the live cancel, commit, and history paths with guard values.
int checkResetRanges()
{
    constexpr float background = .25F;
    constexpr float pendingCanary = .125F;
    constexpr float previewCanary = .75F;
    MaskPaintSession session;
    CHECK(session.setBrushOptions(LocalMaskBrushOptions{.sizePixels = 5, .feather = 0}));
    for (const auto phase : {PointerEventPhase::Cancel, PointerEventPhase::Release}) {
        CHECK(session.createMask(32, 24, background));
        const auto press = session.processPointerEvent(event(PointerEventPhase::Press, 7, 8, 1));
        CHECK(press.accepted && !isEmpty(press.dirtyBounds));
        auto &pending = MaskPaintSessionTestAccess::pending(session);
        auto &preview = MaskPaintSessionTestAccess::preview(session);
        DevicePixelRect pendingBounds{}, previewBounds{};
        for (int y = 0; y < pending.height; ++y) {
            for (int x = 0; x < pending.width; ++x) {
                const DevicePixelRect pixel{{x, y}, 1, 1};
                if (at(pending, x, y) != 0.F) pendingBounds = uniteDevicePixelRects(pendingBounds, pixel);
                if (at(preview, x, y) != background) previewBounds = uniteDevicePixelRects(previewBounds, pixel);
            }
        }
        CHECK(sameBounds(press.dirtyBounds, previewBounds));
        std::ranges::fill(pending.coverage, pendingCanary);
        std::ranges::fill(preview.coverage, previewCanary);

        const auto result = session.processPointerEvent(event(phase, 7, 8, 2));
        CHECK(result.accepted && !result.strokeActive);
        CHECK(matchesRange(pending, pendingBounds, 0.F, pendingCanary));
        if (phase == PointerEventPhase::Cancel) {
            CHECK(!result.committed && !session.canUndo());
            CHECK(matchesRange(preview, previewBounds, background, previewCanary));
            CHECK(sameBounds(result.dirtyBounds, previewBounds));
        } else {
            CHECK(result.committed && matchesRange(session.mask(), previewBounds, previewCanary, background));
            CHECK(matchesRange(preview, {}, previewCanary, previewCanary));

            // History updates only its patch and writes nothing to idle pending storage.
            std::ranges::fill(pending.coverage, pendingCanary);
            CHECK(session.undo());
            CHECK(matchesRange(preview, previewBounds, background, previewCanary));
            CHECK(matchesRange(pending, {}, pendingCanary, pendingCanary));
            constexpr float redoCanary = .875F;
            std::ranges::fill(preview.coverage, redoCanary);
            CHECK(session.redo());
            CHECK(matchesRange(preview, previewBounds, previewCanary, redoCanary));
            CHECK(matchesRange(pending, {}, pendingCanary, pendingCanary));
        }
    }
    return 0;
}

// A saturated Add stroke still writes pending coverage even when it has an empty preview dirty rectangle.
int checkInvisibleAccumulation()
{
    for (const auto phase : {PointerEventPhase::Cancel, PointerEventPhase::Release}) {
        MaskPaintSession session;
        CHECK(session.createMask(8, 8, .75F));
        LocalMaskBrushOptions brush{.sizePixels = 1, .feather = 0, .density = .75};
        CHECK(session.setBrushOptions(brush));
        const auto press = session.processPointerEvent(event(PointerEventPhase::Press, 4, 4, 1));
        CHECK(press.accepted && isEmpty(press.dirtyBounds));
        CHECK(at(MaskPaintSessionTestAccess::pending(session), 4, 4) == 1.F);
        const auto result = session.processPointerEvent(event(phase, 4, 4, 2));
        CHECK(result.accepted && !result.committed && !session.canUndo());
        CHECK(matchesRange(MaskPaintSessionTestAccess::pending(session), {}, 0.F, 0.F));

        brush.mode = MaskBrushMode::Erase;
        brush.flow = .25;
        CHECK(session.setBrushOptions(brush) && click(session, 4, 4, 3));
        CHECK(at(session.mask(), 4, 4) == .5625F);
    }
    return 0;
}

// Compare the entire mask, including bit patterns of untouched floats such as negative zero.
bool samePixels(const PixelMask &actual, const PixelMask &expected)
{
    return actual.width == expected.width && actual.height == expected.height
            && actual.coverage.size() == expected.coverage.size()
            && (expected.coverage.empty() || std::memcmp(actual.coverage.data(), expected.coverage.data(),
                                                        expected.coverage.size() * sizeof(float)) == 0);
}

// A 1px brush on integer centers deposits only at the center. The resulting coverage is exactly dyadic.
int checkSmallStrokeGolden()
{
    auto golden = makePixelMask(2048, 1536, .5F);
    golden.coverage[17] = -0.F;
    MaskPaintSession session;
    CHECK(session.replaceMask(golden));
    const auto *const previewStorage = session.previewMask().coverage.data();
    LocalMaskBrushOptions brush{.sizePixels = 1, .feather = 0, .flow = .25};
    CHECK(session.setBrushOptions(brush) && click(session, 0, 0, 1));
    golden.coverage.front() = .625F; // .5 + (1 - .5) * .25
    CHECK(samePixels(session.mask(), golden) && samePixels(session.previewMask(), golden));

    // A valid replacement press rolls back the previous active region before painting its own region.
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press, 17, 32, 2)).accepted);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press, 64, 56, 3)).accepted);
    auto active = golden;
    active.coverage[56 * static_cast<std::size_t>(active.width) + 64] = .625F;
    CHECK(samePixels(session.previewMask(), active) && samePixels(session.mask(), golden));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Cancel, 64, 56, 4)).accepted);
    CHECK(samePixels(session.previewMask(), golden));

    brush.mode = MaskBrushMode::Erase;
    CHECK(session.setBrushOptions(brush) && click(session, 0, 0, 5));
    auto erased = golden;
    erased.coverage.front() = .46875F; // .625 * (1 - .25)
    CHECK(samePixels(session.mask(), erased) && samePixels(session.previewMask(), erased));

    // Undo discards an unrelated active region and applies only the last committed patch.
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press, 13, 22, 6)).accepted);
    CHECK(session.undo() && !session.strokeActive());
    CHECK(samePixels(session.mask(), golden) && samePixels(session.previewMask(), golden));
    CHECK(session.redo() && samePixels(session.mask(), erased) && samePixels(session.previewMask(), erased));
    CHECK(session.previewMask().coverage.data() == previewStorage);

    // Cancelling the moved-from object must leave the destination's active stroke and buffers intact.
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press, 13, 22, 7)).accepted);
    MaskPaintSession moved(std::move(session));
    session.cancelStroke();
    CHECK(!session.strokeActive() && moved.strokeActive());
    moved.cancelStroke();
    CHECK(samePixels(moved.previewMask(), erased));
    return 0;
}
}
int main()
{
    if (const auto failure = checkResetRanges(); failure != 0) return failure;
    if (const auto failure = checkInvisibleAccumulation(); failure != 0) return failure;
    if (const auto failure = checkSmallStrokeGolden(); failure != 0) return failure;
    MaskPaintSession session;
    CHECK(!session.hasMask()&&!session.strokeActive());
    CHECK(session.createMask(32,24));
    LocalMaskBrushOptions options;options.sizePixels=8;options.feather=0;options.flow=.25;
    CHECK(session.setBrushOptions(options));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,12,12)).accepted);
    CHECK(at(session.mask(),12,12)==0&&near(at(session.previewMask(),12,12),.25));
    const auto first=session.previewMask().coverage;
    CHECK(session.processPointerEvent(event(PointerEventPhase::Move,16,12,1)).accepted);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Move,12,12,2)).accepted);
    CHECK(at(session.previewMask(),12,12)>.25&&at(session.previewMask(),12,12)<1);
    const auto preview=session.previewMask().coverage;
    CHECK(session.processPointerEvent(event(PointerEventPhase::Release,12,12,2)).committed);
    CHECK(session.mask().coverage==preview&&!session.strokeActive()&&session.canUndo());
    CHECK(session.undo()&&std::ranges::all_of(session.mask().coverage,[](float v){return v==0;}));
    CHECK(session.redo()&&session.mask().coverage==preview);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,4,4,3)).accepted);
    session.cancelStroke();CHECK(session.previewMask().coverage==preview&&session.mask().coverage==preview);

    CHECK(session.createMask(32,24));options.flow=1;options.density=.6;options.strokeOpacity=.25;
    CHECK(session.setBrushOptions(options));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,12,12,4)).accepted);
    CHECK(near(at(session.previewMask(),12,12),.15));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Move,16,12,5)).accepted);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Move,12,12,6)).accepted);
    CHECK(near(at(session.previewMask(),12,12),.15));
    CHECK(session.commitStroke()&&near(at(session.mask(),12,12),.15));
    CHECK(click(session,12,12,7)&&near(at(session.mask(),12,12),.2625));
    options.strokeOpacity=1;CHECK(session.setBrushOptions(options));
    CHECK(click(session,12,12,8)&&near(at(session.mask(),12,12),.6));
    CHECK(click(session,12,12,9)&&near(at(session.mask(),12,12),.6));
    options.density=.1;CHECK(session.setBrushOptions(options));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,12,12,10)).accepted);
    CHECK(near(at(session.previewMask(),12,12),.6));session.cancelStroke();
    options.mode=MaskBrushMode::Erase;options.strokeOpacity=.5;options.density=0;
    CHECK(session.setBrushOptions(options));CHECK(click(session,12,12,11)&&near(at(session.mask(),12,12),.3));
    CHECK(session.undo()&&near(at(session.mask(),12,12),.6));
    CHECK(session.redo()&&near(at(session.mask(),12,12),.3));

    CHECK(session.createMask(32,24));options={};options.sizePixels=12;options.feather=1;
    CHECK(session.setBrushOptions(options));CHECK(click(session,12,12,12));
    CHECK(near(at(session.mask(),12,12),1));
    CHECK(at(session.mask(),16,12)>0&&at(session.mask(),16,12)<at(session.mask(),13,12));
    CHECK(at(session.mask(),19,12)==0);
    const auto feathered=session.mask();
    CHECK(session.createMask(32,24));options.feather=0;CHECK(session.setBrushOptions(options));CHECK(click(session,12,12,13));
    CHECK(at(session.mask(),16,12)>at(feathered,16,12));
    const auto hard=session.mask();
    options.sizePixels=4;CHECK(session.createMask(32,24)&&session.setBrushOptions(options));CHECK(click(session,12,12,14));
    CHECK(at(session.mask(),16,12)==0&&at(hard,16,12)>0);

    CHECK(session.createMask(8,8));options={};options.sizePixels=4;options.flow=.001;
    CHECK(session.setBrushOptions(options));CHECK(click(session,4,4,15));
    CHECK(near(at(session.mask(),4,4),.001,1e-8));
    options.flow=0;CHECK(session.setBrushOptions(options));const auto unchanged=session.mask().coverage;
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,4,4,16)).accepted);
    CHECK(!session.processPointerEvent(event(PointerEventPhase::Release,4,4,16)).committed);
    CHECK(session.mask().coverage==unchanged);
    options.flow=1;CHECK(session.setBrushOptions(options));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,1,1,17)).accepted);
    const auto activePreview=session.previewMask().coverage;
    auto selected=options;selected.mode=MaskBrushMode::Erase;selected.sizePixels=8;
    CHECK(session.setBrushOptions(selected)&&session.previewMask().coverage==activePreview);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Release,1,1,17)).committed&&at(session.mask(),1,1)>0);
    CHECK(click(session,1,1,18)&&at(session.mask(),1,1)==0);
    CHECK(!session.canRedo());

    const auto before=session.mask().coverage;
    CHECK(!session.createMask(-1,8)&&session.mask().coverage==before);
    CHECK(!session.createMask(16384,16384)&&session.mask().coverage==before);
    PixelMask invalid=session.mask();invalid.coverage[0]=std::numeric_limits<float>::quiet_NaN();
    CHECK(!session.replaceMask(invalid)&&session.mask().coverage==before);
    auto bad=options;bad.feather=std::numeric_limits<double>::infinity();CHECK(!session.setBrushOptions(bad));
    bad=options;bad.density=1.1;CHECK(!session.setBrushOptions(bad));
    bad=options;bad.mode=static_cast<MaskBrushMode>(-1);CHECK(!session.setBrushOptions(bad));
    auto malformed=event(PointerEventPhase::Press,1,1,19);malformed.documentPosition.x=std::numeric_limits<double>::quiet_NaN();
    CHECK(!session.processPointerEvent(malformed).accepted&&!session.strokeActive()&&session.mask().coverage==before);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,2,2,20)).accepted);
    const auto safePreview=session.previewMask().coverage;
    CHECK(!session.processPointerEvent(event(PointerEventPhase::Move,3,3,19)).accepted);
    CHECK(session.previewMask().coverage==safePreview&&session.strokeActive());
    CHECK(!session.processPointerEvent(event(PointerEventPhase::Move,1e9,1e9,21)).accepted);
    CHECK(session.previewMask().coverage==safePreview&&session.strokeActive());
    CHECK(session.processPointerEvent(event(PointerEventPhase::Cancel,0,0,21)).accepted&&!session.strokeActive());
    CHECK(session.previewMask().coverage==session.mask().coverage);
    CHECK(session.createMask(8,8,1));options.mode=MaskBrushMode::Add;options.density=1;
    CHECK(session.setBrushOptions(options));auto eraser=event(PointerEventPhase::Press,4,4,22);eraser.tool=PointerToolKind::Eraser;
    CHECK(session.processPointerEvent(eraser).accepted&&at(session.previewMask(),4,4)==0);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Release,4,4,22)).committed);
    CHECK(session.createMask(24,24));options={};options.sizePixels=16;options.feather=0;
    options.pressureAffectsSize=true;options.pressureAffectsOpacity=true;
    CHECK(session.setBrushOptions(options));
    auto pen=event(PointerEventPhase::Press,12,12,23);pen.device=PointerDeviceKind::Tablet;pen.tool=PointerToolKind::Pen;pen.pressure=.25;
    CHECK(session.processPointerEvent(pen).accepted&&near(at(session.previewMask(),12,12),.25));
    CHECK(at(session.previewMask(),16,12)==0);
    pen.phase=PointerEventPhase::Release;pen.primaryButtonDown=false;
    CHECK(session.processPointerEvent(pen).committed);const auto lowPressure=session.mask();
    CHECK(session.createMask(24,24));pen.phase=PointerEventPhase::Press;pen.primaryButtonDown=true;pen.pressure=1;pen.time=24;
    CHECK(session.processPointerEvent(pen).accepted&&near(at(session.previewMask(),12,12),1));
    CHECK(at(session.previewMask(),16,12)>at(lowPressure,16,12));
    pen.phase=PointerEventPhase::Move;pen.documentPosition={13,12};pen.pressure=.25;pen.time=25;
    CHECK(session.processPointerEvent(pen).accepted&&near(at(session.previewMask(),12,12),1));
    session.cancelStroke();
    auto mouse=event(PointerEventPhase::Press,12,12,26);mouse.pressure=.25;
    CHECK(session.processPointerEvent(mouse).accepted&&near(at(session.previewMask(),12,12),1));session.cancelStroke();
    const auto beforeClear=session.mask().coverage;
    CHECK(session.clear(.2F)&&std::ranges::all_of(session.mask().coverage,[](float v){return v==.2F;}));
    CHECK(session.undo()&&session.mask().coverage==beforeClear);
    CHECK(session.redo()&&std::ranges::all_of(session.mask().coverage,[](float v){return v==.2F;}));
    CHECK(!session.clear(-.1F)&&std::ranges::all_of(session.mask().coverage,[](float v){return v==.2F;}));
    CHECK(session.createMask(8,8));options={};options.sizePixels=8;options.feather=0;
    CHECK(session.setBrushOptions(options));
    auto clipped=session.processPointerEvent(event(PointerEventPhase::Press,-1,0,27));
    CHECK(clipped.accepted&&clipped.dirtyBounds.origin.x>=0&&clipped.dirtyBounds.origin.y>=0);
    CHECK(clipped.dirtyBounds.width<=8&&clipped.dirtyBounds.height<=8&&at(session.previewMask(),0,0)>0);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Release,-1,0,27)).committed);
    BrushPreset legacy;CHECK(legacy.density==1);legacy.density=8;CHECK(legacy.density==8);
    std::cout<<"Local mask brush pixels, flow, caps, feather, erase, lifecycle, history and rejection passed\n";
}
