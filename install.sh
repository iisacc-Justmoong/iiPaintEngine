#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
PREFIX="${IIPAINTENGINE_PREFIX:-${HOME}/.local/SDK/iiPaintEngine}"
QT_ROOT="${IIPAINTENGINE_QT_ROOT:-${HOME}/Qt/6.8.3}"
LVRS_PREFIX="${IIPAINTENGINE_LVRS_PREFIX:-${HOME}/.local/SDK/LVRS}"

MACOS_QT_PREFIX="${IIPAINTENGINE_MACOS_QT_PREFIX:-${QT_ROOT}/macos}"
LINUX_QT_PREFIX="${IIPAINTENGINE_LINUX_QT_PREFIX:-${QT_ROOT}/gcc_64}"
WINDOWS_QT_PREFIX="${IIPAINTENGINE_WINDOWS_QT_PREFIX:-${QT_ROOT}/mingw_64}"
IOS_QT_PREFIX="${IIPAINTENGINE_IOS_QT_PREFIX:-${QT_ROOT}/ios}"
ANDROID_QT_PREFIX="${IIPAINTENGINE_ANDROID_QT_PREFIX:-${QT_ROOT}/android_arm64_v8a}"
WASM_QT_PREFIX="${IIPAINTENGINE_WASM_QT_PREFIX:-}"

MACOS_BUILD_DIR="${BUILD_DIR}"
LINUX_BUILD_DIR="${BUILD_DIR}"
WINDOWS_BUILD_DIR="${BUILD_DIR}"
IOS_BUILD_DIR="${BUILD_DIR}/platforms/ios"
ANDROID_BUILD_DIR="${BUILD_DIR}/platforms/android"
WASM_BUILD_DIR="${BUILD_DIR}/platforms/wasm"

MACOS_PREFIX="${PREFIX}"
LINUX_PREFIX="${PREFIX}"
WINDOWS_PREFIX="${PREFIX}"
MACOS_PLATFORM_PREFIX="${PREFIX}/platforms/macos"
LINUX_PLATFORM_PREFIX="${PREFIX}/platforms/linux"
WINDOWS_PLATFORM_PREFIX="${PREFIX}/platforms/windows"
IOS_PREFIX="${PREFIX}/platforms/ios"
ANDROID_PREFIX="${PREFIX}/platforms/android"
WASM_PREFIX="${PREFIX}/platforms/wasm"

default_install_platforms() {
    case "$(uname -s)" in
        Darwin)
            echo "macos,ios,android,wasm"
            ;;
        Linux)
            echo "linux,android,wasm"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo "windows,android,wasm"
            ;;
        *)
            echo "$(uname -s | tr '[:upper:]' '[:lower:]')"
            ;;
    esac
}

AUTO_INSTALL_PLATFORMS=true
if [[ -n "${IIPAINTENGINE_INSTALL_PLATFORMS:-}" ]]; then
    AUTO_INSTALL_PLATFORMS=false
    INSTALL_PLATFORMS="${IIPAINTENGINE_INSTALL_PLATFORMS}"
else
    INSTALL_PLATFORMS="$(default_install_platforms)"
fi
SKIP_TESTS="${IIPAINTENGINE_SKIP_TESTS:-OFF}"

cmake_cache_value() {
    local cache_file="$1"
    local key="$2"

    awk -F= -v key="${key}" \
        '$1 == key || $1 ~ ("^" key ":") { print substr($0, index($0, "=") + 1); exit }' \
        "${cache_file}"
}

remove_stale_build_dir() {
    local build_dir="$1"
    local cache_file="${build_dir}/CMakeCache.txt"

    if [[ ! -f "${cache_file}" ]]; then
        return
    fi

    local cached_source
    local cached_build
    local stale_cache=false

    cached_source="$(cmake_cache_value "${cache_file}" "CMAKE_HOME_DIRECTORY")"
    cached_build="$(cmake_cache_value "${cache_file}" "CMAKE_CACHEFILE_DIR")"

    if [[ -n "${cached_source}" && "${cached_source}" != "${ROOT_DIR}" ]]; then
        stale_cache=true
    fi

    if [[ -n "${cached_build}" && "${cached_build}" != "${build_dir}" ]]; then
        stale_cache=true
    fi

    if [[ "${stale_cache}" == true ]]; then
        echo "Removing stale CMake build directory: ${build_dir}"
        echo "  cached source: ${cached_source:-unknown}"
        echo "  current source: ${ROOT_DIR}"
        # Finder can recreate directory metadata while the old tree is removed.
        if ! rm -rf "${build_dir}"; then
            rm -rf "${build_dir}"
        fi
    fi
}

