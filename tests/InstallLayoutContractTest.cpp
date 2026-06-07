#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace {

int failures = 0;

std::string readFile(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void expectContains(const std::string &content, const std::string &token, const char *message)
{
    if (content.find(token) == std::string::npos) {
        std::cerr << message << '\n';
        ++failures;
    }
}

void expectExecutable(const std::string &path, const char *message)
{
    struct stat info {};
    if (stat(path.c_str(), &info) != 0 || (info.st_mode & S_IXUSR) == 0) {
        std::cerr << message << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    const std::string root = IIPAINTENGINE_SOURCE_DIR;
    const std::string installScriptPath = root + "/install.sh";
    const std::string installScript = readFile(installScriptPath);
    const std::string cmakeLists = readFile(root + "/CMakeLists.txt");
    const std::string configTemplate = readFile(root + "/cmake/iiPaintEngineConfig.cmake.in");
    const std::string readme = readFile(root + "/README.md");

    expectContains(installScript, "#!/usr/bin/env bash", "install.sh must be a bash script.");
    expectContains(installScript, "set -euo pipefail", "install.sh must fail on script errors.");
    expectExecutable(installScriptPath, "install.sh must be executable.");
    expectContains(installScript, "BUILD_DIR=\"${ROOT_DIR}/build\"",
                   "install.sh must use build/ as the build directory.");
    expectContains(installScript, "PREFIX=\"${IIPAINTENGINE_PREFIX:-${HOME}/.local/iiPaintEngine}\"",
                   "install.sh must default to ~/.local/iiPaintEngine.");
    expectContains(installScript, "IIPAINTENGINE_INSTALL_PLATFORMS",
                   "install.sh must allow constrained platform installs.");
    expectContains(installScript, "skip_or_fail \"android\" \"Android SDK root was not found\" || return 0",
                   "install.sh must continue default all-platform installs when an optional cross SDK is absent.");
    expectContains(installScript, "macos,ios,android,wasm",
                   "install.sh must default to all available Darwin platform packages.");
    expectContains(installScript, "linux,android,wasm",
                   "install.sh must support Linux host packages.");
    expectContains(installScript, "windows,android,wasm",
                   "install.sh must support Windows host packages.");
    expectContains(installScript, "MACOS_PLATFORM_PREFIX=\"${PREFIX}/platforms/macos\"",
                   "install.sh must publish a macOS platform package.");
    expectContains(installScript, "LINUX_PLATFORM_PREFIX=\"${PREFIX}/platforms/linux\"",
                   "install.sh must publish a Linux platform package.");
    expectContains(installScript, "WINDOWS_PLATFORM_PREFIX=\"${PREFIX}/platforms/windows\"",
                   "install.sh must publish a Windows platform package.");
    expectContains(installScript, "IOS_PREFIX=\"${PREFIX}/platforms/ios\"",
                   "install.sh must publish an iOS platform package.");
    expectContains(installScript, "ANDROID_PREFIX=\"${PREFIX}/platforms/android\"",
                   "install.sh must publish an Android platform package.");
    expectContains(installScript, "WASM_PREFIX=\"${PREFIX}/platforms/wasm\"",
                   "install.sh must publish a WASM platform package.");
    expectContains(installScript, "IIPAINTENGINE_BUILD_SHARED=ON",
                   "install.sh must force a shared iiPaintEngine library build.");
    expectContains(installScript, "-DLVRS_DIR=\"${lvrs_config_dir}\"",
                   "install.sh must pass the exact LVRS platform package config directory.");
    expectContains(installScript, "verify_dynamic_library",
                   "install.sh must verify that a platform dynamic library was installed.");
    expectContains(installScript, "CMAKE_HOME_DIRECTORY",
                   "install.sh must inspect stale CMake source directories.");
    expectContains(installScript, "CMAKE_CACHEFILE_DIR",
                   "install.sh must inspect stale CMake build directories.");
    expectContains(installScript, "--fresh",
                   "install.sh must configure with a fresh CMake cache.");

    expectContains(cmakeLists, "include(GNUInstallDirs)",
                   "CMakeLists.txt must use GNUInstallDirs for install destinations.");
    expectContains(cmakeLists, "include(CMakePackageConfigHelpers)",
                   "CMakeLists.txt must generate a CMake package config.");
    expectContains(cmakeLists, "IIPAINTENGINE_BUILD_SHARED",
                   "CMakeLists.txt must expose a shared/static library switch.");
    expectContains(cmakeLists, "add_library(iiPaintEngine ${IIPAINTENGINE_LIBRARY_TYPE}",
                   "CMakeLists.txt must build iiPaintEngine through the selected library type.");
    expectContains(cmakeLists, "qt_import_plugins(iiPaintEngine NO_DEFAULT)",
                   "CMakeLists.txt must keep app-only Qt platform plugins out of the engine library.");
    expectContains(cmakeLists, "target_link_options(iiPaintEngine PRIVATE \"--bind\")",
                   "CMakeLists.txt must link Emscripten embind support for Qt WASM dependencies.");
    expectContains(cmakeLists, "install(TARGETS iiPaintEngine",
                   "CMakeLists.txt must install the iiPaintEngine library target.");
    expectContains(cmakeLists, "install(FILES library.h iiPaintEngine",
                   "CMakeLists.txt must install the extensionless umbrella header.");
    expectContains(cmakeLists, "EXPORT iiPaintEngineTargets",
                   "CMakeLists.txt must export iiPaintEngineTargets.");
    expectContains(cmakeLists, "NAMESPACE iiPaintEngine::",
                   "CMakeLists.txt must install the iiPaintEngine:: imported target namespace.");
    expectContains(cmakeLists, "configure_package_config_file",
                   "CMakeLists.txt must generate iiPaintEngineConfig.cmake.");
    expectContains(cmakeLists, "DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine",
                   "CMakeLists.txt must install public headers under include/iiPaintEngine.");

    expectContains(configTemplate, "_iiPaintEngine_detect_target_platform",
                   "iiPaintEngineConfig.cmake.in must detect target platforms.");
    expectContains(configTemplate, "platforms/${_iiPaintEngineTargetPlatform}",
                   "iiPaintEngineConfig.cmake.in must redirect root package consumers to platform packages.");
    expectContains(configTemplate, "find_dependency(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)",
                   "iiPaintEngineConfig.cmake.in must declare Qt runtime dependencies.");
    expectContains(configTemplate, "iiPaintEngineTargets.cmake",
                   "iiPaintEngineConfig.cmake.in must include exported targets.");

    expectContains(readme, "./install.sh", "README.md must document the install script.");
    expectContains(readme, "~/.local/iiPaintEngine", "README.md must document the fixed install prefix.");
    expectContains(readme, "IIPAINTENGINE_INSTALL_PLATFORMS=macos ./install.sh",
                   "README.md must document constrained platform installs.");
    expectContains(readme, "find_package(iiPaintEngine CONFIG REQUIRED)",
                   "README.md must document CMake package loading.");
    expectContains(readme, "iiPaintEngine::iiPaintEngine",
                   "README.md must document the imported target.");
    expectContains(readme, "#include <iiPaintEngine>",
                   "README.md must document the single public umbrella include.");
    expectContains(readme, "iiPaintEnginePublicUmbrellaHeaderContract",
                   "README.md must document the umbrella header contract test.");

    return failures == 0 ? 0 : 1;
}
