#pragma once
#include "BitmapFile/BitmapFile.h"
#include "BitmapFile/BitmapPixelDelta.h"
#include "Brush/BrushResolve.h"
#include "Core/EngineError.h"
#include "History/RasterEditHistory.h"
#include "Input/InputStrokeBuilder.h"
#include "Render/DirtyRegion.h"
#include "Stroke/Rasterizer.h"
#include <cstddef>
#include <deque>
#include <expected>
#include <optional>

struct BitmapPaintUpdate {
    bool accepted = false;
    bool committed = false;
    bool strokeActive = false;
    DevicePixelRect dirtyBounds{};
    std::string error;
    // A history budget rejection ends the stroke and reports its restored preview bounds.
    EngineErrorCode code = EngineErrorCode::None;
};

// One retention policy covers Undo and Redo together. A zero limit rejects nonempty edits.
struct BitmapPaintHistoryLimits {
    std::size_t maximumBytes = RasterHistoryLimits{}.maximumBytes;
    std::size_t maximumEntries = RasterHistoryLimits{}.maximumEntries;
};

// The session owns pixels and scalar input state. It retains no input path or replay log.
class BitmapPaintSession final {
public:
    explicit BitmapPaintSession(std::shared_ptr<BitmapFileIO> io = {}) noexcept : m_file(std::move(io)) {}
    // Retains this event policy when opening or creating another bitmap.
    BitmapPaintSession(std::shared_ptr<BitmapFileIO> io, RasterEventLimits limits) noexcept
        : m_file(std::move(io)), m_limits(limits) {}
    // Retains both policies when opening or creating another bitmap.
    BitmapPaintSession(std::shared_ptr<BitmapFileIO> io, RasterEventLimits limits,
                       BitmapPaintHistoryLimits historyLimits) noexcept
        : m_file(std::move(io)), m_limits(limits), m_historyLimits(historyLimits) {}
    bool createFile(const std::filesystem::path &path, int width, int height,
                    std::string_view format = "png", std::uint32_t clearArgb = 0);
    bool openFile(const std::filesystem::path &path);
    bool save(const BitmapFileWriteOptions &options = {});
    bool saveAs(const std::filesystem::path &path, std::string_view format = {},
                const BitmapFileWriteOptions &options = {});
    void closeFile();
    const BitmapFile &file() const;
    const RasterLayer &previewPixels() const;
    const BrushPreset &brushPreset() const;
    bool setBrushPreset(const BrushPreset &preset, bool preserveProtectedTexture = true);
    bool setBrushPresetData(std::string_view payload, bool preserveProtectedTexture = true);
    std::string brushPresetData() const;
    void setBrushColor(std::uint32_t argb);
    void setEraserMode(bool enabled);
    bool strokeActive() const;
    BitmapPaintUpdate processPointerEvent(const PointerEvent &event);
    BitmapPaintUpdate advanceTime(double monotonicSeconds);
    void cancelStroke();
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;
    // Exact retained delta payload and entry bytes across the cursor, as used by the session budget.
    [[nodiscard]] RasterHistoryStatistics historyStatistics() const noexcept;
    // An undoable fill. A rejected history entry leaves the open bitmap and preview intact.
    bool clear(std::uint32_t argb = 0);
    const std::string &lastError() const;

private:
    BitmapFile m_file;
    RasterEventLimits m_limits;
    BitmapPaintHistoryLimits m_historyLimits;
    BrushPreset m_preset;
    std::uint32_t m_color = 0xFF000000U;
    bool m_eraser = false;
    std::uint64_t m_presetRevision = 0, m_activePresetRevision = 0;
    std::uint32_t m_nextSeed = 1;
    BrushState m_activeBrush;
    InputStrokeBuilder m_input;
    RasterDabStream m_stream;
    StrokeCompositeBuffer m_pending;
    RasterLayer m_preview;
    DevicePixelRect m_dirty{};
    // The cursor divides the applied prefix from the redo suffix. Queue storage is allocated on first edit.
    std::optional<std::deque<BitmapPixelDelta>> m_history;
    std::size_t m_historyCursor = 0;
    std::size_t m_historyBytes = 0;
    std::string m_error;
    // Check and record a reversible edit before retiring any previous history.
    EngineErrorCode applyEdit(std::expected<BitmapPixelDelta, EngineErrorCode> delta);
    // A zero entry allowance also prevents allocating a nonempty delta.
    [[nodiscard]] std::size_t historyEntryBudget() const noexcept;
    // Commit the preview, or restore the stroke's dirty bounds if it cannot be recorded.
    void finishStroke(BitmapPaintUpdate &result);
    // Allocate buffers for a newly opened bitmap. Normal strokes reuse their storage.
    void initializePixels();
    // Copy a clipped patch of committed pixels into the persistent preview after a rollback or history edit.
    void synchronizePreview(DevicePixelRect bounds) noexcept;
    // Zero the pending pixels touched by this stroke. A successful commit already matches the preview.
    void resetStrokePixels(bool restorePreview) noexcept;
    // Prepares a fully checked event. A replacement brush is consumed only on accepted stroke start.
    BitmapPaintUpdate appendPoint(const StrokePoint &point, bool finish, BrushState *startingBrush = nullptr);
    // Apply the same delta when moving the cursor in either direction.
    bool stepHistory(bool forward);
};
