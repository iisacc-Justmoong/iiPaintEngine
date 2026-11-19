#include <iiPaintEngine>

#include "TestDirectory.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>
#include <utility>

namespace {
struct AllocationCounts {
    std::size_t calls = 0;
    std::size_t bytes = 0;
    std::size_t largest = 0;
};

thread_local AllocationCounts *measured = nullptr;
bool engineAllocationsVisible = false;
std::size_t largestRejection = 0;

// Count requested C++ heap bytes only while invoking the engine, excluding setup and assertions.
class AllocationCounter final {
public:
    AllocationCounter() { measured = &counts; }
    ~AllocationCounter() { measured = nullptr; }
    AllocationCounter(const AllocationCounter &) = delete;
    AllocationCounter &operator=(const AllocationCounter &) = delete;

    AllocationCounts counts;
};

// Supply the same button state as the live pointer adapter for the corresponding phase.
PointerEvent input(PointerEventPhase phase, double x, double y, double time)
{
    PointerEvent event;
    event.phase = phase;
    event.documentPosition = {x, y};
    event.time = time;
    event.button = phase == PointerEventPhase::Press ? PointerButton::Primary : PointerButton::None;
    event.primaryButtonDown = phase != PointerEventPhase::Release;
    return event;
}

// Isolate one operation so the counter excludes all checks and generated fixtures.
template <class Callback>
auto measure(Callback operation)
{
    AllocationCounter counter;
    auto result = operation();
    return std::pair{std::move(result), counter.counts};
}

// Low injected limits must reject without allocating work proportional to the attempted segment or footprint.
template <class Update>
bool rejectedWithinScratchBudget(const std::pair<Update, AllocationCounts> &measurement, bool active = true)
{
    constexpr std::size_t scratchLimit = 64 * 1024;
    const auto &[update, allocations] = measurement;
    largestRejection = std::max(largestRejection, allocations.bytes);
    if (engineAllocationsVisible && allocations.bytes > scratchLimit) {
        std::cerr << "Rejected event allocated " << allocations.bytes << " bytes\n";
        return false;
    }
    return !update.accepted && !update.committed && update.strokeActive == active
            && isEmpty(update.dirtyBounds) && update.code == EngineErrorCode::BudgetExceeded
            && !update.error.empty();
}
} // namespace

void *operator new(std::size_t size)
{
    if (measured) {
        ++measured->calls;
        measured->bytes += size;
        measured->largest = std::max(measured->largest, size);
    }
    if (void *memory = std::malloc(std::max<std::size_t>(size, 1))) {
        return memory;
    }
    throw std::bad_alloc();
}

void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept { std::free(memory); }

#define CHECK(condition) do { if (!(condition)) { std::cerr << __LINE__ << ": " #condition "\n"; return 1; } } while (false)

