#pragma once
#include "BitmapFile/BitmapFile.h"
#include <map>
#include <memory>
#include <set>
#include <string>

// Pure in-memory fixtures exercise the same file I/O contract without creating
// temporary files or relying on a second SDK in the engine's test suite.
struct MemoryBitmapFileIO final : BitmapFileIO {
    std::map<std::filesystem::path,Bytes> files;
    std::set<std::filesystem::path> rejected;
    std::expected<Bytes,std::string> read(const std::filesystem::path &path,std::size_t maximum) override {
        const auto found=files.find(path);
        if (found==files.end()) return std::unexpected("File does not exist.");
        if (found->second.size()>maximum) return std::unexpected("File exceeds byte limit.");
        return found->second;
    }
    std::expected<void,std::string> write(const std::filesystem::path &path,std::span<const unsigned char> bytes) override {
        if (rejected.contains(path)) return std::unexpected("File publication rejected.");
        files[path]=Bytes(bytes.begin(),bytes.end());
        return {};
    }
};
struct TestDirectory {
    std::filesystem::path path;
    std::shared_ptr<MemoryBitmapFileIO> io=std::make_shared<MemoryBitmapFileIO>();
    explicit TestDirectory(const std::string &name) : path(std::filesystem::path("memory") / name) {}
};
