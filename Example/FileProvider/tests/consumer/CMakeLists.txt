cmake_minimum_required(VERSION 3.31)
# iisdk:consumer-application
project(iiPaintEngineFileProviderInstalledConsumer LANGUAGES CXX)
include(CTest)
find_package(iiPaintEngineFileProvider 0.5 CONFIG REQUIRED)
add_executable(iiPaintEngineInstalledFileProvider ../file_provider.cpp)
target_link_libraries(iiPaintEngineInstalledFileProvider PRIVATE iiPaintEngine::FileProvider)
add_test(NAME iiPaintEngine.InstalledFileProvider COMMAND iiPaintEngineInstalledFileProvider)
set_tests_properties(iiPaintEngine.InstalledFileProvider PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../resources" TIMEOUT 30
    ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=unset:;DYLD_FALLBACK_LIBRARY_PATH=unset:;DYLD_FRAMEWORK_PATH=unset:;LD_LIBRARY_PATH=unset:")
