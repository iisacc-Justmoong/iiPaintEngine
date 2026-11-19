#pragma once
#include "BitmapFile.h"
#include <array>
#include <expected>
namespace bitmap_codec {
struct DecodedBitmap { RasterLayer pixels; std::string format; };
// 8비트 채널을 HDR의 선형 값으로 변환하는 현재 반올림 모드의 표이다.
std::array<float, 256> hdrLinearTable();
std::string normalizeFormat(std::string_view format);
bool validDimensions(int width, int height);
std::expected<DecodedBitmap, std::string> read(std::span<const unsigned char> bytes);
std::expected<BitmapFileIO::Bytes, std::string> write(std::string_view format, const RasterLayer &pixels,
                                                      const BitmapFileWriteOptions &options);
}
