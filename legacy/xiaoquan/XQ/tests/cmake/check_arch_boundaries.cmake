# Architecture boundary guard (pure cmake -P script test; zero compile cost).
#
# Enforces the layering rules from .trellis/spec/XQ/architecture:
#   1. src/core/**            : no external-library includes (Qt/vtk/itk/gdcm/onnx).
#   2. src/services/**/*.h    : public service headers leak no external-library
#                               types (vtk/itk/gdcm/onnx).
#   3. src/io/**              : no external-library includes (Qt/vtk/itk/gdcm/onnx).
#   4. src/adapters/**        : must not include "services/..." (adapters depend on
#                               core ports only; this is the P0-1 regression guard).
#
# Any hit => FATAL_ERROR listing every offending file:line, so ctest goes red.

if(NOT DEFINED XQ_SOURCE_DIR)
    message(FATAL_ERROR "XQ_SOURCE_DIR must be defined")
endif()

set(_arch_violations "")

# Scan every file matching the given glob patterns; append "path:line  <text>" to
# _arch_violations for each line matching _regex. Runs in the caller's scope so the
# accumulator survives across calls.
macro(xq_scan_files _regex)
    foreach(_pat IN ITEMS ${ARGN})
        file(GLOB_RECURSE _matched "${_pat}")
        foreach(_f IN LISTS _matched)
            file(READ "${_f}" _content)
            # Protect literal semicolons so the split into a line-list is faithful.
            string(REPLACE ";" "\\;" _content "${_content}")
            string(REGEX REPLACE "\r?\n" ";" _lines "${_content}")
            set(_lineno 0)
            foreach(_line IN LISTS _lines)
                math(EXPR _lineno "${_lineno} + 1")
                if("${_line}" MATCHES "${_regex}")
                    file(RELATIVE_PATH _rel "${XQ_SOURCE_DIR}" "${_f}")
                    string(STRIP "${_line}" _trimmed)
                    list(APPEND _arch_violations "  ${_rel}:${_lineno}   ${_trimmed}")
                endif()
            endforeach()
        endforeach()
    endforeach()
endmacro()

# Rule 1: core has zero external-library includes.
xq_scan_files("#include[ \t]*<(Qt|vtk|itk|gdcm|onnx)"
    "${XQ_SOURCE_DIR}/src/core/*.h"
    "${XQ_SOURCE_DIR}/src/core/*.cpp")

# Rule 2: public service headers leak no external-library types.
xq_scan_files("#include[ \t]*<(vtk|itk|gdcm|onnx)"
    "${XQ_SOURCE_DIR}/src/services/*.h")

# Rule 3: io has zero external-library includes.
xq_scan_files("#include[ \t]*<(Qt|vtk|itk|gdcm|onnx)"
    "${XQ_SOURCE_DIR}/src/io/*.h"
    "${XQ_SOURCE_DIR}/src/io/*.cpp")

# Rule 4: adapters include core ports only, never services (P0-1 regression guard).
xq_scan_files("#include[ \t]*[\"<]services/"
    "${XQ_SOURCE_DIR}/src/adapters/*.h"
    "${XQ_SOURCE_DIR}/src/adapters/*.cpp")

if(_arch_violations)
    list(LENGTH _arch_violations _n)
    string(REPLACE ";" "\n" _report "${_arch_violations}")
    message(FATAL_ERROR
        "Architecture boundary violations (${_n}):\n${_report}\n"
        "See .trellis/spec/XQ/architecture; adapters must depend on core ports only.")
endif()

message(STATUS "Architecture boundaries OK: no forbidden includes found.")
