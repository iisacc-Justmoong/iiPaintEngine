cmake_minimum_required(VERSION 3.31)
project(iiPaintEngine VERSION 0.1.0 LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(_sdk_home "$ENV{HOME}")
    if(_sdk_home STREQUAL "")
        set(_sdk_home "$ENV{USERPROFILE}")
    endif()
    set(CMAKE_INSTALL_PREFIX "${_sdk_home}/.local/SDK/iiPaintEngine"
        CACHE PATH "SDK installation prefix" FORCE)
    unset(_sdk_home)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(IIPAINTENGINE_NATIVE_POINTER_INPUT_SOURCE
            src/QtAdapter/NativePointerInputPolicy_mac.mm)
else ()
    set(IIPAINTENGINE_NATIVE_POINTER_INPUT_SOURCE
            src/QtAdapter/NativePointerInputPolicy.cpp)
endif ()

set(IIPAINTENGINE_LICENSE_SPDX "AGPL-3.0-only")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (APPLE AND NOT Qt6_DIR)
    file(GLOB _qt_macos_roots LIST_DIRECTORIES true "/Volumes/Storage/Qt/*/macos")
    if (_qt_macos_roots)
        list(SORT _qt_macos_roots COMPARE NATURAL ORDER DESCENDING)
        list(GET _qt_macos_roots 0 _qt_macos_root)
        list(APPEND CMAKE_PREFIX_PATH "${_qt_macos_root}")
    endif ()
endif ()

if (NOT LVRS_DIR)
    if (APPLE AND EXISTS "$ENV{HOME}/.local/SDK/LVRS/platforms/macos")
        list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local/SDK/LVRS/platforms/macos")
    elseif (EXISTS "$ENV{HOME}/.local/SDK/LVRS")
        list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local/SDK/LVRS")
    endif ()
endif ()

find_package(iiFileProvider 0.5.0 CONFIG REQUIRED HINTS "$ENV{HOME}/.local/SDK/iiFileProvider")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)
find_package(LVRS CONFIG REQUIRED)

set(IIPAINTENGINE_LVRS_LIBRARY_DIR "")
if (TARGET LVRS::LVRS)
    get_target_property(IIPAINTENGINE_LVRS_LIBRARY LVRS::LVRS IMPORTED_LOCATION)
    if (NOT IIPAINTENGINE_LVRS_LIBRARY)
        get_target_property(IIPAINTENGINE_LVRS_LIBRARY LVRS::LVRS IMPORTED_LOCATION_RELEASE)
    endif ()
    if (IIPAINTENGINE_LVRS_LIBRARY)
        get_filename_component(IIPAINTENGINE_LVRS_LIBRARY_DIR "${IIPAINTENGINE_LVRS_LIBRARY}" DIRECTORY)
    endif ()
endif ()

