#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "iiPaintEngine::iiPaintEngine" for configuration "Release"
set_property(TARGET iiPaintEngine::iiPaintEngine APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(iiPaintEngine::iiPaintEngine PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libiiPaintEngine.dylib"
  IMPORTED_SONAME_RELEASE "@rpath/libiiPaintEngine.dylib"
  )

list(APPEND _cmake_import_check_targets iiPaintEngine::iiPaintEngine )
list(APPEND _cmake_import_check_files_for_iiPaintEngine::iiPaintEngine "${_IMPORT_PREFIX}/lib/libiiPaintEngine.dylib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
