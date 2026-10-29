#pragma once
#include "BitmapFile.h"
#include <expected>
namespace bitmap_codec {
struct DecodedBitmap { RasterLayer pixels; std::string format; };
std::string normalizeFormat(std::string_view format);
bool validDimensions(int width, int height);
std::expected<DecodedBitmap, std::string> read(std::span<const unsigned char> bytes);
std::expected<BitmapFileIO::Bytes, std::string> write(std::string_view format, const RasterLayer &pixels,
                                                      const BitmapFileWriteOptions &options);
}