// Exercise the default and injected limits without publishing a bitmap or keeping an input path.
int checkEventBudgets()
{
    TestDirectory files{"paint-session-budget"};
    BitmapPaintSession bitmap(files.io);
    CHECK(bitmap.createFile(files.path / "source.png", 64, 64));
    BrushPreset fine;
    fine.size = 1;
    fine.stroke.spacingEnabled = false;
    CHECK(bitmap.setBrushPreset(fine));
    CHECK(bitmap.processPointerEvent(input(PointerEventPhase::Press, 8, 8, 1)).accepted);
    const auto before = bitmap.previewPixels().pixels;
    const auto rejected = bitmap.processPointerEvent(input(PointerEventPhase::Move, 50, 8, 2));
    CHECK(!rejected.accepted && rejected.code == EngineErrorCode::BudgetExceeded);
    CHECK(bitmap.strokeActive() && bitmap.previewPixels().pixels == before);
    const RasterEventLimits limits{.maxDabsPerEvent = 32, .maxProjectedPixels = 512};
    BitmapPaintSession boundedBitmap(files.io, limits);
    CHECK(boundedBitmap.openFile(files.path / "source.png") && boundedBitmap.setBrushPreset(fine));
    MaskPaintSession mask(limits);
    CHECK(mask.createMask(64, 64));
    CHECK(mask.setBrushOptions(LocalMaskBrushOptions{.sizePixels = .05}));
    const auto press = input(PointerEventPhase::Press, 0, 0, 1);
    CHECK(boundedBitmap.processPointerEvent(press).accepted && mask.processPointerEvent(press).accepted);
    const auto bitmapPreview = boundedBitmap.previewPixels().pixels;
    const auto maskPreview = mask.previewMask().coverage;
    const auto committedBitmap = boundedBitmap.file().pixels().pixels;
    const auto committedMask = mask.mask().coverage;
    for (const auto phase : {PointerEventPhase::Move, PointerEventPhase::Release}) {
        const auto far = input(phase, 1e9, 0, 100);
        CHECK(rejectedWithinScratchBudget(measure([&] { return boundedBitmap.processPointerEvent(far); })));
        CHECK(rejectedWithinScratchBudget(measure([&] { return mask.processPointerEvent(far); })));
        CHECK(boundedBitmap.previewPixels().pixels == bitmapPreview && mask.previewMask().coverage == maskPreview);
        CHECK(boundedBitmap.file().pixels().pixels == committedBitmap && mask.mask().coverage == committedMask);
        CHECK(!boundedBitmap.canUndo() && !mask.canUndo());
    }

    // Reject a new press on its pixel budget before discarding the already active stroke.
    auto wide = fine;
    wide.size = 48;
    CHECK(boundedBitmap.setBrushPreset(wide) && mask.setBrushOptions(LocalMaskBrushOptions{.sizePixels = 48}));
    const auto widePress = input(PointerEventPhase::Press, 32, 32, 90);
    CHECK(rejectedWithinScratchBudget(measure([&] { return boundedBitmap.processPointerEvent(widePress); })));
    CHECK(rejectedWithinScratchBudget(measure([&] { return mask.processPointerEvent(widePress); })));
    CHECK(boundedBitmap.previewPixels().pixels == bitmapPreview && mask.previewMask().coverage == maskPreview);
    const auto finish = input(PointerEventPhase::Release, 0, 0, 2);
    CHECK(boundedBitmap.processPointerEvent(finish).committed && mask.processPointerEvent(finish).committed);
    CHECK(boundedBitmap.file().pixels().pixels == bitmapPreview && mask.mask().coverage == maskPreview);

    // A large dab completely outside the target consumes no projection pixels.
    wide.size = 4096;
    CHECK(boundedBitmap.setBrushPreset(wide));
    const auto outside = boundedBitmap.processPointerEvent(input(PointerEventPhase::Press, 1e9, 1e9, 3));
    CHECK(outside.accepted && isEmpty(outside.dirtyBounds) && boundedBitmap.previewPixels().pixels == bitmapPreview);

    BitmapPaintSession timed(files.io, {.maxDabsPerEvent = 32, .maxProjectedPixels = 256});
    CHECK(timed.openFile(files.path / "source.png"));
    BrushPreset air;
    air.size = 8;
    air.stroke.airbrushEnabled = true;
    air.stroke.airbrushRate = 4;
    CHECK(timed.setBrushPreset(air));
    CHECK(timed.processPointerEvent(input(PointerEventPhase::Press, 32, 32, 1)).accepted);
    const auto beforeTime = timed.previewPixels().pixels;
    CHECK(rejectedWithinScratchBudget(measure([&] { return timed.advanceTime(2); })));
    CHECK(timed.lastError().find("pixel budget") != std::string::npos);
    CHECK(rejectedWithinScratchBudget(measure([&] { return timed.advanceTime(1e9); })));
    CHECK(timed.previewPixels().pixels == beforeTime && timed.advanceTime(1.25).accepted);

    BitmapPaintSession copies(files.io, {.maxDabsPerEvent = 2});
    CHECK(copies.openFile(files.path / "source.png"));
    auto scatter = fine;
    scatter.material.scatter.enabled = true;
    scatter.material.scatter.count = 3;
    CHECK(copies.setBrushPreset(scatter));
    CHECK(rejectedWithinScratchBudget(measure([&] { return copies.processPointerEvent(press); }), false));
    auto symmetry = fine;
    symmetry.stroke.symmetry.mode = BrushSymmetryMode::DualAxis;
    CHECK(copies.setBrushPreset(symmetry));
    const auto offAxis = input(PointerEventPhase::Press, 8, 7, 1);
    CHECK(rejectedWithinScratchBudget(measure([&] { return copies.processPointerEvent(offAxis); }), false));
    CHECK(copies.previewPixels().pixels == committedBitmap && !copies.canUndo());
    BrushDynamicsBinding suppressScatter;
    suppressScatter.target = BrushDynamicsTarget::ScatterCount;
    suppressScatter.combineMode = BrushDynamicsCombineMode::Replace;
    suppressScatter.curve = {.enabled = true, .min = 0, .center = 0, .max = 0};
    scatter.dynamics.bindings.push_back(suppressScatter);
    CHECK(copies.setBrushPreset(scatter) && copies.processPointerEvent(offAxis).accepted);
    const auto shortMove = input(PointerEventPhase::Move, 8.1, 7, 2);
    CHECK(rejectedWithinScratchBudget(measure([&] { return copies.processPointerEvent(shortMove); })));
    if (engineAllocationsVisible) {
        std::cout << "Bounded rejections: largest cumulative C++ allocation " << largestRejection << " bytes\n";
    }
    return 0;
}

