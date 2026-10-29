cmake_minimum_required(VERSION 3.31)
# iisdk:consumer-application
project(iiPaintEngineFileProvider VERSION 0.5.4 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
find_package(iiPaintEngine 0.5 CONFIG REQUIRED)
find_package(iiFileProviderNative 0.8 CONFIG REQUIRED)
add_library(iiPaintEngineFileAdapter INTERFACE)
add_library(iiPaintEngine::FileProvider ALIAS iiPaintEngineFileAdapter)
set_target_properties(iiPaintEngineFileAdapter PROPERTIES EXPORT_NAME FileProvider)
target_compile_features(iiPaintEngineFileAdapter INTERFACE cxx_std_23)
target_include_directories(iiPaintEngineFileAdapter INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/App>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngineFileProvider>)
target_link_libraries(iiPaintEngineFileAdapter INTERFACE iiPaintEngine::iiPaintEngine iiFileProvider::Native)
add_executable(iiPaintEngineExample App/main.cpp)
target_link_libraries(iiPaintEngineExample PRIVATE iiPaintEngineFileAdapter)
add_executable(iiPaintEngineBrushGallery App/BrushGallery.cpp)
target_link_libraries(iiPaintEngineBrushGallery PRIVATE iiPaintEngineFileAdapter)
add_executable(iiPaintEngineBrushExpansionGallery App/BrushExpansionGallery.cpp)
target_link_libraries(iiPaintEngineBrushExpansionGallery PRIVATE iiPaintEngineFileAdapter)
if(BUILD_TESTING)
    add_executable(iiPaintEngineFileProviderTests tests/file_provider.cpp)
    target_link_libraries(iiPaintEngineFileProviderTests PRIVATE iiPaintEngineFileAdapter)
    add_test(NAME iiPaintEngine.FileProvider COMMAND iiPaintEngineFileProviderTests)
    # An older globally installed engine may have the same dylib/soname but a
    # different ABI. Exercise the exact packages selected by find_package.
    set_tests_properties(iiPaintEngine.FileProvider PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources" TIMEOUT 30
        ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=unset:;DYLD_FALLBACK_LIBRARY_PATH=unset:;DYLD_FRAMEWORK_PATH=unset:;LD_LIBRARY_PATH=unset:")
endif()
set(package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/iiPaintEngineFileProvider")
install(TARGETS iiPaintEngineFileAdapter EXPORT iiPaintEngineFileProviderTargets)
install(EXPORT iiPaintEngineFileProviderTargets NAMESPACE iiPaintEngine:: DESTINATION "${package_dir}")
install(FILES App/FileProviderBitmapIO.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iiPaintEngineFileProvider)
configure_package_config_file(cmake/iiPaintEngineFileProviderConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineFileProviderConfig.cmake" INSTALL_DESTINATION "${package_dir}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineFileProviderConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineFileProviderConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/iiPaintEngineFileProviderConfigVersion.cmake" DESTINATION "${package_dir}")
