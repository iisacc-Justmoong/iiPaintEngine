#include <iiPaintEngine>
#include <filesystem>
#include <type_traits>
int main()
{
    static_assert(std::is_same_v<decltype(std::declval<BitmapFile>().filePath()),const std::filesystem::path &>);
    static_assert(std::is_same_v<decltype(std::declval<BitmapFile>().pixels()),const RasterLayer &>);
    static_assert(std::is_same_v<decltype(std::declval<BitmapPaintSession>().file()),const BitmapFile &>);
    return std::filesystem::exists(std::filesystem::path{IIPAINTENGINE_SOURCE_DIR}/"src/Application/BitmapPaintSession.h") ? 0 : 1;
}