namespace {
// Model the previous full-buffer reset on a small reference region. The independent projection path
// may produce off-canvas samples, which only the accumulator and preview writer discard.
void renderReferenceEvent(RasterLayer &preview, const RasterLayer &committed, StrokeCompositeBuffer &pending,
                          RasterDabStream &stream, InputStrokeBuilder &inputState, const BrushState &brush,
                          const PointerEvent &event, bool erase)
{
    const auto built = appendPointerEvent(inputState, event);
    if (!built.pointAvailable) return;
    const auto dabs = appendRasterDabs(stream, built.point, brush, built.strokeCompleted);
    auto samples = projectBrushDabs(BrushDabSpan{dabs}, brush.rasterizer, {}, brush.material);
    if (erase) {
        for (auto &sample : samples) sample.blendMode = RasterBlendMode::SourceOver;
    }
    accumulateStrokeSamples(pending, samples);
    copyStrokePreviewSamplePixels(preview, committed, pending, samples, erase);
}

// Compare the painted reference region bit for bit. A full check also verifies every untouched pixel.
bool matchesReference(const RasterLayer &canvas, const RasterLayer &reference, std::uint32_t background,
                      bool full = false)
{
    for (int y = 0; y < (full ? canvas.height : reference.height); ++y) {
        const auto row = canvas.pixels.begin() + static_cast<std::size_t>(y) * canvas.width;
        int start = 0;
        if (y < reference.height) {
            const auto expected = reference.pixels.begin() + static_cast<std::size_t>(y) * reference.width;
            if (!std::equal(expected, expected + reference.width, row)) return false;
            start = reference.width;
        }
        if (full && !std::all_of(row + start, row + canvas.width,
                                [background](auto pixel) { return pixel == background; })) return false;
    }
    return true;
}

// Check persistent stroke storage, full-reset parity, cancellation, and history on the same canvas.
int checkCanvasReuse(int canvasSize)
{
    constexpr auto background = 0x80446622U;
    constexpr auto brushColor = 0xAADD6633U;
    TestDirectory files{"paint-buffer-reuse"};
    BitmapPaintSession bitmap(files.io);
    // A lossless RLE fixture fits the existing encoded-byte limit even at the largest test dimensions.
    if (!bitmap.createFile(files.path / "source.tga", canvasSize, canvasSize, "tga", background)) {
        std::cerr << "Could not create the canvas fixture: " << bitmap.lastError() << '\n';
        return 1;
    }
    files.io->files.clear(); // Release the fixture's encoded bytes before measuring strokes.
    const auto *const previewStorage = bitmap.previewPixels().pixels.data();
    const auto canvasBytes = bitmap.previewPixels().pixels.size() * sizeof(std::uint32_t);
    BrushPreset preset;
    preset.size = 7;
    preset.flow = .32F;
    CHECK(bitmap.setBrushPreset(preset));
    bitmap.setBrushColor(brushColor);

    auto reference = makeRasterLayer(64, 64, background);
    auto beforeLastCommit = reference;
    std::uint32_t seed = 1;
    std::size_t largestAllocation = 0;
    const auto reusedStorage = [&](AllocationCounts allocations) {
        largestAllocation = std::max(largestAllocation, allocations.largest);
        return bitmap.previewPixels().pixels.data() == previewStorage
                && (!engineAllocationsVisible || allocations.largest < canvasBytes);
    };
    for (int stroke = 0; stroke < 100; ++stroke) {
        const bool cancel = stroke % 9 == 3;
        const bool erase = stroke % 3 == 0;
        const auto x = stroke % 5 == 0 ? 0 : 8 + (stroke % 4) * 6;
        const auto y = 8 + (stroke % 3) * 8;
        const double time = stroke * 3;
        const std::array events{
            input(PointerEventPhase::Press, x, y, time),
            input(PointerEventPhase::Move, x + 6, y + 2, time + 1),
            input(cancel ? PointerEventPhase::Cancel : PointerEventPhase::Release, x + 7, y + 2, time + 2),
        };
        bitmap.setEraserMode(erase);
        auto brush = resolveBrushPreset(preset, erase ? 0xFF000000U : brushColor, seed);
        CHECK(brush.brush);
        if (erase) brush.brush->rasterizer.blendMode = RasterBlendMode::DestinationOut;
        auto preview = reference;
        auto pending = makeStrokeCompositeBuffer(reference.width, reference.height);
        RasterDabStream stream;
        InputStrokeBuilder inputState;
        for (const auto &event : events) {
            renderReferenceEvent(preview, reference, pending, stream, inputState, *brush.brush, event, erase);
            const auto [update, allocated] = measure([&] { return bitmap.processPointerEvent(event); });
            CHECK(update.accepted && update.committed == (event.phase == PointerEventPhase::Release));
            if (event.phase == PointerEventPhase::Cancel) CHECK(!isEmpty(update.dirtyBounds));
            CHECK(reusedStorage(allocated));
            const auto &expected = event.phase == PointerEventPhase::Cancel ? reference : preview;
            CHECK(matchesReference(bitmap.previewPixels(), expected, background));
        }
        if (!cancel) {
            beforeLastCommit = std::move(reference);
            reference = std::move(preview);
            ++seed;
        }
    }
    CHECK(matchesReference(bitmap.previewPixels(), reference, background, true));
    CHECK(bitmap.previewPixels().pixels == bitmap.file().pixels().pixels);

    // Undo while another stroke is active, then restore that committed patch with redo.
    const auto [started, pressAllocations] = measure([&] {
        return bitmap.processPointerEvent(input(PointerEventPhase::Press, 52, 52, 301));
    });
    CHECK(started.accepted && reusedStorage(pressAllocations));
    const auto [undone, undoAllocations] = measure([&] { return bitmap.undo(); });
    CHECK(undone && reusedStorage(undoAllocations));
    CHECK(!bitmap.strokeActive() && matchesReference(bitmap.previewPixels(), beforeLastCommit, background));
    const auto [redone, redoAllocations] = measure([&] { return bitmap.redo(); });
    CHECK(redone && reusedStorage(redoAllocations));
    CHECK(matchesReference(bitmap.previewPixels(), reference, background, true));
    CHECK(bitmap.previewPixels().pixels == bitmap.file().pixels().pixels);
    if (engineAllocationsVisible) {
        std::cout << canvasSize << "x" << canvasSize << ": 100 strokes match full-reset pixels; largest allocation "
                  << largestAllocation << " bytes (canvas pixels " << canvasBytes << " bytes)\n";
    }
    return 0;
}
} // namespace

