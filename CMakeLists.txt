cmake_minimum_required(VERSION 3.31)
project(iiPaintEngine VERSION 0.5.4 LANGUAGES CXX)





if(PROJECT_IS_TOP_LEVEL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local/SDK/iiPaintEngine" CACHE PATH "SDK install prefix" FORCE)
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(CMAKE_EXPORT_COMPILE_COMMANDS "Generate a compilation database for developer tooling" OFF)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
option(IIPAINTENGINE_BUILD_SHARED "Build a shared library" ON)
option(IIPAINTENGINE_BUILD_EXAMPLES "Build C++ bitmap examples" OFF)
option(IIPAINTENGINE_SANITIZE "Instrument C++ sources with address and undefined behavior checks" OFF)
option(IIPAINTENGINE_BUILD_BRUSH_TOOLS "Build the brush-authoring component" ON)
set(IIPAINTENGINE_LIBRARY_TYPE STATIC)
if(IIPAINTENGINE_BUILD_SHARED)
    set(IIPAINTENGINE_LIBRARY_TYPE SHARED)
endif()
if(EMSCRIPTEN AND IIPAINTENGINE_BUILD_SHARED)
    set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
endif()
function(iipaint_configure_shared target)
    if(NOT IIPAINTENGINE_BUILD_SHARED)
        return()
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set_target_properties(${target} PROPERTIES FRAMEWORK TRUE FRAMEWORK_VERSION A
            MACOSX_FRAMEWORK_IDENTIFIER "com.iisacc.${target}")
    elseif(APPLE)
        set_property(TARGET ${target} APPEND PROPERTY INSTALL_RPATH "@loader_path")
    elseif(UNIX AND NOT EMSCRIPTEN)
        set_property(TARGET ${target} APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
    if(EMSCRIPTEN)
        target_compile_options(${target} PRIVATE -fPIC -sSIDE_MODULE=1)
        target_link_options(${target} PRIVATE -sSIDE_MODULE=1)
    endif()
endfunction()
file(GLOB_RECURSE IIPAINTENGINE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
# The gesture authoring component is an upper layer, not part of the streaming engine.
list(FILTER IIPAINTENGINE_SOURCES EXCLUDE REGEX "/src/iiBrushTools/")
if(MSVC)
    set_source_files_properties(src/Brush/LocalizedBrushMath.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
    set_source_files_properties(src/Brush/LocalizedBrushMath.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()
add_library(iiPaintEngine ${IIPAINTENGINE_LIBRARY_TYPE} ${IIPAINTENGINE_SOURCES})
iipaint_configure_shared(iiPaintEngine)
add_library(iiPaintEngine::iiPaintEngine ALIAS iiPaintEngine)
target_compile_features(iiPaintEngine PUBLIC cxx_std_23)
target_include_directories(iiPaintEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine>)
target_include_directories(iiPaintEngine SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb")
set_target_properties(iiPaintEngine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON IMPORTED_NO_SYSTEM TRUE)
if(APPLE AND IIPAINTENGINE_BUILD_SHARED)
    set_target_properties(iiPaintEngine PROPERTIES
        INSTALL_NAME_DIR "@rpath"
        BUILD_WITH_INSTALL_NAME_DIR TRUE
        BUILD_WITH_INSTALL_RPATH TRUE)
endif()
if(MSVC)
    target_compile_options(iiPaintEngine PUBLIC /Zc:__cplusplus)
endif()
if(IIPAINTENGINE_SANITIZE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "Sanitizer mode requires Clang or GCC")
    endif()
    target_compile_options(iiPaintEngine PUBLIC -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer)
    target_link_options(iiPaintEngine PUBLIC -fsanitize=address,undefined,float-cast-overflow)
endif()

if(IIPAINTENGINE_BUILD_BRUSH_TOOLS)
    add_library(iiPaintEngineBrushTools ${IIPAINTENGINE_LIBRARY_TYPE} src/iiBrushTools/Controls.cpp src/iiBrushTools/Gesture.cpp src/iiBrushTools/PreviewQueue.cpp src/iiBrushTools/FrozenSource.cpp src/iiBrushTools/FrozenRender.cpp)
    iipaint_configure_shared(iiPaintEngineBrushTools)
    add_library(iiPaintEngine::BrushTools ALIAS iiPaintEngineBrushTools)
    add_library(iiBrushTools::iiBrushTools ALIAS iiPaintEngineBrushTools)
    set_target_properties(iiPaintEngineBrushTools PROPERTIES EXPORT_NAME BrushTools POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF IMPORTED_NO_SYSTEM TRUE WINDOWS_EXPORT_ALL_SYMBOLS ON)
    target_compile_features(iiPaintEngineBrushTools PUBLIC cxx_std_23)
    target_include_directories(iiPaintEngineBrushTools PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine>)
    target_link_libraries(iiPaintEngineBrushTools PUBLIC iiPaintEngine)
    install(TARGETS iiPaintEngineBrushTools EXPORT iiPaintEngineBrushToolsTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES docs/BrushTools-LICENSE.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/iiPaintEngine/BrushTools)
    install(EXPORT iiPaintEngineBrushToolsTargets NAMESPACE iiPaintEngine:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iiPaintEngine)
    configure_package_config_file(cmake/iiBrushToolsConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/iiBrushToolsConfig.cmake INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iiBrushTools)
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/iiBrushToolsConfigVersion.cmake VERSION 0.1.2 COMPATIBILITY SameMinorVersion)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iiBrushToolsConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/iiBrushToolsConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iiBrushTools)
endif()

install(TARGETS iiPaintEngine EXPORT iiPaintEngineTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR})
# Replace only the package-owned header tree when upgrading a retired API.
install(CODE "file(REMOVE_RECURSE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine\")")
install(FILES src/iiPaintEngine src/library.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine)
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine FILES_MATCHING PATTERN "*.h" PATTERN "iiBrushTools" EXCLUDE)
if(IIPAINTENGINE_BUILD_BRUSH_TOOLS)
    install(FILES src/iiBrushTools/Controls.h src/iiBrushTools/Gesture.h src/iiBrushTools/PreviewQueue.h src/iiBrushTools/FrozenSource.h src/iiBrushTools/FrozenRender.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine/iiBrushTools)
endif()
install(FILES README.md docs/BRUSHES.md docs/BRUSH_OPTIONS.md docs/CXX23_MIGRATION.md docs/LAYER_COMPOSITING.md docs/LOCAL_MASK_BRUSH.md docs/LOCALIZED_BRUSH_MATH.md docs/localized-brush-math-provenance.json docs/localized-brush-vectors.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/iiPaintEngine)
install(FILES docs/BRUSH_EXPANSION.md docs/BRUSH_EXPANSION_VALIDATION.md docs/brush-expansion-provenance.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/iiPaintEngine)
install(FILES docs/PERFORMANCE.md DESTINATION ${CMAKE_INSTALL_DATADIR}/iiPaintEngine)
install(FILES docs/SELECTION_RASTER_EDIT.md DESTINATION ${CMAKE_INSTALL_DATADIR}/iiPaintEngine)
install(FILES docs/WET_BRUSH_SAMPLING.md DESTINATION ${CMAKE_INSTALL_DATADIR}/iiPaintEngine)
install(FILES docs/RASTER_HISTORY.md DESTINATION ${CMAKE_INSTALL_DATADIR}/iiPaintEngine)
# Preserve the source documentation layout and its structured agent projections.
# Flat documents above remain for existing installed documentation paths.
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/iiPaintEngine/docs
    FILES_MATCHING PATTERN "*.md" PATTERN "*.json" PATTERN "*.iisacc.json" EXCLUDE
    PATTERN ".*.iisacc-preview" EXCLUDE PATTERN ".iisacc-temporary.lock" EXCLUDE)
install(FILES AGENT.md LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/iiPaintEngine)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/iiPaintEngine)
install(FILES third_party/stb/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/iiPaintEngine/stb)
install(FILES third_party/stb/provenance.json DESTINATION ${CMAKE_INSTALL_DATADIR}/iiPaintEngine)
install(EXPORT iiPaintEngineTargets FILE iiPaintEngineTargets.cmake NAMESPACE iiPaintEngine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iiPaintEngine)
configure_package_config_file(cmake/iiPaintEngineConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iiPaintEngine)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/iiPaintEngine)

