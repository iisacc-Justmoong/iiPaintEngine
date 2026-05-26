cmake_minimum_required(VERSION 3.31)
project(iiPaintEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg Network)
find_package(LVRS CONFIG REQUIRED)

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
        Core/RasterBlendMode.h
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
        Canvas/Canvas.cpp
        Canvas/Canvas.h
        Canvas/CanvasMetadata.cpp
        Canvas/CanvasMetadata.h
        Canvas/CanvasState.cpp
        Canvas/CanvasState.h
        Canvas/CanvasViewport.cpp
        Canvas/CanvasViewport.h
        Canvas/CanvasSession.cpp
        Canvas/CanvasSession.h
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
        Layer/StrokeLayer.cpp
        Layer/StrokeLayer.h
        Layer/TextLayer.cpp
        Layer/TextLayer.h
        Layer/VectorLayer.cpp
        Layer/VectorLayer.h
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
        Stroke/StrokeRepository.cpp
        Stroke/StrokeRepository.h
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
        Filter/FilterPipeline.cpp
        Filter/FilterPipeline.h
        Tool/RasterEditTool.cpp
        Tool/RasterEditTool.h
        Tool/ToolStateMachine.cpp
        Tool/ToolStateMachine.h
        QtAdapter/PaintCanvasItem.cpp
        QtAdapter/PaintCanvasItem.h
        QtAdapter/CanvasEventWork.cpp
        QtAdapter/CanvasEventWork.h
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

add_library(iiPaintEngine SHARED
        ${IIPAINTENGINE_SOURCES})

target_include_directories(iiPaintEngine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(iiPaintEngine PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick)

set(IIPAINTENGINE_EXAMPLE_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Example/bin")

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

add_executable(iiPaintEngineCanvasDocumentStructureTests
        tests/CanvasDocumentStructureTest.cpp)

target_link_libraries(iiPaintEngineCanvasDocumentStructureTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineCanvasDocumentStructure
        COMMAND iiPaintEngineCanvasDocumentStructureTests)

add_executable(iiPaintEngineDocumentSerializerContractTests
        tests/DocumentSerializerContractTest.cpp)

target_link_libraries(iiPaintEngineDocumentSerializerContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineDocumentSerializerContract
        COMMAND iiPaintEngineDocumentSerializerContractTests)

add_executable(iiPaintEngineCanvasQmlApiTests
        tests/CanvasQmlApiTest.cpp)

target_link_libraries(iiPaintEngineCanvasQmlApiTests PRIVATE
        iiPaintEngine
        Qt6::Qml)

add_test(NAME iiPaintEngineCanvasQmlApi
        COMMAND iiPaintEngineCanvasQmlApiTests)

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
        IIPAINTENGINE_LVRS_QML_IMPORT_PATH="${LVRS_QML_IMPORT_PATH}")

add_test(NAME iiPaintEngineExampleDemoContract
        COMMAND iiPaintEngineExampleDemoContractTests)

add_executable(iiPaintEngineCanvasPointerAlignmentTests
        tests/CanvasPointerAlignmentTest.cpp)

target_link_libraries(iiPaintEngineCanvasPointerAlignmentTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineCanvasPointerAlignment
        COMMAND iiPaintEngineCanvasPointerAlignmentTests)

add_executable(iiPaintEngineCanvasTabletPressureContractTests
        tests/CanvasTabletPressureContractTest.cpp)

target_link_libraries(iiPaintEngineCanvasTabletPressureContractTests PRIVATE
        iiPaintEngine)
if (TARGET Qt6::GuiPrivate)
    target_link_libraries(iiPaintEngineCanvasTabletPressureContractTests PRIVATE
            Qt6::GuiPrivate)
endif ()

add_test(NAME iiPaintEngineCanvasTabletPressureContract
        COMMAND iiPaintEngineCanvasTabletPressureContractTests)

add_executable(iiPaintEngineCanvasLivePreviewRealtimeContractTests
        tests/CanvasLivePreviewRealtimeContractTest.cpp)

target_link_libraries(iiPaintEngineCanvasLivePreviewRealtimeContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineCanvasLivePreviewRealtimeContract
        COMMAND iiPaintEngineCanvasLivePreviewRealtimeContractTests)

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

add_executable(iiPaintEngineStabilizerAdvancedContractTests
        tests/StabilizerAdvancedContractTest.cpp)

target_link_libraries(iiPaintEngineStabilizerAdvancedContractTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineStabilizerAdvancedContract
        COMMAND iiPaintEngineStabilizerAdvancedContractTests)

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

add_executable(iiPaintEngineCanvasEventThreadingTests
        tests/CanvasEventThreadingTest.cpp)

target_link_libraries(iiPaintEngineCanvasEventThreadingTests PRIVATE
        iiPaintEngine)

add_test(NAME iiPaintEngineCanvasEventThreading
        COMMAND iiPaintEngineCanvasEventThreadingTests)

add_test(NAME iiPaintEngineDependencyBoundary
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/DependencyBoundaryTest.cmake)
