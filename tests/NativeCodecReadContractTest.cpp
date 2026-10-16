#include <iiPaintEngine>
#include "TestDirectory.h"
#include <fstream>
#include <iostream>
#include <span>

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
namespace {
using Bytes = std::vector<unsigned char>;
void bigEndian(Bytes &bytes, std::uint32_t value, int count)
{
    for (int shift = (count - 1) * 8; shift >= 0; shift -= 8)
        bytes.push_back(static_cast<unsigned char>(value >> shift));
}
bool writeFixture(const std::filesystem::path &path, std::span<const unsigned char> bytes)
{
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}
Bytes flatPsd(std::uint32_t width = 1, std::uint32_t height = 1)
{
    Bytes bytes{'8','B','P','S',0,1,0,0,0,0,0,0,0,3};
    bigEndian(bytes,height,4); bigEndian(bytes,width,4);
    bigEndian(bytes,8,2); bigEndian(bytes,3,2);
    bytes.insert(bytes.end(),14,0); // Three empty sections and raw compression.
    bytes.insert(bytes.end(),{0x20,0x40,0x80});
    return bytes;
}
}
int main()
{
    TestDirectory dir{"native-codec-read"};
    BitmapFile file;
    // Generated 1x1 GIF: black/white palette, LZW clear + black + end.
    const Bytes gif{'G','I','F','8','9','a',1,0,1,0,0x80,0,0,0,0,0,255,255,255,
                    0x2c,0,0,0,0,1,0,1,0,0,2,2,0x44,0x01,0,0x3b};
    CHECK(writeFixture(dir.path / "one",gif));
    CHECK(file.open(dir.path / "one") && file.fileFormat() == "gif");
    CHECK(file.width() == 1 && file.height() == 1 && file.pixels().pixels[0] == 0xFF000000U);
    CHECK(file.isPixelWritable() && !file.canSaveInPlace());
    CHECK(file.setPixel({0,0},0x80204080U) && !file.save());
    CHECK(file.saveAs(dir.path / "gif-edit.png") && file.open(dir.path / "gif-edit.png"));
    CHECK(file.pixels().pixels[0] == 0x80204080U);

    CHECK(writeFixture(dir.path / "flat",flatPsd()));
    CHECK(file.open(dir.path / "flat") && file.fileFormat() == "psd");
    CHECK(file.width() == 1 && file.height() == 1 && file.pixels().pixels[0] == 0xFF204080U);
    CHECK(!file.canSaveInPlace());

    Bytes pic(92,0);
    pic[0]=0x53; pic[1]=0x80; pic[2]=0xF6; pic[3]=0x34;
    pic[88]='P'; pic[89]='I'; pic[90]='C'; pic[91]='T';
    bigEndian(pic,1,2); bigEndian(pic,1,2); pic.insert(pic.end(),8,0);
    pic.insert(pic.end(),{0,8,0,0xE0,0x20,0x40,0x80}); // One raw RGB packet.
    CHECK(writeFixture(dir.path / "picture",pic));
    CHECK(file.open(dir.path / "picture") && file.fileFormat() == "pic");
    CHECK(file.pixels().pixels[0] == 0xFF204080U && !file.canSaveInPlace());

    const Bytes pnm16{'P','6','\n','1',' ','1','\n','6','5','5','3','5','\n',0x20,0,0x40,0,0x80,0};
    CHECK(writeFixture(dir.path / "pnm16",pnm16));
    CHECK(file.open(dir.path / "pnm16") && file.fileFormat() == "pnm");
    CHECK(file.pixels().pixels[0] == 0xFF204080U);
    const Bytes gray{'P','5','\n','#','c','o','m','m','e','n','t','\n','2',' ','1','\n','1','5','\n',0,15};
    CHECK(writeFixture(dir.path / "gray.pgm",gray));
    CHECK(file.open(dir.path / "gray.pgm") && file.width() == 2 && file.height() == 1);
    CHECK(file.pixels().pixels[0] == 0xFF000000U && file.pixels().pixels[1] == 0xFFFFFFFFU);
    const auto validPixels = file.pixels().pixels;
    const auto validPath = file.filePath();
    CHECK(writeFixture(dir.path / "excessive-dimensions",flatPsd(16385,1)));
    CHECK(!file.open(dir.path / "excessive-dimensions"));
    CHECK(writeFixture(dir.path / "excessive-pixels",flatPsd(16384,16384)));
    CHECK(!file.open(dir.path / "excessive-pixels"));
    CHECK(file.filePath() == validPath && file.pixels().pixels == validPixels);

    for (const auto &format : {"png","bmp","tga"}) {
        CHECK(file.create(dir.path / (std::string("alpha.") + format),2,1,format,0x80204080U));
        CHECK(file.setPixel({1,0},0x00112233U) && file.save() && file.open(file.filePath()));
        CHECK(file.pixels().pixels[0] == 0x80204080U && file.pixels().pixels[1] == 0x00112233U);
    }
    const auto preservedPath = file.filePath();
    const auto preservedPixels = file.pixels().pixels;
    BitmapFileWriteOptions options;
    options.quality = 101;
    CHECK(!file.save(options) && file.filePath() == preservedPath && file.pixels().pixels == preservedPixels);
    options.quality = -1; options.backgroundArgb = 0xFF000000U;
    CHECK(file.saveAs(dir.path / "flatten.ppm","ppm",options));
    CHECK(file.open(dir.path / "flatten.ppm") && file.pixels().pixels[0] == 0xFF102040U);
    CHECK(file.pixels().pixels[1] == 0xFF000000U);
    CHECK(supportedBitmapReadFormats().size() == 9 && supportedBitmapWriteFormats().size() == 6);
}
