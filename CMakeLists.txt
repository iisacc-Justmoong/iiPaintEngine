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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick)

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

set(IIPAINTENGINE_SOURCES
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
        Core/CoordinateSpace.cpp
        Core/CoordinateSpace.h
        Document/PaintDocument.cpp
        Document/PaintDocument.h
        Document/DocumentMetadata.cpp
        Document/DocumentMetadata.h
        Document/DocumentSnapshot.cpp
        Document/DocumentSnapshot.h
        Document/DocumentSerializer.cpp
        Document/DocumentSerializer.h
        Canvas/CanvasState.cpp
        Canvas/CanvasState.h
        Canvas/CanvasViewport.cpp
        Canvas/CanvasViewport.h
        Canvas/CanvasSession.cpp
        Canvas/CanvasSession.h
        Layer/Layer.cpp
        Layer/Layer.h
        Layer/LayerStack.cpp
        Layer/LayerStack.h
        Layer/RasterLayer.cpp
        Layer/RasterLayer.h
        Layer/StrokeLayer.cpp
        Layer/StrokeLayer.h
        Layer/TextLayer.cpp
        Layer/TextLayer.h
        Layer/VectorLayer.cpp
        Layer/VectorLayer.h
        Brush/BrushPreset.cpp
        Brush/BrushPreset.h
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
        Stroke/LiveStroke.cpp
        Stroke/LiveStroke.h
        Stroke/Stabilizer.cpp
        Stroke/Stabilizer.h
        Stroke/StrokePoint.cpp
        Stroke/StrokePoint.h
        Stroke/Stroke.cpp
        Stroke/Stroke.h
        Stroke/StrokeCommand.cpp
        Stroke/StrokeCommand.h
        Stroke/StrokeInput.cpp
        Stroke/StrokeInput.h
        Stroke/StrokeResampler.cpp
        Stroke/StrokeResampler.h
        Stroke/StrokeCurve.cpp
        Stroke/StrokeCurve.h
        Render/Renderer.cpp
        Render/Renderer.h
        Render/RenderContext.cpp
        Render/RenderContext.h
        Render/DirtyRegion.cpp
        Render/DirtyRegion.h
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
        QtAdapter/PaintCanvasItem.cpp
        QtAdapter/PaintCanvasItem.h
        QtAdapter/PaintEngineController.cpp
        QtAdapter/PaintEngineController.h
        QtAdapter/DocumentAdapter.cpp
        QtAdapter/DocumentAdapter.h
        QtAdapter/LayerListModel.cpp
        QtAdapter/LayerListModel.h
        Core/Types.h
        Core/Geometry.h)

add_library(iiPaintEngine SHARED
        ${IIPAINTENGINE_SOURCES})

target_include_directories(iiPaintEngine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(iiPaintEngine PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Quick)

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

add_executable(iiPaintEnginePointerStrokeFlowTests
        tests/PointerStrokeFlowTest.cpp)

target_link_libraries(iiPaintEnginePointerStrokeFlowTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEnginePointerStrokeFlow
        COMMAND iiPaintEnginePointerStrokeFlowTests)

add_executable(iiPaintEngineHybridPaintingModelTests
        tests/HybridPaintingModelTest.cpp)

target_link_libraries(iiPaintEngineHybridPaintingModelTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineHybridPaintingModel
        COMMAND iiPaintEngineHybridPaintingModelTests)

add_executable(iiPaintEngineStrokePhysicalContractTests
        tests/StrokePhysicalContractTest.cpp)

target_link_libraries(iiPaintEngineStrokePhysicalContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineStrokePhysicalContract
        COMMAND iiPaintEngineStrokePhysicalContractTests)

add_executable(iiPaintEngineLiveStrokeRenderingTests
        tests/LiveStrokeRenderingTest.cpp)

target_link_libraries(iiPaintEngineLiveStrokeRenderingTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineLiveStrokeRendering
        COMMAND iiPaintEngineLiveStrokeRenderingTests)

add_executable(iiPaintEngineBrushDynamicsMappingTests
        tests/BrushDynamicsMappingTest.cpp)

target_link_libraries(iiPaintEngineBrushDynamicsMappingTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineBrushDynamicsMapping
        COMMAND iiPaintEngineBrushDynamicsMappingTests)

add_executable(iiPaintEngineStrokeResamplerTests
        tests/StrokeResamplerTest.cpp)

target_link_libraries(iiPaintEngineStrokeResamplerTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineStrokeResampler
        COMMAND iiPaintEngineStrokeResamplerTests)

add_executable(iiPaintEngineStrokeCompositingTests
        tests/StrokeCompositingTest.cpp)

target_link_libraries(iiPaintEngineStrokeCompositingTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineStrokeCompositing
        COMMAND iiPaintEngineStrokeCompositingTests)

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

add_test(NAME iiPaintEngineDependencyBoundary
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/DependencyBoundaryTest.cmake)
