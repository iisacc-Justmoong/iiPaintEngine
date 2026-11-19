#pragma once
#include <span>
#include <string>
#include <vector>
#include "Brush/LocalMaskBrush.h"
#include "Core/EngineError.h"
#include "Core/PixelMask.h"
#include "Input/InputStrokeBuilder.h"
#include "Render/DirtyRegion.h"
#include "Stroke/Rasterizer.h"

struct MaskPaintUpdate {
    bool accepted = false;
    bool committed = false;
    bool strokeActive = false;
    DevicePixelRect dirtyBounds{};
    std::string error;
    EngineErrorCode code = EngineErrorCode::None; // BudgetExceeded is reported without applying the event.
};

// Owns committed/preview/pending mask pixels, source guidance pixels, and pixel
// patches for history. No input point list or replayable stroke is retained.
class MaskPaintSession final {
public:
    MaskPaintSession() = default;
    // Retains this event policy when creating or replacing the mask.
    explicit MaskPaintSession(RasterEventLimits limits) noexcept : m_limits(limits) {}
    bool createMask(Types::Pixel width, Types::Pixel height, float clearCoverage = 0);
    bool replaceMask(const PixelMask &mask);
    bool hasMask() const;
    const PixelMask &mask() const;
    const PixelMask &previewMask() const;
    const LocalMaskBrushOptions &brushOptions() const;
    bool setBrushOptions(const LocalMaskBrushOptions &options);

    // Copy caller-supplied straight ARGB32 pixels with the current mask's shape.
    // Changing source guidance during a stroke is rejected; caller storage may
    // be released after this call. Fully transparent start pixels are rejected.
    bool setColorGuidance(std::span<const std::uint32_t> argb);
    bool clearColorGuidance();
    bool hasColorGuidance() const;

    bool strokeActive() const;
    MaskPaintUpdate processPointerEvent(const PointerEvent &event);
    bool commitStroke();
    void cancelStroke();
    bool clear(float coverage = 0);
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;
    const std::string &lastError() const;

private:
    // The range contract test observes writes to both private scratch planes.
    friend struct MaskPaintSessionTestAccess;

    struct PixelPatch { DevicePixelRect bounds; std::vector<float> coverage; };
    RasterEventLimits m_limits;
    PixelMask m_mask, m_preview, m_pending;
    std::vector<std::uint32_t> m_guidance;
    LocalMaskBrushOptions m_options, m_activeOptions;
    BrushState m_activeBrush;
    InputStrokeBuilder m_input;
    RasterDabStream m_stream;
    std::uint32_t m_referenceArgb = 0;
    DevicePixelRect m_dirty{};        // Changed preview pixels used for commit, restoration, and invalidation.
    DevicePixelRect m_pendingDirty{}; // Nonzero accumulation, including pixels whose coverage did not change.
    std::vector<PixelPatch> m_undo, m_redo;
    std::string m_error;

    // Clear the accumulated region. A successful commit already matches the preview.
    void resetPendingPixels(bool restorePreview) noexcept;
    PixelPatch capturePatch(DevicePixelRect bounds) const;
    // Apply only this history patch to committed and preview coverage.
    void applyPatch(const PixelPatch &patch);
    bool stepHistory(std::vector<PixelPatch> &from, std::vector<PixelPatch> &to);
    DevicePixelRect applyDabs(std::span<const BrushDab> dabs);
};