if (COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif ()

if (APPLE)
    foreach (_qt_target Qt6::Gui WrapOpenGL::WrapOpenGL)
        if (TARGET ${_qt_target})
            get_target_property(_qt_link_libraries ${_qt_target} INTERFACE_LINK_LIBRARIES)
            if (_qt_link_libraries)
                list(REMOVE_ITEM _qt_link_libraries "-framework" "AGL" "-framework AGL")
                list(FILTER _qt_link_libraries EXCLUDE REGEX "AGL")
                set_target_properties(${_qt_target} PROPERTIES
                        INTERFACE_LINK_LIBRARIES "${_qt_link_libraries}")
            endif ()
            get_target_property(_qt_link_options ${_qt_target} INTERFACE_LINK_OPTIONS)
            if (_qt_link_options)
                list(FILTER _qt_link_options EXCLUDE REGEX "AGL")
                set_target_properties(${_qt_target} PROPERTIES
                        INTERFACE_LINK_OPTIONS "${_qt_link_options}")
            endif ()
        endif ()
    endforeach ()
endif ()

set(CMAKE_AUTOMOC ON)

option(IIPAINTENGINE_BUILD_SHARED "Build iiPaintEngine as a shared library." ON)

set(IIPAINTENGINE_LIBRARY_TYPE STATIC)
if (EMSCRIPTEN)
    set(IIPAINTENGINE_LIBRARY_TYPE STATIC)
elseif (IIPAINTENGINE_BUILD_SHARED)
    set(IIPAINTENGINE_LIBRARY_TYPE SHARED)
endif ()

set(IIPAINTENGINE_SOURCES
        src/iiPaintEngine
        src/library.cpp
        src/Core/EngineConfig.cpp
        src/Core/EngineConfig.h
        src/Core/EngineError.cpp
        src/Core/EngineError.h
        src/Core/PaintUuid.cpp
        src/Core/PaintUuid.h
        src/Core/PaintPoint.cpp
        src/Core/PaintPoint.h
        src/Core/PaintRect.cpp
        src/Core/PaintRect.h
        src/Core/RasterSample.cpp
        src/Core/RasterSample.h
        src/Core/RasterBlendMode.h
        src/Core/CoordinateSpace.cpp
        src/Core/CoordinateSpace.h
        src/BitmapFile/BitmapFile.cpp
        src/BitmapFile/BitmapFile.h
        src/Document/PaintDocument.cpp
        src/Document/PaintDocument.h
        src/Document/DocumentMetadata.cpp
        src/Document/DocumentMetadata.h
        src/Document/DocumentSnapshot.cpp
        src/Document/DocumentSnapshot.h
        src/Document/DocumentSerializer.cpp
        src/Document/DocumentSerializer.h
        src/Layer/Layer.cpp
        src/Layer/Layer.h
        src/Layer/LayerMetadata.cpp
        src/Layer/LayerMetadata.h
        src/Layer/DrawingSurface.cpp
        src/Layer/DrawingSurface.h
        src/Layer/LayerStack.cpp
        src/Layer/LayerStack.h
        src/Layer/RasterLayer.cpp
        src/Layer/RasterLayer.h
        src/Brush/BrushPreset.cpp
        src/Brush/BrushPreset.h
        src/Brush/BrushMaterial.cpp
        src/Brush/BrushMaterial.h
        src/Brush/BrushPresetSerializer.cpp
        src/Brush/BrushPresetSerializer.h
        src/Brush/BrushDynamics.cpp
        src/Brush/BrushDynamics.h
        src/Brush/BrushShape.cpp
        src/Brush/BrushShape.h
        src/Brush/BrushTip.cpp
        src/Brush/BrushTip.h
        src/Brush/BrushLibrary.cpp
        src/Brush/BrushLibrary.h
        src/Brush/BrushSnapshot.cpp
        src/Brush/BrushSnapshot.h
        src/Brush/BrushResolve.cpp
        src/Brush/BrushResolve.h
        src/Stroke/Rasterizer.cpp
        src/Stroke/Rasterizer.h
        src/Stroke/StrokePoint.cpp
        src/Stroke/StrokePoint.h
        src/Render/Renderer.cpp
        src/Render/Renderer.h
        src/Render/RenderContext.cpp
        src/Render/RenderContext.h
        src/Render/DirtyRegion.cpp
        src/Render/DirtyRegion.h
        src/Render/RenderCache.cpp
        src/Render/RenderCache.h
        src/Render/Compositor.cpp
        src/Render/Compositor.h
        src/Render/CpuRenderer.cpp
        src/Render/CpuRenderer.h
        src/Render/GpuRenderer.cpp
        src/Render/GpuRenderer.h
        src/History/Command.cpp
        src/History/Command.h
        src/History/HistoryStack.cpp
        src/History/HistoryStack.h
        src/History/UndoRedoController.cpp
        src/History/UndoRedoController.h
        src/History/HistorySnapshot.cpp
        src/History/HistorySnapshot.h
        src/Input/PointerEvent.cpp
        src/Input/PointerEvent.h
        src/Input/PressureInput.cpp
        src/Input/PressureInput.h
        src/Input/TabletState.cpp
        src/Input/TabletState.h
        src/Input/InputNormalizer.cpp
        src/Input/InputNormalizer.h
        src/Input/InputStrokeBuilder.cpp
        src/Input/InputStrokeBuilder.h
        src/Color/PaintColor.cpp
        src/Color/PaintColor.h
        src/Color/Palette.cpp
        src/Color/Palette.h
        src/Color/ColorSpace.cpp
        src/Color/ColorSpace.h
        src/Color/Gradient.cpp
        src/Color/Gradient.h
        src/Selection/Selection.cpp
        src/Selection/Selection.h
        src/Transform/Transform.cpp
        src/Transform/Transform.h
        src/Transform/ViewportTransform.cpp
        src/Transform/ViewportTransform.h
        src/Filter/FilterPipeline.cpp
        src/Filter/FilterPipeline.h
        src/Tool/RasterEditTool.cpp
        src/Tool/RasterEditTool.h
        src/Tool/ToolStateMachine.cpp
        src/Tool/ToolStateMachine.h
        src/QtAdapter/BitmapFileItem.cpp
        src/QtAdapter/BitmapFileItem.h
        src/QtAdapter/HighFidelityPointerInput.cpp
        src/QtAdapter/HighFidelityPointerInput.h
        ${IIPAINTENGINE_NATIVE_POINTER_INPUT_SOURCE}
        src/QtAdapter/BitmapFileApiConfig.h
        src/QtAdapter/BitmapBrushConfig.h
        src/QtAdapter/IipeQmlTypes.cpp
        src/QtAdapter/IipeQmlTypes.h
        src/QtAdapter/PaintEngineController.cpp
        src/QtAdapter/PaintEngineController.h
        src/QtAdapter/DocumentAdapter.cpp
        src/QtAdapter/DocumentAdapter.h
        src/QtAdapter/LayerListModel.cpp
        src/QtAdapter/LayerListModel.h
        src/Core/Types.h
        src/Core/Geometry.h)

add_library(iiPaintEngine ${IIPAINTENGINE_LIBRARY_TYPE}
        ${IIPAINTENGINE_SOURCES})

if (APPLE AND IIPAINTENGINE_LIBRARY_TYPE STREQUAL "SHARED")
    set_target_properties(iiPaintEngine PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE)
endif ()

target_include_directories(iiPaintEngine PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine>)

target_link_libraries(iiPaintEngine PRIVATE iiFileProvider::iiFileProvider)
target_link_libraries(iiPaintEngine PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick)

if (COMMAND qt_import_plugins)
    qt_import_plugins(iiPaintEngine NO_DEFAULT)
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_link_options(iiPaintEngine INTERFACE "--bind")
endif ()

install(TARGETS iiPaintEngine
        EXPORT iiPaintEngineTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/iiPaintEngine")

# CMake's directory installer only overwrites current headers; it does not remove
# headers deleted by an upgrade. Replace the package-owned public header tree so
# retired APIs cannot remain visible to consumers.
install(CODE
        "file(REMOVE_RECURSE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine\")")

install(FILES src/library.h src/iiPaintEngine
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine)

install(DIRECTORY
        src/BitmapFile
        src/Brush
        src/Color
        src/Core
        src/Document
        src/Filter
        src/History
        src/Input
        src/Layer
        src/QtAdapter
        src/Render
        src/Selection
        src/Stroke
        src/Tool
        src/Transform
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine
        FILES_MATCHING PATTERN "*.h")

install(EXPORT iiPaintEngineTargets
        FILE iiPaintEngineTargets.cmake
        NAMESPACE iiPaintEngine::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iiPaintEngine)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/iiPaintEngineConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iiPaintEngine)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineConfigVersionRoot.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iiPaintEngine)