// Both the committed raster and its preview must reflect the last history operation.
bool matchesSolidBitmap(const BitmapPaintSession &bitmap, std::uint32_t color)
{
    const auto &pixels = bitmap.file().pixels().pixels;
    return std::ranges::all_of(pixels, [color](auto pixel) { return pixel == color; })
            && bitmap.previewPixels().pixels == pixels;
}

// Clear a mostly uniform canvas. The original stroke and alpha must survive a full Undo/Redo cycle.
int checkClearAllocation()
{
    constexpr auto background = 0xFF2468ACU;
    constexpr auto replacement = 0x80345678U;
    TestDirectory files{"paint-clear-budget"};
    BitmapPaintSession bitmap(files.io);
    CHECK(bitmap.createFile(files.path / "source.tga", 1024, 768, "tga", background));
    BrushPreset brush;
    brush.size = 5;
    CHECK(bitmap.setBrushPreset(brush));
    CHECK(bitmap.processPointerEvent(input(PointerEventPhase::Press, 9, 12, 1)).accepted);
    CHECK(bitmap.processPointerEvent(input(PointerEventPhase::Release, 11, 12, 2)).committed);
    CHECK(!matchesSolidBitmap(bitmap, background));

    const auto before = bitmap.file().pixels().pixels;
    const auto beforeBytes = bitmap.historyStatistics().retainedBytes;
    const auto rawBytes = before.size() * sizeof(std::uint32_t);
    const auto [cleared, allocations] = measure([&] { return bitmap.clear(replacement); });
    CHECK(cleared && matchesSolidBitmap(bitmap, replacement));
    const auto retained = bitmap.historyStatistics().retainedBytes;
    CHECK(retained > beforeBytes && retained - beforeBytes < rawBytes);
    CHECK(!engineAllocationsVisible || allocations.bytes < rawBytes);

    const auto [undone, undoAllocations] = measure([&] { return bitmap.undo(); });
    CHECK(undone && bitmap.file().pixels().pixels == before && bitmap.previewPixels().pixels == before);
    const auto [redone, redoAllocations] = measure([&] { return bitmap.redo(); });
    CHECK(redone && matchesSolidBitmap(bitmap, replacement));
    CHECK(bitmap.historyStatistics().retainedBytes == retained);
    CHECK(!engineAllocationsVisible || (undoAllocations.calls == 0 && redoAllocations.calls == 0));
    if (engineAllocationsVisible) {
        std::cout << "1024x768 clear: " << retained - beforeBytes << " retained bytes, "
                  << allocations.bytes << " bytes of C++ allocations (" << rawBytes
                  << " bytes for a full ARGB32 backup); Undo/Redo allocated zero bytes\n";
    }
    return 0;
}

