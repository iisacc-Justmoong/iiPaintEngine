# Isolate the install from all existing SDK and user data.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(root "${BINARY_DIR}/brush-tools-consumer-${suffix}")
set(prefix "${root}/prefix")
set(consumer "${root}/consumer")
function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Installed brush component check failed: ${ARGV}")
    endif()
endfunction()
run_checked("${CMAKE_COMMAND}" --install "${BINARY_DIR}" --config "${CONFIG}" --prefix "${prefix}")
run_checked("${CMAKE_COMMAND}" -S "${SOURCE_DIR}/tests/BrushTools/consumer" -B "${consumer}"
    "-DCMAKE_PREFIX_PATH=${prefix}" "-DCMAKE_DISABLE_FIND_PACKAGE_iiFileProviderNative=ON" "-DCMAKE_BUILD_TYPE=${CONFIG}")
run_checked("${CMAKE_COMMAND}" --build "${consumer}" --config "${CONFIG}" --parallel 2)
run_checked("${CTEST_COMMAND}" --test-dir "${consumer}" -C "${CONFIG}" --output-on-failure)
# Only this test's randomly named staging and build outputs are removed.
file(REMOVE_RECURSE "${root}")
