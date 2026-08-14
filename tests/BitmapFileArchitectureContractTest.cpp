#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string readFile(const std::filesystem::path &path)
{
    std::ifstream stream(path);
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

bool containsForbiddenProductApi(const std::string &source)
{
    constexpr std::array<const char *, 6> forbidden{
            "Paint" "Canvas" "Item",
            "Canvas" "Adapter",
            "Canvas" "ApiConfig",
            "Canvas" "BrushConfig",
            "Iipe." "Canvas",
            "new" "Canvas",
    };
    for (const char *token : forbidden) {
        if (source.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

int main()
{
    const std::filesystem::path sourceRoot{IIPAINTENGINE_SOURCE_DIR};
    if (std::filesystem::exists(sourceRoot / (std::string{"Can"} + "vas"))) {
        return 1;
    }

    constexpr std::array<const char *, 5> productFiles{
            "CMakeLists.txt",
            "iiPaintEngine",
            "QtAdapter/IipeQmlTypes.cpp",
            "Example/Main.qml",
            "README.md",
    };
    for (const char *relativePath : productFiles) {
        if (containsForbiddenProductApi(readFile(sourceRoot / relativePath))) {
            return 2;
        }
    }

    constexpr std::array<const char *, 4> removedFiles{
            "QtAdapter/PaintCanvasItem.h",
            "QtAdapter/CanvasAdapter.h",
            "QtAdapter/CanvasApiConfig.h",
            "QtAdapter/CanvasBrushConfig.h",
    };
    for (const char *relativePath : removedFiles) {
        if (std::filesystem::exists(sourceRoot / relativePath)) {
            return 3;
        }
    }

    if (!std::filesystem::exists(sourceRoot / "BitmapFile/BitmapFile.h")
            || !std::filesystem::exists(sourceRoot / "QtAdapter/BitmapFileItem.h")) {
        return 4;
    }

    return 0;
}
