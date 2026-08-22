if (NOT DEFINED IIPAINTENGINE_BINARY_DIR)
    message(FATAL_ERROR "IIPAINTENGINE_BINARY_DIR is required")
endif ()

set(test_root "${IIPAINTENGINE_BINARY_DIR}/install-upgrade-contract")
set(test_prefix "${test_root}/prefix")
set(obsolete_header "${test_prefix}/include/iiPaintEngine/Stroke/StrokeCommand.h")

file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_prefix}/include/iiPaintEngine/Stroke")
file(WRITE "${obsolete_header}" "#pragma once\nstruct BrushState {};\n")

set(install_command
    "${CMAKE_COMMAND}"
    --install "${IIPAINTENGINE_BINARY_DIR}"
    --prefix "${test_prefix}")
if (DEFINED IIPAINTENGINE_INSTALL_CONFIG AND NOT IIPAINTENGINE_INSTALL_CONFIG STREQUAL "")
    list(APPEND install_command --config "${IIPAINTENGINE_INSTALL_CONFIG}")
endif ()

execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)

if (NOT install_result EQUAL 0)
    message(FATAL_ERROR
        "The upgrade install failed with exit code ${install_result}.\n"
        "stdout:\n${install_output}\n"
        "stderr:\n${install_error}")
endif ()

if (EXISTS "${obsolete_header}")
    message(FATAL_ERROR
        "The upgrade install retained an obsolete public header: ${obsolete_header}")
endif ()

if (NOT EXISTS "${test_prefix}/include/iiPaintEngine/Stroke/Rasterizer.h")
    message(FATAL_ERROR "The upgrade install did not publish the current Rasterizer.h header")
endif ()

set(consumer_source_dir "${test_root}/consumer")
set(consumer_build_dir "${test_root}/consumer-build")
file(MAKE_DIRECTORY "${consumer_source_dir}")
file(WRITE "${consumer_source_dir}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.31)
project(iiPaintEngineInstalledConsumer LANGUAGES CXX)

find_package(iiPaintEngine CONFIG REQUIRED)

add_executable(iiPaintEngineInstalledConsumer main.cpp)
target_compile_features(iiPaintEngineInstalledConsumer PRIVATE cxx_std_17)
target_link_libraries(iiPaintEngineInstalledConsumer PRIVATE iiPaintEngine::iiPaintEngine)

if (WIN32)
    add_custom_command(TARGET iiPaintEngineInstalledConsumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:iiPaintEngine::iiPaintEngine>"
            "$<TARGET_FILE_DIR:iiPaintEngineInstalledConsumer>")
endif ()

add_custom_target(runInstalledConsumer ALL
    COMMAND "$<TARGET_FILE:iiPaintEngineInstalledConsumer>"
    DEPENDS iiPaintEngineInstalledConsumer
    VERBATIM)
]=])
file(WRITE "${consumer_source_dir}/main.cpp" [=[
#include <iiPaintEngine>

int main()
{
    BrushState brush{};
    hello();
    return brush.randomSeed == 0 ? 0 : 1;
}
]=])

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${consumer_source_dir}"
    -B "${consumer_build_dir}"
    "-DiiPaintEngine_DIR=${test_prefix}/lib/cmake/iiPaintEngine"
    "-DQt6_DIR=${IIPAINTENGINE_QT6_DIR}"
    -DCMAKE_BUILD_TYPE=Release)
if (DEFINED IIPAINTENGINE_GENERATOR AND NOT IIPAINTENGINE_GENERATOR STREQUAL "")
    list(APPEND configure_command -G "${IIPAINTENGINE_GENERATOR}")
endif ()
if (DEFINED IIPAINTENGINE_GENERATOR_PLATFORM AND NOT IIPAINTENGINE_GENERATOR_PLATFORM STREQUAL "")
    list(APPEND configure_command -A "${IIPAINTENGINE_GENERATOR_PLATFORM}")
endif ()
if (DEFINED IIPAINTENGINE_GENERATOR_TOOLSET AND NOT IIPAINTENGINE_GENERATOR_TOOLSET STREQUAL "")
    list(APPEND configure_command -T "${IIPAINTENGINE_GENERATOR_TOOLSET}")
endif ()
if (DEFINED IIPAINTENGINE_CXX_COMPILER AND NOT IIPAINTENGINE_CXX_COMPILER STREQUAL "")
    list(APPEND configure_command "-DCMAKE_CXX_COMPILER=${IIPAINTENGINE_CXX_COMPILER}")
endif ()
if (DEFINED IIPAINTENGINE_MAKE_PROGRAM AND NOT IIPAINTENGINE_MAKE_PROGRAM STREQUAL "")
    list(APPEND configure_command "-DCMAKE_MAKE_PROGRAM=${IIPAINTENGINE_MAKE_PROGRAM}")
endif ()

set(configure_invocation ${configure_command})
if (APPLE)
    set(consumer_library_path "${test_prefix}/lib")
    if (NOT "$ENV{LIBRARY_PATH}" STREQUAL "")
        string(APPEND consumer_library_path ":$ENV{LIBRARY_PATH}")
    endif ()
    set(configure_invocation
        "${CMAKE_COMMAND}" -E env
        "LIBRARY_PATH=${consumer_library_path}"
        ${configure_command})
endif ()

execute_process(
    COMMAND ${configure_invocation}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error)
if (NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "The installed-package consumer configure failed with exit code ${configure_result}.\n"
        "stdout:\n${configure_output}\n"
        "stderr:\n${configure_error}")
endif ()

set(build_command "${CMAKE_COMMAND}" --build "${consumer_build_dir}")
if (DEFINED IIPAINTENGINE_INSTALL_CONFIG AND NOT IIPAINTENGINE_INSTALL_CONFIG STREQUAL "")
    list(APPEND build_command --config "${IIPAINTENGINE_INSTALL_CONFIG}")
endif ()
execute_process(
    COMMAND ${build_command}
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error)
if (NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "The installed-package consumer build or load failed with exit code ${build_result}.\n"
        "stdout:\n${build_output}\n"
        "stderr:\n${build_error}")
endif ()
if (NOT build_output MATCHES "Hello, World!")
    message(FATAL_ERROR "The installed-package consumer did not execute the iiPaintEngine library")
endif ()