join_prefixes() {
    local IFS=';'
    echo "$*"
}

skip_or_fail() {
    local platform="$1"
    local message="$2"

    if [[ "${AUTO_INSTALL_PLATFORMS}" == true ]]; then
        echo "Skipping iiPaintEngine ${platform} package: ${message}" >&2
        return 1
    fi

    echo "Cannot install iiPaintEngine ${platform} package: ${message}" >&2
    exit 1
}

require_dir_or_skip() {
    local platform="$1"
    local path="$2"
    local message="$3"

    if [[ -d "${path}" ]]; then
        return 0
    fi

    skip_or_fail "${platform}" "${message}: ${path}"
}

require_file_or_skip() {
    local platform="$1"
    local path="$2"
    local message="$3"

    if [[ -f "${path}" ]]; then
        return 0
    fi

    skip_or_fail "${platform}" "${message}: ${path}"
}

require_host_or_skip() {
    local platform="$1"
    local expected_uname="$2"
    local actual_uname

    actual_uname="$(uname -s)"
    if [[ "${actual_uname}" == "${expected_uname}" ]]; then
        return 0
    fi

    skip_or_fail "${platform}" "${platform} package requires ${expected_uname} host; current host is ${actual_uname}"
}

latest_child_dir() {
    local root="$1"

    if [[ ! -d "${root}" ]]; then
        return
    fi

    find "${root}" -mindepth 1 -maxdepth 1 -type d | sort | tail -n 1
}

resolve_existing_qt_prefix() {
    local candidate

    for candidate in "$@"; do
        if [[ -n "${candidate}" && -d "${candidate}/lib/cmake/Qt6" ]]; then
            echo "${candidate}"
            return
        fi
    done
}

resolve_wasm_qt_prefix() {
    local candidate

    if [[ -n "${WASM_QT_PREFIX}" && -d "${WASM_QT_PREFIX}/lib/cmake/Qt6" ]]; then
        echo "${WASM_QT_PREFIX}"
        return
    fi

    for candidate in "${QT_ROOT}/wasm_multithread" "${QT_ROOT}/wasm_singlethread" "${QT_ROOT}"/wasm_*; do
        if [[ -d "${candidate}/lib/cmake/Qt6" ]]; then
            echo "${candidate}"
            return
        fi
    done
}

detect_android_sdk_root() {
    local candidate

    for candidate in \
        "${ANDROID_SDK_ROOT:-}" \
        "${ANDROID_HOME:-}" \
        "${HOME}/Library/Android/sdk" \
        "/opt/homebrew/share/android-commandlinetools" \
        "/usr/local/share/android-commandlinetools" \
        "/opt/android/sdk"; do
        if [[ -n "${candidate}" && -d "${candidate}" ]]; then
            echo "${candidate}"
            return
        fi
    done
}

detect_android_ndk_root() {
    local sdk_root="$1"
    local candidate
    local sdk_ndk

    sdk_ndk="$(latest_child_dir "${sdk_root}/ndk")"
    for candidate in \
        "${ANDROID_NDK_ROOT:-}" \
        "${ANDROID_NDK_HOME:-}" \
        "${CMAKE_ANDROID_NDK:-}" \
        "${sdk_ndk}" \
        "/opt/homebrew/share/android-ndk" \
        "/usr/local/share/android-ndk" \
        "/opt/android/android-ndk-r26b"; do
        if [[ -n "${candidate}" && -d "${candidate}" ]]; then
            echo "${candidate}"
            return
        fi
    done
}

