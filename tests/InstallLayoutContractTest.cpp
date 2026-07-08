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
    const std::string installPowerShellPath = root + "/install.ps1";
    const std::string installScript = readFile(installScriptPath);
    const std::string installPowerShell = readFile(installPowerShellPath);
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
    expectContains(installScript, "IIPAINTENGINE_SKIP_TESTS",
                   "install.sh must allow installation completion when a local test executable is locked.");
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
    expectContains(installScript, "libiiPaintEngine.dll",
                   "install.sh must accept MinGW's prefixed Windows DLL name.");
    expectContains(installScript, "IIPAINTENGINE_HOST_TEST_TARGETS",
                   "install.sh must build install-relevant host test targets explicitly.");
    expectContains(installScript, "-E \"iiPaintEngineExampleDemoContract\"",
                   "install.sh must exclude the example app contract when the installed LVRS package lacks the app entrypoint library.");
    expectContains(installScript, "CMAKE_HOME_DIRECTORY",
                   "install.sh must inspect stale CMake source directories.");
    expectContains(installScript, "CMAKE_CACHEFILE_DIR",
                   "install.sh must inspect stale CMake build directories.");
    expectContains(installScript, "--fresh",
                   "install.sh must configure with a fresh CMake cache.");

    expectContains(installPowerShell, "Set-StrictMode -Version Latest",
                   "install.ps1 must enable strict PowerShell mode.");
    expectContains(installPowerShell, "$ErrorActionPreference = \"Stop\"",
                   "install.ps1 must fail on script errors.");
    expectContains(installPowerShell, "$BuildDir = Join-Path $RootDir \"build\"",
                   "install.ps1 must use build/ as the build directory.");
    expectContains(installPowerShell, "Join-Path $HOME \".local/iiPaintEngine\"",
                   "install.ps1 must default to ~/.local/iiPaintEngine.");
    expectContains(installPowerShell, "C:\\Qt\\6.8.3",
                   "install.ps1 must support the default Qt installer root on Windows.");
    expectContains(installPowerShell, "IIPAINTENGINE_INSTALL_PLATFORMS",
                   "install.ps1 must allow constrained platform installs.");
    expectContains(installPowerShell, "IIPAINTENGINE_SKIP_TESTS",
                   "install.ps1 must allow installation completion when a local test executable is locked.");
    expectContains(installPowerShell, "return \"windows,android,wasm\"",
                   "install.ps1 must default to Windows host and supported cross platform packages.");
    expectContains(installPowerShell, "$WindowsPlatformPrefix = Join-Path $Prefix \"platforms/windows\"",
                   "install.ps1 must publish a Windows platform package.");
    expectContains(installPowerShell, "$AndroidPrefix = Join-Path $Prefix \"platforms/android\"",
                   "install.ps1 must publish an Android platform package.");
    expectContains(installPowerShell, "$WasmPrefix = Join-Path $Prefix \"platforms/wasm\"",
                   "install.ps1 must publish a WASM platform package.");
    expectContains(installPowerShell, "-DIIPAINTENGINE_BUILD_SHARED=ON",
                   "install.ps1 must force a shared iiPaintEngine library build.");
    expectContains(installPowerShell, "-DLVRS_DIR=$lvrsConfigDir",
                   "install.ps1 must pass the exact LVRS platform package config directory.");
    expectContains(installPowerShell, "Verify-DynamicLibrary",
                   "install.ps1 must verify that a platform dynamic library was installed.");
    expectContains(installPowerShell, "libiiPaintEngine.dll",
                   "install.ps1 must accept MinGW's prefixed Windows DLL name.");
    expectContains(installPowerShell, "CMAKE_HOME_DIRECTORY",
                   "install.ps1 must inspect stale CMake source directories.");
    expectContains(installPowerShell, "CMAKE_CACHEFILE_DIR",
                   "install.ps1 must inspect stale CMake build directories.");
    expectContains(installPowerShell, "--fresh",
                   "install.ps1 must configure with a fresh CMake cache.");
    expectContains(installPowerShell, "Invoke-NativeCommand",
                   "install.ps1 must fail when native build tools fail.");
    expectContains(installPowerShell, "$IiPaintEngineHostTestTargets",
                   "install.ps1 must build install-relevant host test targets explicitly.");
    expectContains(installPowerShell, "\"iiPaintEngineExampleDemoContract\"",
                   "install.ps1 must exclude the example app contract when the installed LVRS package lacks the app entrypoint library.");
    expectContains(installPowerShell, "Run-HostTests -PlatformBuildDir $WindowsBuildDir",
                   "install.ps1 must run Windows host tests after building.");

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
    expectContains(readme, ".\\install.ps1", "README.md must document the Windows install script.");
    expectContains(readme, "~/.local/iiPaintEngine", "README.md must document the fixed install prefix.");
    expectContains(readme, "IIPAINTENGINE_INSTALL_PLATFORMS=macos ./install.sh",
                   "README.md must document constrained platform installs.");
    expectContains(readme, "$env:IIPAINTENGINE_INSTALL_PLATFORMS = \"windows\"",
                   "README.md must document constrained Windows platform installs.");
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
