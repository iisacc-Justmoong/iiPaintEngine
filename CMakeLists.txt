cmake_minimum_required(VERSION 3.31)
project(iiPaintEngine VERSION 0.1.0 LANGUAGES CXX)

set(IIPAINTENGINE_LICENSE_SPDX "AGPL-3.0-only")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (APPLE AND NOT Qt6_DIR)
    file(GLOB _qt_macos_roots LIST_DIRECTORIES true "$ENV{HOME}/Qt/*/macos")
    if (_qt_macos_roots)
        list(SORT _qt_macos_roots COMPARE NATURAL ORDER DESCENDING)
        list(GET _qt_macos_roots 0 _qt_macos_root)
        list(APPEND CMAKE_PREFIX_PATH "${_qt_macos_root}")
    endif ()
endif ()

if (NOT LVRS_DIR)
    if (APPLE AND EXISTS "$ENV{HOME}/.local/LVRS/platforms/macos")
        list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local/LVRS/platforms/macos")
    elseif (EXISTS "$ENV{HOME}/.local/LVRS")
        list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local/LVRS")
    endif ()
endif ()

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
        iiPaintEngine
        library.cpp
        Core/EngineConfig.cpp
        Core/EngineConfig.h
        Core/EngineError.cpp
        Core/EngineError.h
        Core/PaintUuid.cpp
        Core/PaintUuid.h
        Core/PaintPoint.cpp
        Core/PaintPoint.h
        Core/PaintRect.cpp
        Core/PaintRect.h
        Core/RasterSample.cpp
        Core/RasterSample.h
        Core/RasterBlendMode.h
        Core/CoordinateSpace.cpp
        Core/CoordinateSpace.h
        BitmapFile/BitmapFile.cpp
        BitmapFile/BitmapFile.h
        Document/PaintDocument.cpp
        Document/PaintDocument.h
        Document/DocumentMetadata.cpp
        Document/DocumentMetadata.h
        Document/DocumentSnapshot.cpp
        Document/DocumentSnapshot.h
        Document/DocumentSerializer.cpp
        Document/DocumentSerializer.h
        Layer/Layer.cpp
        Layer/Layer.h
        Layer/LayerMetadata.cpp
        Layer/LayerMetadata.h
        Layer/DrawingSurface.cpp
        Layer/DrawingSurface.h
        Layer/LayerStack.cpp
        Layer/LayerStack.h
        Layer/RasterLayer.cpp
        Layer/RasterLayer.h
        Brush/BrushPreset.cpp
        Brush/BrushPreset.h
        Brush/BrushMaterial.cpp
        Brush/BrushMaterial.h
        Brush/BrushPresetSerializer.cpp
        Brush/BrushPresetSerializer.h
        Brush/BrushDynamics.cpp
        Brush/BrushDynamics.h
        Brush/BrushShape.cpp
        Brush/BrushShape.h
        Brush/BrushTip.cpp
        Brush/BrushTip.h
        Brush/BrushLibrary.cpp
        Brush/BrushLibrary.h
        Brush/BrushSnapshot.cpp
        Brush/BrushSnapshot.h
        Brush/BrushResolve.cpp
        Brush/BrushResolve.h
        Stroke/Rasterizer.cpp
        Stroke/Rasterizer.h
        Stroke/StrokePoint.cpp
        Stroke/StrokePoint.h
        Render/Renderer.cpp
        Render/Renderer.h
        Render/RenderContext.cpp
        Render/RenderContext.h
        Render/DirtyRegion.cpp
        Render/DirtyRegion.h
        Render/RenderCache.cpp
        Render/RenderCache.h
        Render/Compositor.cpp
        Render/Compositor.h
        Render/CpuRenderer.cpp
        Render/CpuRenderer.h
        Render/GpuRenderer.cpp
        Render/GpuRenderer.h
        History/Command.cpp
        History/Command.h
        History/HistoryStack.cpp
        History/HistoryStack.h
        History/UndoRedoController.cpp
        History/UndoRedoController.h
        History/HistorySnapshot.cpp
        History/HistorySnapshot.h
        Input/PointerEvent.cpp
        Input/PointerEvent.h
        Input/PressureInput.cpp
        Input/PressureInput.h
        Input/TabletState.cpp
        Input/TabletState.h
        Input/InputNormalizer.cpp
        Input/InputNormalizer.h
        Input/InputStrokeBuilder.cpp
        Input/InputStrokeBuilder.h
        Color/PaintColor.cpp
        Color/PaintColor.h
        Color/Palette.cpp
        Color/Palette.h
        Color/ColorSpace.cpp
        Color/ColorSpace.h
        Color/Gradient.cpp
        Color/Gradient.h
        Selection/Selection.cpp
        Selection/Selection.h
        Transform/Transform.cpp
        Transform/Transform.h
        Transform/ViewportTransform.cpp
        Transform/ViewportTransform.h
        Filter/FilterPipeline.cpp
        Filter/FilterPipeline.h
        Tool/RasterEditTool.cpp
        Tool/RasterEditTool.h
        Tool/ToolStateMachine.cpp
        Tool/ToolStateMachine.h
        QtAdapter/BitmapFileItem.cpp
        QtAdapter/BitmapFileItem.h
        QtAdapter/BitmapFileApiConfig.h
        QtAdapter/BitmapBrushConfig.h
        QtAdapter/IipeQmlTypes.cpp
        QtAdapter/IipeQmlTypes.h
        QtAdapter/PaintEngineController.cpp
        QtAdapter/PaintEngineController.h
        QtAdapter/DocumentAdapter.cpp
        QtAdapter/DocumentAdapter.h
        QtAdapter/LayerListModel.cpp
        QtAdapter/LayerListModel.h
        Core/Types.h
        Core/Geometry.h)

add_library(iiPaintEngine ${IIPAINTENGINE_LIBRARY_TYPE}
        ${IIPAINTENGINE_SOURCES})

if (APPLE AND IIPAINTENGINE_LIBRARY_TYPE STREQUAL "SHARED")
    set_target_properties(iiPaintEngine PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE)
endif ()

target_include_directories(iiPaintEngine PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine>)

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

install(FILES library.h iiPaintEngine
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine)

install(DIRECTORY
        BitmapFile
        Brush
        Color
        Core
        Document
        Filter
        History
        Input
        Layer
        QtAdapter
        Render
        Selection
        Stroke
        Tool
        Transform
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