resolve_emscripten_toolchain_file() {
    local candidate
    local emsdk_root

    for candidate in \
        "${IIPAINTENGINE_EMSCRIPTEN_TOOLCHAIN_FILE:-}" \
        "${QT_CHAINLOAD_TOOLCHAIN_FILE:-}" \
        "${LVRS_BOOTSTRAP_EMSCRIPTEN_TOOLCHAIN_FILE:-}"; do
        if [[ -n "${candidate}" && -f "${candidate}" ]]; then
            echo "${candidate}"
            return
        fi
    done

    for emsdk_root in "${IIPAINTENGINE_EMSDK_ROOT:-}" "${EMSDK:-}" "${HOME}/emsdk" "${HOME}/.local/emsdk" "/opt/emsdk"; do
        candidate="${emsdk_root}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
        if [[ -n "${emsdk_root}" && -f "${candidate}" ]]; then
            echo "${candidate}"
            return
        fi
    done
}

verify_dynamic_library() {
    local platform="$1"
    local prefix="$2"
    local expected=""

    case "${platform}" in
        macos|ios)
            expected="${prefix}/lib/libiiPaintEngine.dylib"
            ;;
        linux|android)
            expected="${prefix}/lib/libiiPaintEngine.so"
            ;;
        windows)
            expected="$(find "${prefix}/bin" -maxdepth 1 -type f \( -name "iiPaintEngine.dll" -o -name "libiiPaintEngine.dll" \) 2>/dev/null | head -n 1)"
            ;;
        wasm)
            expected="${prefix}/lib/libiiPaintEngine.a"
            ;;
    esac

    if [[ -n "${expected}" && -f "${expected}" ]]; then
        echo "Verified iiPaintEngine ${platform} library: ${expected}"
        return
    fi

    echo "iiPaintEngine ${platform} library was not found under ${prefix}" >&2
    exit 1
}

IIPAINTENGINE_HOST_TEST_TARGETS=(
    iiPaintEngineTests
    iiPaintEngineCoreTests
    iiPaintEnginePipelineTests
    iiPaintEngineBitmapDocumentStructureTests
    iiPaintEngineDocumentSerializerContractTests
    iiPaintEngineAppDocumentApiContractTests
    iiPaintEngineInstallLayoutContractTests
    iiPaintEnginePublicUmbrellaHeaderContractTests
    iiPaintEnginePublicCxx17HeaderContractTests
    iiPaintEngineBitmapFileQmlApiTests
    iiPaintEngineBitmapFileApiContractTests
    iiPaintEngineBitmapFilePointerAlignmentTests
    iiPaintEngineBitmapFileTabletPressureContractTests
    iiPaintEngineBitmapFileLivePreviewRealtimeContractTests
    iiPaintEnginePointerStrokeFlowTests
    iiPaintEnginePressureInputContractTests
    iiPaintEngineTabletInputSurfaceTests
    iiPaintEngineRasterPaintingModelTests
    iiPaintEngineStrokePhysicalContractTests
    iiPaintEngineBrushDynamicsMappingTests
    iiPaintEngineBrushDynamicsResponseCurveContractTests
    iiPaintEngineBrushFeatureToggleContractTests
    iiPaintEngineBrushExpressionContractTests
    iiPaintEngineBrushTextureLayerContractTests
    iiPaintEngineWetBrushSimulationContractTests
    iiPaintEngineHistoryUndoRedoContractTests
    iiPaintEngineEditingToolPipelineContractTests
    iiPaintEngineStrokeCompositingTests
    iiPaintEngineHighFidelityPointerInputContractTests
    iiPaintEngineLayerCompositingContractTests
    iiPaintEngineRendererProjectionContractTests
    iiPaintEngineRenderCacheBackendContractTests
    iiPaintEngineColorManagementContractTests
    iiPaintEngineBrushMaskSamplingTests
    iiPaintEngineCoordinateDirtyRegionTests
    iiPaintEngineBitmapFileEventLoopLoadContractTests
    iiPaintEngineBitmapOnlyArchitectureContractTests
    iiPaintEngineBitmapFileCompatibilityContractTests
    iiPaintEngineBitmapFileArchitectureContractTests
)

