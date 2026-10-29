#pragma once
#include <iiPaintEngine>
#include <iiFileProviderNative.h>
#include <exception>

namespace paint_file {
// iiFileProvider publishes complete bytes atomically and enqueues presentation
// only after publication. The engine itself never includes this adapter.
class FileProviderBitmapIO final : public BitmapFileIO {
public:
    std::expected<Bytes,std::string> read(const std::filesystem::path& path,std::size_t maximum) override {
        try {
            const auto bytes=iiFileProvider::native::File::read(path,maximum);
            return Bytes(bytes.begin(),bytes.end());
        } catch (const std::exception& error) { return std::unexpected(std::string(error.what())); }
    }
    std::expected<void,std::string> write(const std::filesystem::path& path,std::span<const unsigned char> bytes) override {
        try {
            const auto view=std::as_bytes(bytes);
            iiFileProvider::native::File::write(path,view);
            return {};
        } catch (const std::exception& error) { return std::unexpected(std::string(error.what())); }
    }
};
} // namespace paint_file