set(IIPAINTENGINE_EXAMPLE_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/Example/bin")

lvrs_add_qml_app(
        TARGET iiPaintEngineExample
        URI IiPaintEngineExample
        VERSION 1.0
        ROOT_OBJECT Main
        APP_NAME "iiPaintEngine Example"
        SOURCES
        Example/main.cpp
        QML_FILES
        Example/Main.qml
        NO_PLATFORM_RUNTIME_TARGETS)

target_link_libraries(iiPaintEngineExample PRIVATE
        iiPaintEngine)

set_target_properties(iiPaintEngineExample PROPERTIES
        MACOSX_BUNDLE FALSE
        RUNTIME_OUTPUT_DIRECTORY "${IIPAINTENGINE_EXAMPLE_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${IIPAINTENGINE_EXAMPLE_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${IIPAINTENGINE_EXAMPLE_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${IIPAINTENGINE_EXAMPLE_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${IIPAINTENGINE_EXAMPLE_BIN_DIR}")

if (APPLE AND IIPAINTENGINE_LVRS_LIBRARY_DIR)
    set_property(TARGET iiPaintEngineExample APPEND PROPERTY
            BUILD_RPATH "${IIPAINTENGINE_LVRS_LIBRARY_DIR}")
endif ()

enable_testing()

add_executable(iiPaintEngineTests
        tests/BlueprintStructureTest.cpp)

target_link_libraries(iiPaintEngineTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBlueprintStructure
        COMMAND iiPaintEngineTests)

add_executable(iiPaintEngineCoreTests
        tests/CoreContractTest.cpp)

target_link_libraries(iiPaintEngineCoreTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineCoreContract
        COMMAND iiPaintEngineCoreTests)

add_executable(iiPaintEnginePipelineTests
        tests/PipelineHeartbeatTest.cpp)

target_link_libraries(iiPaintEnginePipelineTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEnginePipelineHeartbeat
        COMMAND iiPaintEnginePipelineTests)

add_executable(iiPaintEngineBitmapDocumentStructureTests
        tests/BitmapDocumentStructureTest.cpp)

target_link_libraries(iiPaintEngineBitmapDocumentStructureTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBitmapDocumentStructure
        COMMAND iiPaintEngineBitmapDocumentStructureTests)

add_executable(iiPaintEngineDocumentSerializerContractTests
        tests/DocumentSerializerContractTest.cpp)

target_link_libraries(iiPaintEngineDocumentSerializerContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineDocumentSerializerContract
        COMMAND iiPaintEngineDocumentSerializerContractTests)

add_executable(iiPaintEngineAppDocumentApiContractTests
        tests/AppDocumentApiContractTest.cpp)

target_link_libraries(iiPaintEngineAppDocumentApiContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineAppDocumentApiContract
        COMMAND iiPaintEngineAppDocumentApiContractTests)

add_executable(iiPaintEngineInstallLayoutContractTests
        tests/InstallLayoutContractTest.cpp)

set_target_properties(iiPaintEngineInstallLayoutContractTests PROPERTIES
        OUTPUT_NAME iiPaintEngineLayoutContractTests)

target_compile_definitions(iiPaintEngineInstallLayoutContractTests PRIVATE
        IIPAINTENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_test(NAME iiPaintEngineInstallLayoutContract
        COMMAND iiPaintEngineInstallLayoutContractTests)

add_test(NAME iiPaintEngineInstallUpgradeContract
        COMMAND ${CMAKE_COMMAND}
        "-DIIPAINTENGINE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DIIPAINTENGINE_INSTALL_CONFIG=$<CONFIG>"
        "-DIIPAINTENGINE_GENERATOR=${CMAKE_GENERATOR}"
        "-DIIPAINTENGINE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DIIPAINTENGINE_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DIIPAINTENGINE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DIIPAINTENGINE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
        "-DIIPAINTENGINE_QT6_DIR=${Qt6_DIR}"
        "-DIIPAINTENGINE_IIFILEPROVIDER_DIR=${iiFileProvider_DIR}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/InstallUpgradeContractTest.cmake)

add_executable(iiPaintEnginePublicUmbrellaHeaderContractTests
        tests/PublicUmbrellaHeaderContractTest.cpp)

target_link_libraries(iiPaintEnginePublicUmbrellaHeaderContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEnginePublicUmbrellaHeaderContract
        COMMAND iiPaintEnginePublicUmbrellaHeaderContractTests)

add_executable(iiPaintEnginePublicCxx17HeaderContractTests
        tests/PublicCxx17HeaderContractTest.cpp)

set_target_properties(iiPaintEnginePublicCxx17HeaderContractTests PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

target_link_libraries(iiPaintEnginePublicCxx17HeaderContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEnginePublicCxx17HeaderContract
        COMMAND iiPaintEnginePublicCxx17HeaderContractTests)

add_executable(iiPaintEngineBitmapFileQmlApiTests
        tests/BitmapFileQmlApiTest.cpp)

target_link_libraries(iiPaintEngineBitmapFileQmlApiTests PRIVATE
        iiPaintEngine
        Qt6::Qml)

add_test(NAME iiPaintEngineBitmapFileQmlApi
        COMMAND iiPaintEngineBitmapFileQmlApiTests)

add_executable(iiPaintEngineBitmapFileApiContractTests
        tests/BitmapFileApiContractTest.cpp)

target_link_libraries(iiPaintEngineBitmapFileApiContractTests PRIVATE
        iiFileProvider::iiFileProvider
        iiPaintEngine
        Qt6::Qml)

add_test(NAME iiPaintEngineBitmapFileApiContract
        COMMAND iiPaintEngineBitmapFileApiContractTests)

add_executable(iiPaintEngineExampleDemoContractTests
        tests/ExampleDemoAppContractTest.cpp)

add_dependencies(iiPaintEngineExampleDemoContractTests
        iiPaintEngineExample)

target_link_libraries(iiPaintEngineExampleDemoContractTests PRIVATE
        iiPaintEngine
        LVRS::LVRS
        Qt6::Qml
        Qt6::Quick)

target_compile_definitions(iiPaintEngineExampleDemoContractTests PRIVATE
        IIPAINTENGINE_EXAMPLE_MAIN_QML="${CMAKE_CURRENT_SOURCE_DIR}/Example/Main.qml"
        IIPAINTENGINE_EXAMPLE_EXECUTABLE="$<TARGET_FILE:iiPaintEngineExample>"
        IIPAINTENGINE_EXAMPLE_DEMO_CONTRACT_EXECUTABLE="$<TARGET_FILE:iiPaintEngineExampleDemoContractTests>"
        IIPAINTENGINE_LVRS_QML_IMPORT_PATH="${LVRS_QML_IMPORT_PATH}"
        IIPAINTENGINE_LVRS_LIBRARY_DIR="${IIPAINTENGINE_LVRS_LIBRARY_DIR}")

if (APPLE AND IIPAINTENGINE_LVRS_LIBRARY_DIR)
    set_property(TARGET iiPaintEngineExampleDemoContractTests APPEND PROPERTY
            BUILD_RPATH "${IIPAINTENGINE_LVRS_LIBRARY_DIR}")
endif ()

add_test(NAME iiPaintEngineExampleDemoContract
        COMMAND iiPaintEngineExampleDemoContractTests)

add_executable(iiPaintEngineBitmapFilePointerAlignmentTests
        tests/BitmapFilePointerAlignmentTest.cpp)

target_link_libraries(iiPaintEngineBitmapFilePointerAlignmentTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBitmapFilePointerAlignment
        COMMAND iiPaintEngineBitmapFilePointerAlignmentTests)

add_executable(iiPaintEngineBitmapFileTabletPressureContractTests
        tests/BitmapFileTabletPressureContractTest.cpp)

target_link_libraries(iiPaintEngineBitmapFileTabletPressureContractTests PRIVATE
        iiPaintEngine)
if (TARGET Qt6::GuiPrivate)
    target_link_libraries(iiPaintEngineBitmapFileTabletPressureContractTests PRIVATE
            Qt6::GuiPrivate)
endif ()

add_test(NAME iiPaintEngineBitmapFileTabletPressureContract
        COMMAND iiPaintEngineBitmapFileTabletPressureContractTests)

add_executable(iiPaintEngineBitmapFileLivePreviewRealtimeContractTests
        tests/BitmapFileLivePreviewRealtimeContractTest.cpp)

target_link_libraries(iiPaintEngineBitmapFileLivePreviewRealtimeContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBitmapFileLivePreviewRealtimeContract
        COMMAND iiPaintEngineBitmapFileLivePreviewRealtimeContractTests)

add_executable(iiPaintEnginePointerStrokeFlowTests
        tests/PointerStrokeFlowTest.cpp)

target_link_libraries(iiPaintEnginePointerStrokeFlowTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEnginePointerStrokeFlow
        COMMAND iiPaintEnginePointerStrokeFlowTests)

add_executable(iiPaintEnginePressureInputContractTests
        tests/PressureInputContractTest.cpp)

target_link_libraries(iiPaintEnginePressureInputContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEnginePressureInputContract
        COMMAND iiPaintEnginePressureInputContractTests)

add_executable(iiPaintEngineTabletInputSurfaceTests
        tests/TabletInputSurfaceTest.cpp)

target_link_libraries(iiPaintEngineTabletInputSurfaceTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineTabletInputSurface
        COMMAND iiPaintEngineTabletInputSurfaceTests)

add_executable(iiPaintEngineRasterPaintingModelTests
        tests/RasterPaintingModelTest.cpp)

target_link_libraries(iiPaintEngineRasterPaintingModelTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineRasterPaintingModel
        COMMAND iiPaintEngineRasterPaintingModelTests)

add_executable(iiPaintEngineStrokePhysicalContractTests
        tests/StrokePhysicalContractTest.cpp)

target_link_libraries(iiPaintEngineStrokePhysicalContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineStrokePhysicalContract
        COMMAND iiPaintEngineStrokePhysicalContractTests)

add_executable(iiPaintEngineBrushDynamicsMappingTests
        tests/BrushDynamicsMappingTest.cpp)

target_link_libraries(iiPaintEngineBrushDynamicsMappingTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBrushDynamicsMapping
        COMMAND iiPaintEngineBrushDynamicsMappingTests)

add_executable(iiPaintEngineBrushDynamicsResponseCurveContractTests
        tests/BrushDynamicsResponseCurveContractTest.cpp)

target_link_libraries(iiPaintEngineBrushDynamicsResponseCurveContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBrushDynamicsResponseCurveContract
        COMMAND iiPaintEngineBrushDynamicsResponseCurveContractTests)

add_executable(iiPaintEngineBrushFeatureToggleContractTests
        tests/BrushFeatureToggleContractTest.cpp)

target_link_libraries(iiPaintEngineBrushFeatureToggleContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBrushFeatureToggleContract
        COMMAND iiPaintEngineBrushFeatureToggleContractTests)

add_executable(iiPaintEngineBrushExpressionContractTests
        tests/BrushExpressionContractTest.cpp)

target_link_libraries(iiPaintEngineBrushExpressionContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBrushExpressionContract
        COMMAND iiPaintEngineBrushExpressionContractTests)

add_executable(iiPaintEngineBrushTextureLayerContractTests
        tests/BrushTextureLayerContractTest.cpp)

target_link_libraries(iiPaintEngineBrushTextureLayerContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBrushTextureLayerContract
        COMMAND iiPaintEngineBrushTextureLayerContractTests)

add_executable(iiPaintEngineWetBrushSimulationContractTests
        tests/WetBrushSimulationContractTest.cpp)

target_link_libraries(iiPaintEngineWetBrushSimulationContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineWetBrushSimulationContract
        COMMAND iiPaintEngineWetBrushSimulationContractTests)

add_executable(iiPaintEngineHistoryUndoRedoContractTests
        tests/HistoryUndoRedoContractTest.cpp)

target_link_libraries(iiPaintEngineHistoryUndoRedoContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineHistoryUndoRedoContract
        COMMAND iiPaintEngineHistoryUndoRedoContractTests)

add_executable(iiPaintEngineEditingToolPipelineContractTests
        tests/EditingToolPipelineContractTest.cpp)

target_link_libraries(iiPaintEngineEditingToolPipelineContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineEditingToolPipelineContract
        COMMAND iiPaintEngineEditingToolPipelineContractTests)

add_executable(iiPaintEngineStrokeCompositingTests
        tests/StrokeCompositingTest.cpp)

target_link_libraries(iiPaintEngineStrokeCompositingTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineStrokeCompositing
        COMMAND iiPaintEngineStrokeCompositingTests)

add_executable(iiPaintEngineHighFidelityPointerInputContractTests
        tests/HighFidelityPointerInputContractTest.cpp)

target_link_libraries(iiPaintEngineHighFidelityPointerInputContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineHighFidelityPointerInputContract
        COMMAND iiPaintEngineHighFidelityPointerInputContractTests)

add_executable(iiPaintEngineLayerCompositingContractTests
        tests/LayerCompositingContractTest.cpp)

target_link_libraries(iiPaintEngineLayerCompositingContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineLayerCompositingContract
        COMMAND iiPaintEngineLayerCompositingContractTests)

add_executable(iiPaintEngineRendererProjectionContractTests
        tests/RendererProjectionContractTest.cpp)

target_link_libraries(iiPaintEngineRendererProjectionContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineRendererProjectionContract
        COMMAND iiPaintEngineRendererProjectionContractTests)

add_executable(iiPaintEngineRenderCacheBackendContractTests
        tests/RenderCacheBackendContractTest.cpp)

target_link_libraries(iiPaintEngineRenderCacheBackendContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineRenderCacheBackendContract
        COMMAND iiPaintEngineRenderCacheBackendContractTests)

add_executable(iiPaintEngineColorManagementContractTests
        tests/ColorManagementContractTest.cpp)

target_link_libraries(iiPaintEngineColorManagementContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineColorManagementContract
        COMMAND iiPaintEngineColorManagementContractTests)

add_executable(iiPaintEngineBrushMaskSamplingTests
        tests/BrushMaskSamplingTest.cpp)

target_link_libraries(iiPaintEngineBrushMaskSamplingTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBrushMaskSampling
        COMMAND iiPaintEngineBrushMaskSamplingTests)

add_executable(iiPaintEngineCoordinateDirtyRegionTests
        tests/CoordinateDirtyRegionTest.cpp)

target_link_libraries(iiPaintEngineCoordinateDirtyRegionTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineCoordinateDirtyRegion
        COMMAND iiPaintEngineCoordinateDirtyRegionTests)

add_executable(iiPaintEngineBitmapFileEventLoopLoadContractTests
        tests/BitmapFileEventLoopLoadContractTest.cpp)

target_link_libraries(iiPaintEngineBitmapFileEventLoopLoadContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBitmapFileEventLoopLoadContract
        COMMAND iiPaintEngineBitmapFileEventLoopLoadContractTests)

add_executable(iiPaintEngineBitmapOnlyArchitectureContractTests
        tests/BitmapOnlyArchitectureContractTest.cpp)

target_compile_definitions(iiPaintEngineBitmapOnlyArchitectureContractTests PRIVATE
        IIPAINTENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(iiPaintEngineBitmapOnlyArchitectureContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBitmapOnlyArchitectureContract
        COMMAND iiPaintEngineBitmapOnlyArchitectureContractTests)

add_executable(iiPaintEngineBitmapFileCompatibilityContractTests
        tests/BitmapFileCompatibilityContractTest.cpp)

target_link_libraries(iiPaintEngineBitmapFileCompatibilityContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBitmapFileCompatibilityContract
        COMMAND iiPaintEngineBitmapFileCompatibilityContractTests)

add_executable(iiPaintEngineBitmapFileArchitectureContractTests
        tests/BitmapFileArchitectureContractTest.cpp)

target_compile_definitions(iiPaintEngineBitmapFileArchitectureContractTests PRIVATE
        IIPAINTENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(iiPaintEngineBitmapFileArchitectureContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBitmapFileArchitectureContract
        COMMAND iiPaintEngineBitmapFileArchitectureContractTests)

add_test(NAME iiPaintEngineDependencyBoundary
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/DependencyBoundaryTest.cmake)

if (APPLE)
    get_property(IIPAINTENGINE_TEST_NAMES DIRECTORY PROPERTY TESTS)
    set_tests_properties(${IIPAINTENGINE_TEST_NAMES} PROPERTIES
            ENVIRONMENT_MODIFICATION
            "DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:iiPaintEngine>")
endif ()