build_host_tests() {
    local build_dir="$1"

    if [[ "${SKIP_TESTS}" == "ON" || "${SKIP_TESTS}" == "1" || "${SKIP_TESTS}" == "true" ]]; then
        echo "Skipping iiPaintEngine host test build because IIPAINTENGINE_SKIP_TESTS=${SKIP_TESTS}"
        return
    fi

    cmake --build "${build_dir}" --config Release --target "${IIPAINTENGINE_HOST_TEST_TARGETS[@]}"
}

run_host_tests() {
    local build_dir="$1"

    if [[ "${SKIP_TESTS}" == "ON" || "${SKIP_TESTS}" == "1" || "${SKIP_TESTS}" == "true" ]]; then
        echo "Skipping iiPaintEngine host tests because IIPAINTENGINE_SKIP_TESTS=${SKIP_TESTS}"
        return
    fi

    ctest --test-dir "${build_dir}" --output-on-failure -E "iiPaintEngineExampleDemoContract"
}

configure_build_install() {
    local platform="$1"
    local build_dir="$2"
    local install_prefix="$3"
    local qt_prefix="$4"
    local lvrs_platform_prefix="$5"
    shift 5
    local cmake_args=()
    local cmake_prefix_path
    local lvrs_config_dir="${lvrs_platform_prefix}"

    if [[ -f "${lvrs_platform_prefix}/lib/cmake/LVRS/LVRSConfig.cmake" ]]; then
        lvrs_config_dir="${lvrs_platform_prefix}/lib/cmake/LVRS"
    fi

    remove_stale_build_dir "${build_dir}"
    cmake_prefix_path="$(join_prefixes "${qt_prefix}" "${lvrs_platform_prefix}" "${LVRS_PREFIX}")"

    echo "Configuring iiPaintEngine ${platform} install prefix: ${install_prefix}"
    cmake_args=(
        -S "${ROOT_DIR}" \
        -B "${build_dir}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${install_prefix}" \
        -DCMAKE_PREFIX_PATH="${cmake_prefix_path}" \
        -DLVRS_DIR="${lvrs_config_dir}" \
        -DIIPAINTENGINE_BUILD_SHARED=ON
    )

    if (($# > 0)); then
        cmake_args+=("$@")
    fi

    cmake --fresh "${cmake_args[@]}"

    echo "Building iiPaintEngine ${platform} library in ${build_dir}"
    cmake --build "${build_dir}" --config Release --target iiPaintEngine

    echo "Installing iiPaintEngine ${platform} package into ${install_prefix}"
    if [[ "${platform}" == "wasm" ]]; then
        rm -f "${install_prefix}/lib/libiiPaintEngine.so"
    fi
    cmake --install "${build_dir}" --prefix "${install_prefix}" --config Release
    verify_dynamic_library "${platform}" "${install_prefix}"
}

install_host_platform() {
    local platform="$1"
    local expected_uname="$2"
    local build_dir="$3"
    local install_prefix="$4"
    local platform_prefix="$5"
    local qt_prefix="$6"
    local lvrs_platform_prefix="${LVRS_PREFIX}/platforms/${platform}"

    require_host_or_skip "${platform}" "${expected_uname}" || return 0
    require_dir_or_skip "${platform}" "${qt_prefix}/lib/cmake/Qt6" "Qt ${platform} package is required" || return 0
    require_file_or_skip "${platform}" "${lvrs_platform_prefix}/LVRSConfig.cmake" "LVRS ${platform} package is required" || return 0

    configure_build_install "${platform}" "${build_dir}" "${install_prefix}" "${qt_prefix}" "${lvrs_platform_prefix}"

    if [[ "${platform_prefix}" != "${install_prefix}" ]]; then
        echo "Installing iiPaintEngine ${platform} platform mirror into ${platform_prefix}"
        cmake --install "${build_dir}" --prefix "${platform_prefix}" --config Release
        verify_dynamic_library "${platform}" "${platform_prefix}"
    fi

    cmake -E copy_if_different \
        "${build_dir}/iiPaintEngineConfigVersionRoot.cmake" \
        "${install_prefix}/lib/cmake/iiPaintEngine/iiPaintEngineConfigVersion.cmake"

    echo "Building iiPaintEngine ${platform} tests in ${build_dir}"
    build_host_tests "${build_dir}"

    echo "Running iiPaintEngine ${platform} tests"
    run_host_tests "${build_dir}"
}

install_macos() {
    install_host_platform "macos" "Darwin" "${MACOS_BUILD_DIR}" "${MACOS_PREFIX}" "${MACOS_PLATFORM_PREFIX}" "${MACOS_QT_PREFIX}"
}

install_linux() {
    install_host_platform "linux" "Linux" "${LINUX_BUILD_DIR}" "${LINUX_PREFIX}" "${LINUX_PLATFORM_PREFIX}" "${LINUX_QT_PREFIX}"
}

install_windows() {
    local actual_uname
    actual_uname="$(uname -s)"
    case "${actual_uname}" in
        MINGW*|MSYS*|CYGWIN*) ;;
        *) skip_or_fail "windows" "windows package requires a Windows/MSYS host; current host is ${actual_uname}" || return 0 ;;
    esac

    require_dir_or_skip "windows" "${WINDOWS_QT_PREFIX}/lib/cmake/Qt6" "Qt Windows package is required" || return 0
    require_file_or_skip "windows" "${LVRS_PREFIX}/platforms/windows/LVRSConfig.cmake" "LVRS Windows package is required" || return 0
    configure_build_install "windows" "${WINDOWS_BUILD_DIR}" "${WINDOWS_PREFIX}" "${WINDOWS_QT_PREFIX}" "${LVRS_PREFIX}/platforms/windows"
    cmake --install "${WINDOWS_BUILD_DIR}" --prefix "${WINDOWS_PLATFORM_PREFIX}" --config Release
    verify_dynamic_library "windows" "${WINDOWS_PLATFORM_PREFIX}"
    cmake -E copy_if_different \
        "${WINDOWS_BUILD_DIR}/iiPaintEngineConfigVersionRoot.cmake" \
        "${WINDOWS_PREFIX}/lib/cmake/iiPaintEngine/iiPaintEngineConfigVersion.cmake"
    build_host_tests "${WINDOWS_BUILD_DIR}"
    run_host_tests "${WINDOWS_BUILD_DIR}"
}