if(IIPAINTENGINE_BUILD_EXAMPLES)
    if(IIPAINTENGINE_BUILD_BRUSH_TOOLS)
        add_executable(iiPaintEngineBrushToolsExample Example/BrushTools/main.cpp)
        target_link_libraries(iiPaintEngineBrushToolsExample PRIVATE iiPaintEngine::BrushTools)
    endif()
endif()
if(BUILD_TESTING)
    if(NOT IIPAINTENGINE_BUILD_EXAMPLES)
        add_test(NAME iiPaintEngineDefaultTargets COMMAND ${CMAKE_COMMAND}
            "-DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DEXPECT_COMPILATION_DATABASE=${CMAKE_EXPORT_COMPILE_COMMANDS}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/DefaultTargetsTest.cmake")
    endif()
    if(IIPAINTENGINE_BUILD_BRUSH_TOOLS)
        add_executable(iiPaintEngineBrushToolsTests tests/BrushTools/ContractTest.cpp)
        target_link_libraries(iiPaintEngineBrushToolsTests PRIVATE iiPaintEngine::BrushTools)
        if(IIPAINTENGINE_BUILD_SHARED AND NOT CMAKE_CROSSCOMPILING)
            add_test(NAME iiPaintEngine.BrushTools.shared_type COMMAND "${CMAKE_COMMAND}"
                "-DTARGET_TYPE=$<TARGET_PROPERTY:iiPaintEngineBrushTools,TYPE>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/BrushTools/shared_target_type.cmake")
            add_test(NAME iiPaintEngine.BrushTools.shared_link COMMAND "${CMAKE_COMMAND}"
                "-DIMAGE=$<TARGET_FILE:iiPaintEngineBrushToolsTests>"
                "-DEXPECTED=$<TARGET_FILE:iiPaintEngineBrushTools>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/BrushTools/shared_link.cmake")
        endif()
        foreach(_case Controls Taper Correction Parameters Input Tiles)
            add_test(NAME iiPaintEngine.BrushTools.${_case} COMMAND iiPaintEngineBrushToolsTests ${_case})
        endforeach()
        foreach(_contract PresetContract PresetAvailability PreviewQueueContract PreviewQueueAllocation FrozenSource)
            add_executable(iiPaintEngine${_contract}Tests "tests/BrushTools/${_contract}Test.cpp")
            target_link_libraries(iiPaintEngine${_contract}Tests PRIVATE iiPaintEngine::BrushTools)
            if(_contract STREQUAL "FrozenSource")
                find_package(Threads REQUIRED)
                target_link_libraries(iiPaintEngine${_contract}Tests PRIVATE Threads::Threads)
            endif()
            add_test(NAME iiPaintEngine.BrushTools.${_contract} COMMAND iiPaintEngine${_contract}Tests)
        endforeach()
        add_test(NAME iiPaintEngine.BrushTools.installed_consumer COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DCONFIG=$<CONFIG>" "-DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/BrushTools/installed.cmake")
        set_tests_properties(iiPaintEngine.BrushTools.installed_consumer PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
    endif()
    file(GLOB IIPAINTENGINE_TEST_SOURCES CONFIGURE_DEPENDS "tests/*Test.cpp")
    foreach(source IN LISTS IIPAINTENGINE_TEST_SOURCES)
        get_filename_component(name "${source}" NAME_WE)
        string(REGEX REPLACE "Test$" "" name "${name}")
        set(target "iiPaintEngine${name}Tests")
        add_executable(${target} "${source}")
        target_link_libraries(${target} PRIVATE iiPaintEngine)
        target_compile_definitions(${target} PRIVATE
            IIPAINTENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            IIPAINTENGINE_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME "iiPaintEngine${name}" COMMAND ${target})
    endforeach()
    add_test(NAME iiPaintEngineDependencyBoundary COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/DependencyBoundaryTest.cmake")
    add_test(NAME iiPaintEngineInstallUpgradeContract COMMAND ${CMAKE_COMMAND}
        "-DIIPAINTENGINE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DIIPAINTENGINE_INSTALL_CONFIG=$<CONFIG>"
        "-DIIPAINTENGINE_GENERATOR=${CMAKE_GENERATOR}"
        "-DIIPAINTENGINE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DIIPAINTENGINE_PACKAGE_VERSION=${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/InstallUpgradeContractTest.cmake")
    if(APPLE)
        get_property(names DIRECTORY PROPERTY TESTS)
        set_tests_properties(${names} PROPERTIES ENVIRONMENT_MODIFICATION
            "DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:iiPaintEngine>")
    endif()
endif()
