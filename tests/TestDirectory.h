#pragma once
#include <chrono>
#include <filesystem>
#include <string>

struct TestDirectory {
    std::filesystem::path path;
    explicit TestDirectory(const std::string &name)
        : path(std::filesystem::path{IIPAINTENGINE_BINARY_DIR} / "test-data"
               / (name + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
    { std::filesystem::create_directories(path); }
    ~TestDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