install_ios() {
    local ios_toolchain_file="${IOS_QT_PREFIX}/lib/cmake/Qt6/qt.toolchain.cmake"
    local lvrs_ios_prefix="${LVRS_PREFIX}/platforms/ios"

    require_host_or_skip "ios" "Darwin" || return 0
    require_dir_or_skip "ios" "${IOS_QT_PREFIX}/lib/cmake/Qt6" "Qt iOS package is required" || return 0
    require_file_or_skip "ios" "${ios_toolchain_file}" "Qt iOS toolchain file is required" || return 0
    require_file_or_skip "ios" "${lvrs_ios_prefix}/LVRSConfig.cmake" "LVRS iOS package is required" || return 0

    configure_build_install "ios" "${IOS_BUILD_DIR}" "${IOS_PREFIX}" "${IOS_QT_PREFIX}" "${lvrs_ios_prefix}" \
        -G Xcode \
        -DCMAKE_TOOLCHAIN_FILE="${ios_toolchain_file}" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_SYSROOT=iphoneos \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
        -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO
}

install_android() {
    local android_sdk_root
    local android_ndk_root
    local android_ndk_toolchain
    local android_toolchain_file="${ANDROID_QT_PREFIX}/lib/cmake/Qt6/qt.toolchain.cmake"
    local lvrs_android_prefix="${LVRS_PREFIX}/platforms/android"

    require_dir_or_skip "android" "${ANDROID_QT_PREFIX}/lib/cmake/Qt6" "Qt Android package is required" || return 0
    require_file_or_skip "android" "${android_toolchain_file}" "Qt Android toolchain file is required" || return 0
    require_file_or_skip "android" "${lvrs_android_prefix}/LVRSConfig.cmake" "LVRS Android package is required" || return 0

    android_sdk_root="$(detect_android_sdk_root)"
    if [[ -z "${android_sdk_root}" ]]; then
        skip_or_fail "android" "Android SDK root was not found" || return 0
    fi

    android_ndk_root="$(detect_android_ndk_root "${android_sdk_root}")"
    if [[ -z "${android_ndk_root}" ]]; then
        skip_or_fail "android" "Android NDK root was not found under ${android_sdk_root}" || return 0
    fi
    android_ndk_toolchain="${android_ndk_root}/build/cmake/android.toolchain.cmake"
    require_file_or_skip "android" "${android_ndk_toolchain}" "Android NDK CMake toolchain file is required" || return 0

    configure_build_install "android" "${ANDROID_BUILD_DIR}" "${ANDROID_PREFIX}" "${ANDROID_QT_PREFIX}" "${lvrs_android_prefix}" \
        -DCMAKE_TOOLCHAIN_FILE="${android_toolchain_file}" \
        -DQT_CHAINLOAD_TOOLCHAIN_FILE="${android_ndk_root}/build/cmake/android.toolchain.cmake" \
        -DCMAKE_SYSTEM_NAME=Android \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM=android-23 \
        -DANDROID_SDK_ROOT="${android_sdk_root}" \
        -DANDROID_NDK="${android_ndk_root}" \
        -DANDROID_NDK_ROOT="${android_ndk_root}" \
        -DCMAKE_ANDROID_NDK="${android_ndk_root}"
}

