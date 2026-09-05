# Architecture boundary guard (pure cmake -P script test; zero compile cost).
#
# Enforces the layering rules from .trellis/spec/XQ/architecture:
#   1. src/core/**            : no external-library includes.
#   2. src/services/**/*.h and src/adapters/**/*.h: public headers include no
#                               external-library headers.
#   3. src/io/**              : no external-library includes.
#   4. src/adapters/**        : must not include "services/..." (adapters depend on
#                               core ports only; this is the P0-1 regression guard).
#   5. Public core/service/io/adapter headers contain no third-party type tokens.
#
# Any hit => FATAL_ERROR listing every offending file:line, so ctest goes red.

if(NOT DEFINED XQ_SOURCE_DIR)
    message(FATAL_ERROR "XQ_SOURCE_DIR must be defined")
endif()

set(_arch_violation_count 0)
set(_arch_report "")

# Scan every file matching the given glob patterns; append "path:line  <text>" to
# _arch_report for each line matching _regex. Runs in the caller's scope so the
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
                string(REGEX REPLACE "//.*$" "" _code_line "${_line}")
                if("${_code_line}" MATCHES "${_regex}")
                    file(RELATIVE_PATH _rel "${XQ_SOURCE_DIR}" "${_f}")
                    string(STRIP "${_line}" _trimmed)
                    math(EXPR _arch_violation_count "${_arch_violation_count} + 1")
                    string(APPEND _arch_report "  ${_rel}:${_lineno}   ${_trimmed}\n")
                endif()
            endforeach()
        endforeach()
    endforeach()
endmacro()

# Rule 1: core has zero external-library includes.
xq_scan_files("#include[ \t]*[\"<](Qt|Q[A-Z]|vtk|itk|gdcm|onnx|mmg|tetgen)"
    "${XQ_SOURCE_DIR}/src/core/*.h"
    "${XQ_SOURCE_DIR}/src/core/*.cpp")

# Rule 2: public service and adapter headers include no external-library headers.
xq_scan_files("#include[ \t]*[\"<](Qt|Q[A-Z]|vtk|itk|gdcm|onnx|mmg|tetgen)"
    "${XQ_SOURCE_DIR}/src/services/*.h"
    "${XQ_SOURCE_DIR}/src/adapters/*.h")

# Rule 3: io has zero external-library includes.
xq_scan_files("#include[ \t]*[\"<](Qt|Q[A-Z]|vtk|itk|gdcm|onnx|mmg|tetgen)"
    "${XQ_SOURCE_DIR}/src/io/*.h"
    "${XQ_SOURCE_DIR}/src/io/*.cpp")

# Rule 4: adapters include core ports only, never services (P0-1 regression guard).
xq_scan_files("#include[ \t]*[\"<]services/"
    "${XQ_SOURCE_DIR}/src/adapters/*.h"
    "${XQ_SOURCE_DIR}/src/adapters/*.cpp")

# Rule 5: all public lower-layer headers expose XQ-owned types only. This
# catches forward declarations as well as includes.
string(CONCAT _xq_external_type_regex
    "(vtk[A-Z][A-Za-z0-9_]*"
    "|itk::"
    "|gdcm::"
    "|(^|[^A-Za-z0-9_])Ort(::|[A-Z])[A-Za-z0-9_]*([^A-Za-z0-9_]|$)"
    "|(^|[^A-Za-z0-9_])ONNX[A-Za-z0-9_]*([^A-Za-z0-9_]|$)"
    "|MMG5_[A-Za-z0-9_]*"
    "|tetgen[A-Za-z0-9_]*"
    "|(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*([^A-Za-z0-9_]|$))"
)
xq_scan_files("${_xq_external_type_regex}"
    "${XQ_SOURCE_DIR}/src/core/*.h"
    "${XQ_SOURCE_DIR}/src/services/*.h"
    "${XQ_SOURCE_DIR}/src/io/*.h"
    "${XQ_SOURCE_DIR}/src/adapters/*.h")

# Rule 6: the Shell-A module ABI is deliberately Path-only. Keep this narrower
# than the general service-header rule so future edits cannot grow a hidden
# Scene/Contour/Flow context while still passing the third-party type guard.
string(CONCAT _xq_path_module_forbidden_type_regex
    "((^|[^A-Za-z0-9_])XQScene([^A-Za-z0-9_]|$)"
    "|(^|[^A-Za-z0-9_])XQContour[A-Za-z0-9_]*([^A-Za-z0-9_]|$)"
    "|(^|[^A-Za-z0-9_])XQFlow[A-Za-z0-9_]*([^A-Za-z0-9_]|$)"
    "|(^|[^A-Za-z0-9_])XQSimulationCase([^A-Za-z0-9_]|$)"
    "|(^|[^A-Za-z0-9_])Flow[A-Z][A-Za-z0-9_]*([^A-Za-z0-9_]|$))"
)
xq_scan_files("${_xq_path_module_forbidden_type_regex}"
    "${XQ_SOURCE_DIR}/src/services/modules/PathModuleRegistry.h")

if(_arch_violation_count GREATER 0)
    message(FATAL_ERROR
        "Architecture boundary violations (${_arch_violation_count}):\n${_arch_report}"
        "See .trellis/spec/XQ/architecture; adapters must depend on core ports only.")
endif()

message(STATUS "Architecture boundaries OK: no forbidden includes found.")
