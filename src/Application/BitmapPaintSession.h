#pragma once
#include "BitmapFile/BitmapFile.h"
#include "Brush/BrushResolve.h"
#include "Input/InputStrokeBuilder.h"
#include "Render/DirtyRegion.h"
#include "Stroke/Rasterizer.h"

struct BitmapPaintUpdate {
    bool accepted = false;
    bool committed = false;
    bool strokeActive = false;
    DevicePixelRect dirtyBounds{};
    std::string error;
};

// The session owns pixels and scalar input state. It retains no input path or replay log.
class BitmapPaintSession final {
public:
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
    bool clear(std::uint32_t argb = 0);
    const std::string &lastError() const;

private:
    struct PixelPatch { DevicePixelRect bounds; std::vector<std::uint32_t> pixels; };
    BitmapFile m_file;
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
    std::vector<PixelPatch> m_undo, m_redo;
    std::string m_error;
    PixelPatch capturePatch(DevicePixelRect bounds) const;
    bool commitStroke();
    void initializePixels();
    BitmapPaintUpdate appendPoint(const StrokePoint &point, bool finish);
    bool stepHistory(std::vector<PixelPatch> &from, std::vector<PixelPatch> &to);
};
