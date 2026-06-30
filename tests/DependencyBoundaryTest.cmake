set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(MODULES Core Canvas Input Document Layer Stroke Brush Render History Color Selection Transform Filter Tool QtAdapter)

function(module_for path out_var)
    string(REGEX MATCH "^[^/]+" module "${path}")
    if (module IN_LIST MODULES)
        set(${out_var} "${module}" PARENT_SCOPE)
    else ()
        set(${out_var} "" PARENT_SCOPE)
    endif ()
endfunction()

function(allowed_modules_for source_module out_var)
    if (source_module STREQUAL "Core")
        set(allowed "")
    elseif (source_module STREQUAL "Layer"
            OR source_module STREQUAL "Brush"
            OR source_module STREQUAL "History"
            OR source_module STREQUAL "Color"
            OR source_module STREQUAL "Selection")
        set(allowed Core)
    elseif (source_module STREQUAL "Render")
        set(allowed Core Layer Color)
    elseif (source_module STREQUAL "Transform")
        set(allowed Core Layer Selection)
    elseif (source_module STREQUAL "Filter")
        set(allowed Core Layer Selection)
    elseif (source_module STREQUAL "Tool")
        set(allowed Core Layer Selection Transform Filter)
    elseif (source_module STREQUAL "Stroke")
        set(allowed Core Brush)
    elseif (source_module STREQUAL "Document")
        set(allowed Core Canvas)
    elseif (source_module STREQUAL "Canvas")
        set(allowed Core Layer Stroke)
    elseif (source_module STREQUAL "Input")
        set(allowed Core Stroke)
    elseif (source_module STREQUAL "QtAdapter")
        set(allowed Core Canvas Input Document Layer Stroke Brush Render History Color Selection Transform Filter Tool)
    else ()
        set(allowed "")
    endif ()

    set(${out_var} "${allowed}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE source_files
        RELATIVE "${ROOT}"
        "${ROOT}/*.h"
        "${ROOT}/*.cpp")

foreach (source_file IN LISTS source_files)
    if (source_file MATCHES "^(build|cmake-build-debug|tests)/")
        continue()
    endif ()

    module_for("${source_file}" source_module)
    if (NOT source_module)
        continue()
    endif ()

    file(READ "${ROOT}/${source_file}" content)

    if (NOT source_module STREQUAL "QtAdapter")
        if (content MATCHES "#include[ \t]*<Q[A-Za-z0-9_/\\.]+>"
                OR content MATCHES "\\bQ_OBJECT\\b"
                OR content MATCHES "\\bQObject\\b"
                OR content MATCHES "\\bQQuickItem\\b"
                OR content MATCHES "\\bQQuickPaintedItem\\b"
                OR content MATCHES "\\bQPainter\\b")
            message(FATAL_ERROR "${source_file} must not depend on Qt object or QML headers")
        endif ()
    endif ()

    if (source_module STREQUAL "QtAdapter"
            AND NOT source_file MATCHES "^QtAdapter/PaintCanvasItem\\.(h|cpp)$")
        if (content MATCHES "\\bQQuickPaintedItem\\b"
                OR content MATCHES "\\bQPainter\\b")
            message(FATAL_ERROR "${source_file} must not depend on the painted-item renderer boundary")
        endif ()
    endif ()

    allowed_modules_for("${source_module}" allowed_modules)
    if (source_file MATCHES "^Document/DocumentSerializer\\.(h|cpp)$")
        set(allowed_modules Core Canvas Brush Color History)
    endif ()

    string(REGEX MATCHALL "#include[ \t]*\"[^\"]+\"" includes "${content}")

    foreach (include_line IN LISTS includes)
        string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" include_path "${include_line}")
        module_for("${include_path}" included_module)

        if (NOT included_module OR included_module STREQUAL source_module)
            continue()
        endif ()

        if (NOT included_module IN_LIST allowed_modules)
            message(FATAL_ERROR
                    "${source_file} must not include ${include_path}; ${source_module} may only depend on ${allowed_modules}")
        endif ()
    endforeach ()
endforeach ()
