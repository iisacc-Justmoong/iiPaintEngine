#include <iiPaintEngine>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include "TestDirectory.h"

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
int main()
{
    static_assert(__cplusplus > 202002L);
    TestDirectory dir{"native-bitmap"};
    BitmapFile file;
    const auto png = dir.path / std::filesystem::path{u8"한글 이미지"};
    CHECK(file.create(png, 7, 5, "png", 0x80204080U));
    CHECK(file.filePath() == png && !file.isModified());
    BitmapFile reopened;
    CHECK(reopened.open(png) && reopened.fileFormat() == "png");
    CHECK(reopened.pixels().pixels == file.pixels().pixels);
    auto wrongExtension = dir.path / "bitmap.svg";
    std::filesystem::copy_file(png, wrongExtension);
    CHECK(reopened.open(wrongExtension) && reopened.fileFormat() == "png");
    CHECK(reopened.setPixel({2, 3}, 0xFF44AA66U) && reopened.save());
    CHECK(file.open(wrongExtension) && rasterLayerPixelAt(file.pixels(), {2, 3}) == 0xFF44AA66U);
    const auto before = file.pixels().pixels;
    std::ofstream{dir.path / "vector.png"} << "<svg xmlns=\"http://www.w3.org/2000/svg\"/>";
    CHECK(!file.open(dir.path / "vector.png") && file.pixels().pixels == before);
    std::ofstream{dir.path / "broken.png", std::ios::binary} << "\x89PNG\r\n\x1a\n";
    CHECK(!file.open(dir.path / "broken.png") && !file.lastError().empty());
    CHECK(file.pixels().pixels == before);
    CHECK(!file.create(dir.path / "bad", 0, 4, "png"));
    CHECK(!file.create(dir.path / "bad", 16384, 16384, "png"));
    CHECK(file.pixels().pixels == before);
    CHECK(!file.saveAs(dir.path / "missing" / "file.png", "png"));
    CHECK(file.filePath() == wrongExtension);
    for (const auto &format : supportedBitmapWriteFormats()) {
        BitmapFile roundtrip;
        const auto output = dir.path / ("roundtrip." + format);
        CHECK(roundtrip.create(output, 8, 8, format, 0xFF44AA66U));
        CHECK(roundtrip.setPixel({1, 0}, 0xFF114422U) && roundtrip.save());
        CHECK(roundtrip.open(output) && roundtrip.width() == 8 && roundtrip.height() == 8);
        if (format == "png" || format == "bmp" || format == "tga" || format == "pnm")
            CHECK(rasterLayerPixelAt(roundtrip.pixels(), {1,0}) == 0xFF114422U);
    }
    CHECK(std::ranges::find(supportedBitmapReadFormats(), "svg") == supportedBitmapReadFormats().end());
    const auto bytes = [&] { std::ifstream in(png, std::ios::binary); return std::vector<char>{std::istreambuf_iterator<char>{in}, {}}; }();
    const auto oldPath = file.filePath();
    CHECK(!file.saveAs(dir.path / "file.pdf", "pdf"));
    CHECK(file.filePath() == oldPath);
    std::ifstream unchanged(png, std::ios::binary);
    CHECK((std::vector<char>{std::istreambuf_iterator<char>{unchanged}, {}} == bytes));
    return 0;
}