// Verify independent byte and entry eviction, shared accounting, and exact cursor behavior.
int checkHistoryBudget()
{
    constexpr auto background = 0xFF000000U;
    constexpr auto first = 0xFFA01020U;
    constexpr auto second = 0xFF10B020U;
    constexpr auto third = 0xFF1020C0U;
    constexpr auto branch = 0x80123456U;
    TestDirectory files{"paint-history-budget"};
    const auto path = files.path / "source.tga";
    BitmapPaintSession reference(files.io);
    CHECK(reference.createFile(path, 16, 16, "tga", background) && reference.clear(first));
    const auto oneEntry = reference.historyStatistics().retainedBytes;
    CHECK(oneEntry > 0);

    BitmapPaintSession bitmap(files.io, {}, {.maximumBytes = 2 * oneEntry});
    CHECK(bitmap.openFile(path));
    for (const auto color : {first, second, third}) {
        CHECK(bitmap.clear(color) && matchesSolidBitmap(bitmap, color));
    }
    const auto hasHistory = [&](std::size_t undo, std::size_t redo) {
        const auto history = bitmap.historyStatistics();
        return history.undoEntries == undo && history.redoEntries == redo
                && history.retainedBytes == (undo + redo) * oneEntry;
    };
    CHECK(hasHistory(2, 0));
    CHECK(bitmap.undo() && hasHistory(1, 1) && matchesSolidBitmap(bitmap, second));
    CHECK(bitmap.undo() && hasHistory(0, 2) && matchesSolidBitmap(bitmap, first));
    CHECK(!bitmap.canUndo() && !bitmap.undo() && matchesSolidBitmap(bitmap, first));
    CHECK(bitmap.clear(first) && hasHistory(0, 2));
    CHECK(bitmap.redo() && hasHistory(1, 1) && matchesSolidBitmap(bitmap, second));
    CHECK(bitmap.redo() && hasHistory(2, 0) && matchesSolidBitmap(bitmap, third));
    CHECK(bitmap.undo() && bitmap.clear(branch) && hasHistory(2, 0));
    CHECK(!bitmap.canRedo() && !bitmap.redo() && matchesSolidBitmap(bitmap, branch));
    CHECK(bitmap.undo() && matchesSolidBitmap(bitmap, second));
    CHECK(bitmap.undo() && matchesSolidBitmap(bitmap, first));
    CHECK(bitmap.redo() && bitmap.redo() && matchesSolidBitmap(bitmap, branch));
    bitmap.closeFile();
    CHECK(hasHistory(0, 0));
    CHECK(bitmap.createFile(path, 16, 16, "tga", background));
    CHECK(bitmap.clear(first) && bitmap.clear(second) && bitmap.clear(third) && hasHistory(2, 0));

    BitmapPaintSession entryLimited(files.io, {}, {.maximumEntries = 1});
    CHECK(entryLimited.openFile(path) && entryLimited.clear(first) && entryLimited.clear(second));
    CHECK(entryLimited.historyStatistics().retainedBytes == oneEntry);
    CHECK(entryLimited.undo() && !entryLimited.canUndo() && matchesSolidBitmap(entryLimited, first));
    return 0;
}