install_wasm() {
    local wasm_qt_prefix
    local emscripten_toolchain_file
    local lvrs_wasm_prefix="${LVRS_PREFIX}/platforms/wasm"

    wasm_qt_prefix="$(resolve_wasm_qt_prefix)"
    if [[ -z "${wasm_qt_prefix}" ]]; then
        skip_or_fail "wasm" "Qt WASM package was not found" || return 0
    fi
    require_dir_or_skip "wasm" "${wasm_qt_prefix}/lib/cmake/Qt6" "Qt WASM package is required" || return 0
    require_file_or_skip "wasm" "${lvrs_wasm_prefix}/LVRSConfig.cmake" "LVRS WASM package is required" || return 0

    emscripten_toolchain_file="$(resolve_emscripten_toolchain_file)"
    if [[ -z "${emscripten_toolchain_file}" ]]; then
        skip_or_fail "wasm" "Emscripten toolchain file was not found" || return 0
    fi

    configure_build_install "wasm" "${WASM_BUILD_DIR}" "${WASM_PREFIX}" "${wasm_qt_prefix}" "${lvrs_wasm_prefix}" \
        -DCMAKE_TOOLCHAIN_FILE="${wasm_qt_prefix}/lib/cmake/Qt6/qt.toolchain.cmake" \
        -DQT_CHAINLOAD_TOOLCHAIN_FILE="${emscripten_toolchain_file}" \
        -DCMAKE_SYSTEM_NAME=Emscripten
}

run_platform_install() {
    local platform="$1"

    case "${platform}" in
        macos)
            install_macos
            ;;
        linux)
            install_linux
            ;;
        windows)
            install_windows
            ;;
        ios)
            install_ios
            ;;
        android)
            install_android
            ;;
        wasm)
            install_wasm
            ;;
        "")
            ;;
        *)
            echo "Unsupported iiPaintEngine install platform: ${platform}" >&2
            echo "Supported platforms: macos, linux, windows, ios, android, wasm" >&2
            exit 1
            ;;
    esac
}

IFS=',;' read -ra requested_platforms <<< "${INSTALL_PLATFORMS}"

echo "Installing iiPaintEngine for platforms: ${INSTALL_PLATFORMS}"
for platform in "${requested_platforms[@]}"; do
    platform="$(echo "${platform}" | xargs)"
    run_platform_install "${platform}"
done

echo "iiPaintEngine installed for platforms: ${INSTALL_PLATFORMS}."
