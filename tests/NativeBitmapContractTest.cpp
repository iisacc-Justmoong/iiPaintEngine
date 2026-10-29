#include <iiPaintEngine>
#include <algorithm>
#include <iostream>
#include "TestDirectory.h"

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
int main()
{
    static_assert(__cplusplus > 202002L);
    TestDirectory dir{"native-bitmap"};
    BitmapFile file(dir.io);
    const auto png = dir.path / std::filesystem::path{u8"한글 이미지"};
    CHECK(file.create(png, 7, 5, "png", 0x80204080U));
    CHECK(file.filePath() == png && !file.isModified());
    BitmapFile reopened(dir.io);
    CHECK(reopened.open(png) && reopened.fileFormat() == "png");
    CHECK(reopened.pixels().pixels == file.pixels().pixels);
    auto wrongExtension = dir.path / "bitmap.svg";
    dir.io->files[wrongExtension] = dir.io->files.at(png);
    CHECK(reopened.open(wrongExtension) && reopened.fileFormat() == "png");
    CHECK(reopened.setPixel({2, 3}, 0xFF44AA66U) && reopened.save());
    CHECK(file.open(wrongExtension) && rasterLayerPixelAt(file.pixels(), {2, 3}) == 0xFF44AA66U);
    const auto before = file.pixels().pixels;
    const std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\"/>";
    dir.io->files[dir.path / "vector.png"] = BitmapFileIO::Bytes(svg.begin(),svg.end());
    CHECK(!file.open(dir.path / "vector.png") && file.pixels().pixels == before);
    const std::string broken = "\x89PNG\r\n\x1a\n";
    dir.io->files[dir.path / "broken.png"] = BitmapFileIO::Bytes(broken.begin(),broken.end());
    CHECK(!file.open(dir.path / "broken.png") && !file.lastError().empty());
    CHECK(file.pixels().pixels == before);
    CHECK(!file.create(dir.path / "bad", 0, 4, "png"));
    CHECK(!file.create(dir.path / "bad", 16384, 16384, "png"));
    CHECK(file.pixels().pixels == before);
    dir.io->rejected.insert(dir.path / "missing" / "file.png");
    CHECK(!file.saveAs(dir.path / "missing" / "file.png", "png"));
    CHECK(file.filePath() == wrongExtension);
    for (const auto &format : supportedBitmapWriteFormats()) {
        BitmapFile roundtrip(dir.io);
        const auto output = dir.path / ("roundtrip." + format);
        CHECK(roundtrip.create(output, 8, 8, format, 0xFF44AA66U));
        CHECK(roundtrip.setPixel({1, 0}, 0xFF114422U) && roundtrip.save());
        CHECK(roundtrip.open(output) && roundtrip.width() == 8 && roundtrip.height() == 8);
        if (format == "png" || format == "bmp" || format == "tga" || format == "pnm")
            CHECK(rasterLayerPixelAt(roundtrip.pixels(), {1,0}) == 0xFF114422U);
    }
    CHECK(std::ranges::find(supportedBitmapReadFormats(), "svg") == supportedBitmapReadFormats().end());
    const auto bytes = dir.io->files.at(png);
    const auto oldPath = file.filePath();
    CHECK(!file.saveAs(dir.path / "file.pdf", "pdf"));
    CHECK(file.filePath() == oldPath);
    CHECK(dir.io->files.at(png) == bytes);
    return 0;
}
