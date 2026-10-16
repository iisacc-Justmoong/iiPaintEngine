cmake_minimum_required(VERSION 3.31)
if(NOT DEFINED IIPAINTENGINE_BINARY_DIR)
    message(FATAL_ERROR "IIPAINTENGINE_BINARY_DIR is required")
endif()
set(test_root "${IIPAINTENGINE_BINARY_DIR}/install-upgrade-contract")
set(test_prefix "${test_root}/prefix")
file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_prefix}/include/iiPaintEngine/QtAdapter")
file(WRITE "${test_prefix}/include/iiPaintEngine/QtAdapter/BitmapFileItem.h" "retired header\n")
file(WRITE "${test_prefix}/include/iiPaintEngine/obsolete.h" "retired header\n")
set(install_command "${CMAKE_COMMAND}" --install "${IIPAINTENGINE_BINARY_DIR}" --prefix "${test_prefix}")
if(IIPAINTENGINE_INSTALL_CONFIG)
    list(APPEND install_command --config "${IIPAINTENGINE_INSTALL_CONFIG}")
endif()
execute_process(COMMAND ${install_command} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Upgrade install failed (${result}):\n${output}\n${error}")
endif()
if(CMAKE_HOST_APPLE AND EXISTS "${test_prefix}/lib/libiiPaintEngine.dylib")
    execute_process(COMMAND /usr/bin/otool -D "${test_prefix}/lib/libiiPaintEngine.dylib"
        RESULT_VARIABLE id_result OUTPUT_VARIABLE install_id ERROR_VARIABLE id_error)
    if(NOT id_result EQUAL 0 OR NOT install_id MATCHES "\n@rpath/libiiPaintEngine[.]dylib\n")
        message(FATAL_ERROR "Staged SDK dylib must have a relocatable install ID: ${install_id} ${id_error}")
    endif()
endif()
if(EXISTS "${test_prefix}/include/iiPaintEngine/QtAdapter" OR EXISTS "${test_prefix}/include/iiPaintEngine/obsolete.h")
    message(FATAL_ERROR "Upgrade retained retired public headers")
endif()
if(NOT EXISTS "${test_prefix}/include/iiPaintEngine/Application/BitmapPaintSession.h")
    message(FATAL_ERROR "Upgrade did not install the C++ painting session")
endif()
if(NOT EXISTS "${test_prefix}/include/iiPaintEngine/Application/MaskPaintSession.h"
        OR NOT EXISTS "${test_prefix}/include/iiPaintEngine/Core/PixelMask.h"
        OR NOT EXISTS "${test_prefix}/include/iiPaintEngine/Brush/LocalMaskBrush.h"
        OR NOT EXISTS "${test_prefix}/include/iiPaintEngine/Brush/LocalizedBrushMath.h"
        OR NOT EXISTS "${test_prefix}/share/iiPaintEngine/LOCAL_MASK_BRUSH.md"
        OR NOT EXISTS "${test_prefix}/share/iiPaintEngine/LOCALIZED_BRUSH_MATH.md"
        OR NOT EXISTS "${test_prefix}/share/iiPaintEngine/localized-brush-math-provenance.json"
        OR NOT EXISTS "${test_prefix}/share/iiPaintEngine/localized-brush-vectors.json")
    message(FATAL_ERROR "Upgrade did not install the local mask brush API and documentation")
endif()
set(consumer_source_dir "${test_root}/consumer")
set(consumer_build_dir "${test_root}/build")
file(MAKE_DIRECTORY "${consumer_source_dir}")
file(WRITE "${consumer_source_dir}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.31)
project(iiPaintEngineInstalledConsumer LANGUAGES CXX)
find_package(iiPaintEngine 0.4.2 EXACT CONFIG REQUIRED)
add_executable(iiPaintEngineInstalledConsumer main.cpp)
target_link_libraries(iiPaintEngineInstalledConsumer PRIVATE iiPaintEngine::iiPaintEngine)
set_target_properties(iiPaintEngineInstalledConsumer PROPERTIES CXX_EXTENSIONS OFF)
if(WIN32)
    add_custom_command(TARGET iiPaintEngineInstalledConsumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:iiPaintEngine::iiPaintEngine>"
            "$<TARGET_FILE_DIR:iiPaintEngineInstalledConsumer>")
endif()
add_custom_target(runInstalledConsumer ALL
    COMMAND "${CMAKE_COMMAND}" -E env
        "DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:iiPaintEngine::iiPaintEngine>"
        "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:iiPaintEngine::iiPaintEngine>"
        "$<TARGET_FILE:iiPaintEngineInstalledConsumer>" "${CMAKE_CURRENT_BINARY_DIR}/paint.png"
    DEPENDS iiPaintEngineInstalledConsumer VERBATIM)
]=])
file(WRITE "${consumer_source_dir}/main.cpp" [=[
#include <iiPaintEngine>
#include <iostream>
static_assert(__cplusplus > 202002L);
int main(int argc, char **argv)
{
    if(argc != 2) return 1;
    BitmapPaintSession session;
    if(!session.createFile(argv[1],32,32)) return 2;
    auto preset = builtInBrushPresets().front();
    preset.size = 8;
    preset.dynamics.sizeVariation.enabled = true;
    preset.dynamics.sizeVariation.control = BrushDynamicsControl::PenPressure;
    if(!session.setBrushPresetData(serializeBrushPreset(preset))) return 3;
    PointerEvent point;
    point.phase = PointerEventPhase::Press;
    point.button = PointerButton::Primary;
    point.primaryButtonDown = true;
    point.documentPosition = {16,16};
    point.pressure = 0.8;
    if(!session.processPointerEvent(point).accepted) return 4;
    point.phase = PointerEventPhase::Release;
    point.primaryButtonDown = false;
    if(!session.processPointerEvent(point).committed || !session.save()) return 5;
    BitmapFile reopened;
    if(!reopened.open(argv[1]) || reopened.pixels().pixels != session.file().pixels().pixels) return 6;
    if(!session.undo() || !session.redo()) return 7;
    MaskPaintSession mask;
    if(!mask.createMask(8,8)) return 8;
    LocalMaskBrushOptions options; options.sizePixels=4; options.feather=0; options.density=.6;
    if(!mask.setBrushOptions(options)) return 9;
    point.phase=PointerEventPhase::Press; point.primaryButtonDown=true; point.button=PointerButton::Primary;
    point.documentPosition={4,4};
    if(!mask.processPointerEvent(point).accepted || mask.mask().coverage[36]!=0) return 10;
    point.phase=PointerEventPhase::Release; point.primaryButtonDown=false;
    if(!mask.processPointerEvent(point).committed || mask.mask().coverage[36]<.59F || mask.mask().coverage[36]>.61F) return 11;
    if(!mask.undo() || mask.mask().coverage[36]!=0 || !mask.redo()) return 12;
    std::cout << "Installed C++23 bitmap consumer passed\n";
}
]=])
set(configure_command "${CMAKE_COMMAND}" -S "${consumer_source_dir}" -B "${consumer_build_dir}"
    "-DiiPaintEngine_DIR=${test_prefix}/lib/cmake/iiPaintEngine" -DCMAKE_BUILD_TYPE=Release)
if(IIPAINTENGINE_GENERATOR)
    list(APPEND configure_command -G "${IIPAINTENGINE_GENERATOR}")
endif()
if(IIPAINTENGINE_CXX_COMPILER)
    list(APPEND configure_command "-DCMAKE_CXX_COMPILER=${IIPAINTENGINE_CXX_COMPILER}")
endif()
execute_process(COMMAND ${configure_command} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed consumer configure failed (${result}):\n${output}\n${error}")
endif()
set(build_command "${CMAKE_COMMAND}" --build "${consumer_build_dir}")
if(IIPAINTENGINE_INSTALL_CONFIG)
    list(APPEND build_command --config "${IIPAINTENGINE_INSTALL_CONFIG}")
endif()
execute_process(COMMAND ${build_command} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed consumer build or load failed (${result}):\n${output}\n${error}")
endif()
if(NOT output MATCHES "Installed C[+][+]23 bitmap consumer passed")
    message(FATAL_ERROR "Installed consumer did not execute successfully:\n${output}")
endif()
