cmake_minimum_required(VERSION 3.31)
project(iiPaintEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE AND NOT Qt6_DIR)
    file(GLOB _qt_macos_roots LIST_DIRECTORIES true "$ENV{HOME}/Qt/*/macos")
    if(_qt_macos_roots)
        list(SORT _qt_macos_roots COMPARE NATURAL ORDER DESCENDING)
        list(GET _qt_macos_roots 0 _qt_macos_root)
        list(APPEND CMAKE_PREFIX_PATH "${_qt_macos_root}")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

if(APPLE)
    foreach(_qt_target Qt6::Gui WrapOpenGL::WrapOpenGL)
        if(TARGET ${_qt_target})
            get_target_property(_qt_link_libraries ${_qt_target} INTERFACE_LINK_LIBRARIES)
            if(_qt_link_libraries)
                list(REMOVE_ITEM _qt_link_libraries "-framework" "AGL" "-framework AGL")
                list(FILTER _qt_link_libraries EXCLUDE REGEX "AGL")
                set_target_properties(${_qt_target} PROPERTIES
                        INTERFACE_LINK_LIBRARIES "${_qt_link_libraries}")
            endif()
            get_target_property(_qt_link_options ${_qt_target} INTERFACE_LINK_OPTIONS)
            if(_qt_link_options)
                list(FILTER _qt_link_options EXCLUDE REGEX "AGL")
                set_target_properties(${_qt_target} PROPERTIES
                        INTERFACE_LINK_OPTIONS "${_qt_link_options}")
            endif()
        endif()
    endforeach()
endif()

set(CMAKE_AUTOMOC ON)

add_library(iiPaintEngine SHARED library.cpp
        Brush/BrushPreset.cpp
        Brush/BrushPreset.h
        Brush/BrushDynamics.cpp
        Brush/BrushDynamics.h
        Brush/BrushShape.cpp
        Brush/BrushShape.h
        Brush/BrushResolve.cpp
        Brush/BrushResolve.h
        Stroke/Rasterizer.cpp
        Stroke/Rasterizer.h
        Stroke/Stabilizer.cpp
        Stroke/Stabilizer.h
        Stroke/StrokePoint.cpp
        Stroke/StrokePoint.h
        Stroke/Stroke.cpp
        Stroke/Stroke.h
        Layer/Layer.cpp
        Layer/Layer.h
        Canvas/DrawingSurface.cpp
        Canvas/DrawingSurface.h)

target_include_directories(iiPaintEngine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(iiPaintEngine PUBLIC
        Qt6::Core
        Qt6::Gui)

enable_testing()

add_executable(iiPaintEngineTests
        tests/ObjectStorageContractTest.cpp)

target_link_libraries(iiPaintEngineTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineObjectStorageContract
        COMMAND iiPaintEngineTests)