// Preserve exact dense pixels when an entry fits and preserve all state when it exceeds the budget.
int checkOversizedEdit()
{
    constexpr auto replacement = 0x80345678U;
    TestDirectory files{"paint-oversized-history"};
    const auto path = files.path / "source.png";
    BitmapFile source(files.io);
    CHECK(source.create(path, 16, 24, "png"));
    auto noisy = makeRasterLayer(16, 24);
    for (std::size_t i = 0; i < noisy.pixels.size(); ++i) {
        noisy.pixels[i] = static_cast<std::uint32_t>(i + 1) * 0x9E3779B9U;
    }
    CHECK(source.replacePixels(noisy) && source.save());

    BitmapPaintSession dense(files.io);
    CHECK(dense.openFile(path) && dense.file().pixels().pixels == noisy.pixels);
    CHECK(dense.clear(replacement) && matchesSolidBitmap(dense, replacement));
    CHECK(dense.undo() && dense.file().pixels().pixels == noisy.pixels && dense.previewPixels().pixels == noisy.pixels);
    CHECK(dense.redo() && matchesSolidBitmap(dense, replacement));

    BitmapPaintSession bounded(files.io, {}, {.maximumBytes = 256});
    BrushPreset brush;
    brush.size = 3;
    CHECK(bounded.openFile(path) && bounded.setBrushPreset(brush));
    CHECK(bounded.processPointerEvent(input(PointerEventPhase::Press, 5, 8, 1)).accepted);
    CHECK(bounded.processPointerEvent(input(PointerEventPhase::Release, 5, 8, 2)).committed);
    const auto strokePixels = bounded.file().pixels().pixels;
    CHECK(strokePixels != noisy.pixels && bounded.undo());
    CHECK(bounded.processPointerEvent(input(PointerEventPhase::Press, 10, 11, 3)).accepted);
    const auto preview = bounded.previewPixels().pixels;
    CHECK(preview != noisy.pixels);
    const auto history = bounded.historyStatistics();
    CHECK(!bounded.clear(replacement) && bounded.lastError().find("history budget") != std::string::npos);
    CHECK(bounded.strokeActive() && bounded.previewPixels().pixels == preview);
    CHECK(bounded.file().pixels().pixels == noisy.pixels);
    CHECK(!bounded.canUndo() && bounded.canRedo() && bounded.historyStatistics().retainedBytes == history.retainedBytes);
    CHECK(bounded.redo() && bounded.file().pixels().pixels == strokePixels && bounded.previewPixels().pixels == strokePixels);

    BitmapPaintSession noHistory(files.io, {}, {.maximumBytes = 0});
    CHECK(noHistory.openFile(path) && noHistory.setBrushPreset(brush));
    CHECK(noHistory.processPointerEvent(input(PointerEventPhase::Press, 5, 8, 1)).accepted);
    CHECK(noHistory.previewPixels().pixels != noisy.pixels);
    const auto released = noHistory.processPointerEvent(input(PointerEventPhase::Release, 5, 8, 2));
    CHECK(released.code == EngineErrorCode::BudgetExceeded && !released.accepted && !released.committed);
    CHECK(!released.strokeActive && !noHistory.strokeActive() && !isEmpty(released.dirtyBounds));
    CHECK(noHistory.previewPixels().pixels == noisy.pixels && noHistory.file().pixels().pixels == noisy.pixels);
    CHECK(noHistory.historyStatistics().retainedBytes == 0);
    return 0;
}

int main(int argc, char **argv)
{
    // Prove that the counter sees actual library allocations before relying on its results.
    BrushState brush;
    RasterDabStream stream;
    const auto [dabs, allocations] = measure([&] { return appendRasterDabs(stream, StrokePoint{}, brush); });
    CHECK(dabs.size() == 1);
    engineAllocationsVisible = allocations.calls > 0 && allocations.bytes >= sizeof(BrushDab);
#if defined(_WIN32)
    // A Windows DLL may resolve allocation entry points directly in the CRT.
    if (!engineAllocationsVisible) std::cout << "Allocation instrumentation unavailable across the DLL boundary.\n";
#else
    CHECK(engineAllocationsVisible);
#endif
    if (const auto failure = checkClearAllocation(); failure != 0) return failure;
    if (const auto failure = checkHistoryBudget(); failure != 0) return failure;
    if (const auto failure = checkOversizedEdit(); failure != 0) return failure;
    if (const auto failure = checkEventBudgets(); failure != 0) return failure;
    const auto canvasSize = argc == 2 && std::string_view(argv[1]) == "8192" ? 8192 : 2048;
    return checkCanvasReuse(canvasSize);
}
