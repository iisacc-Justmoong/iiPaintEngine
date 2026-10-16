cmake_minimum_required(VERSION 3.31)
project(iiPaintEngine VERSION 0.4.2 LANGUAGES CXX)
# File creation is owned by iiFileProvider.
find_package(iiFileProviderNative 0.6 CONFIG REQUIRED HINTS "$ENV{IIFILEPROVIDER_PREFIX}" "$ENV{HOME}/.local/SDK/iiFileProvider")
link_libraries(iiFileProvider::Native)





if(PROJECT_IS_TOP_LEVEL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local/SDK/iiPaintEngine" CACHE PATH "SDK install prefix" FORCE)
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
option(IIPAINTENGINE_BUILD_SHARED "Build a shared library" ON)
option(IIPAINTENGINE_BUILD_EXAMPLES "Build C++ bitmap examples" ON)
option(IIPAINTENGINE_SANITIZE "Instrument C++ sources with address and undefined behavior checks" OFF)
set(IIPAINTENGINE_LIBRARY_TYPE STATIC)
if(IIPAINTENGINE_BUILD_SHARED AND NOT EMSCRIPTEN)
    set(IIPAINTENGINE_LIBRARY_TYPE SHARED)
endif()
file(GLOB_RECURSE IIPAINTENGINE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
if(MSVC)
    set_source_files_properties(src/Brush/LocalizedBrushMath.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
    set_source_files_properties(src/Brush/LocalizedBrushMath.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
endif()
add_library(iiPaintEngine ${IIPAINTENGINE_LIBRARY_TYPE} ${IIPAINTENGINE_SOURCES})
add_library(iiPaintEngine::iiPaintEngine ALIAS iiPaintEngine)
target_compile_features(iiPaintEngine PUBLIC cxx_std_23)
target_include_directories(iiPaintEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine>)
target_include_directories(iiPaintEngine SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb")
set_target_properties(iiPaintEngine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
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

install(TARGETS iiPaintEngine EXPORT iiPaintEngineTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# Replace only the package-owned header tree when upgrading a retired API.
install(CODE "file(REMOVE_RECURSE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine\")")
install(FILES src/iiPaintEngine src/library.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine)
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngine FILES_MATCHING PATTERN "*.h")
install(FILES README.md docs/BRUSHES.md docs/BRUSH_OPTIONS.md docs/CXX23_MIGRATION.md docs/LAYER_COMPOSITING.md docs/LOCAL_MASK_BRUSH.md docs/LOCALIZED_BRUSH_MATH.md docs/localized-brush-math-provenance.json docs/localized-brush-vectors.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/iiPaintEngine)
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
    add_executable(iiPaintEngineExample Example/main.cpp)
    target_link_libraries(iiPaintEngineExample PRIVATE iiPaintEngine)
    set_target_properties(iiPaintEngineExample PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Example/bin")
    add_executable(iiPaintEngineBrushGallery Example/BrushGallery.cpp)
    target_link_libraries(iiPaintEngineBrushGallery PRIVATE iiPaintEngine)
endif()
if(BUILD_TESTING)
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
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/InstallUpgradeContractTest.cmake")
    if(APPLE)
        get_property(names DIRECTORY PROPERTY TESTS)
        set_tests_properties(${names} PROPERTIES ENVIRONMENT_MODIFICATION
            "DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:iiPaintEngine>")
    endif()
endif()
