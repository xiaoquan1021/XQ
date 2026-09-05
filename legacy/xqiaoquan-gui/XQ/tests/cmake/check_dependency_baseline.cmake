# Dependency-baseline regression guard (pure cmake -P script test).
#
# Runtime/link closure is verified by the remediation build scripts. This fast
# source guard prevents the known broad-discovery and licensing regressions from
# returning unnoticed between full dependency audits.

if(NOT DEFINED XQ_SOURCE_DIR)
    message(FATAL_ERROR "XQ_SOURCE_DIR must be defined")
endif()

file(READ "${XQ_SOURCE_DIR}/CMakeLists.txt" _cmake)

set(_required_patterns
    "find_package\\(Qt6 6\\.7\\.0 EXACT CONFIG REQUIRED COMPONENTS Core Widgets\\)"
    "find_package\\(VTK 9\\.3\\.0 EXACT CONFIG REQUIRED COMPONENTS"
    "find_package\\(ITK 5\\.4\\.0 EXACT CONFIG REQUIRED COMPONENTS"
    "CMAKE_MSVC_RUNTIME_LIBRARY"
    "xq_validate_explicit_package_dir"
    "set\\(_xq_product_vtk_libraries \\$\\{VTK_LIBRARIES\\}\\)"
    "set\\(VTK_LIBRARIES \\$\\{_xq_product_vtk_libraries\\}\\)"
    "ITKLevelSets_LIBRARIES"
    "ITKImageGradient_LIBRARIES"
    "ITKImageIntensity_LIBRARIES"
    "XQ_ACKNOWLEDGE_TETGEN_RESEARCH_ONLY"
)
foreach(_pattern IN LISTS _required_patterns)
    if(NOT _cmake MATCHES "${_pattern}")
        message(FATAL_ERROR
            "Dependency baseline is missing required contract: ${_pattern}")
    endif()
endforeach()

if(_cmake MATCHES "include\\(\\$\\{ITK_USE_FILE\\}\\)")
    message(FATAL_ERROR "Product CMake must not use the directory-wide ITK_USE_FILE")
endif()
if(_cmake MATCHES "\\$\\{ITK_LIBRARIES\\}")
    message(FATAL_ERROR "Product CMake must not link the package-wide ITK_LIBRARIES list")
endif()
if(_cmake MATCHES "itk-tubetk-install|ITKTubeTK-XQ-probe")
    message(FATAL_ERROR
        "Product CMake must not consume a research TubeTK prefix or probe")
endif()
if(_cmake MATCHES "xq_itk_tubetk_ridge|tubeSegmentTubes|tubeConvertTubesToImage|tubeResampleImage")
    message(FATAL_ERROR
        "Automatic v3 product CMake must not build or link TubeTK")
endif()

foreach(_source IN ITEMS
        "${XQ_SOURCE_DIR}/third_party/tetgen/README.simvascular"
        "${XQ_SOURCE_DIR}/src/adapters/tetgen/TetGenTetMesher.h")
    file(READ "${_source}" _tetgen_text)
    if(_tetgen_text MATCHES "TetGen 1\\.5\\.1|tetgen 1\\.5\\.1")
        message(FATAL_ERROR
            "TetGen source comments must use the audited self-identity 1.5: ${_source}")
    endif()
endforeach()

function(xq_read_canonical_contract relative_path output_variable)
    set(_path "${XQ_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "Canonical Shell A contract file is missing: ${_path}")
    endif()
    file(READ "${_path}" _text)
    string(REPLACE "\\" "/" _text "${_text}")
    string(TOLOWER "${_text}" _text)
    string(REGEX REPLACE
        "(^|\n)[ \t]*(rem[ \t]+|::)[^\n]*"
        "\\1" _text "${_text}")
    set(${output_variable} "${_text}" PARENT_SCOPE)
endfunction()

function(xq_require_canonical_literal label text literal)
    string(FIND "${text}" "${literal}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR
            "Canonical Shell A ${label} is missing required contract: ${literal}")
    endif()
endfunction()

function(xq_forbid_canonical_literal label text literal)
    string(FIND "${text}" "${literal}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Canonical Shell A ${label} contains forbidden contract: ${literal}")
    endif()
endfunction()

xq_read_canonical_contract(
    "probes/vascular_foundation/canonical_shell_env.bat" _canonical_env)
xq_read_canonical_contract(
    "probes/vascular_foundation/configure_canonical_shell.bat" _canonical_configure)
xq_read_canonical_contract("build_gui_wt.bat" _canonical_on_wrapper)
xq_read_canonical_contract("build_shell_noflow_wt.bat" _canonical_off_wrapper)
xq_read_canonical_contract("run_xq.bat" _canonical_run)
xq_read_canonical_contract("verify_canonical_shell_a.bat" _canonical_verify)
xq_read_canonical_contract(
    "probes/vascular_foundation/CANONICAL_SHELL_A.md" _canonical_docs)
xq_read_canonical_contract(
    "probes/vascular_foundation/check_dependency_baseline.ps1" _canonical_checker)

set(_canonical_gitignore_path "${XQ_SOURCE_DIR}/../.gitignore")
if(NOT EXISTS "${_canonical_gitignore_path}")
    message(FATAL_ERROR "Repository .gitignore is missing: ${_canonical_gitignore_path}")
endif()
file(READ "${_canonical_gitignore_path}" _canonical_gitignore)
string(REPLACE "\\" "/" _canonical_gitignore "${_canonical_gitignore}")
string(TOLOWER "${_canonical_gitignore}" _canonical_gitignore)
foreach(_literal IN ITEMS
        "!/xq/build_gui_wt.bat"
        "!/xq/build_shell_noflow_wt.bat"
        "!/xq/run_xq.bat")
    xq_require_canonical_literal(
        "repository ignore policy" "${_canonical_gitignore}" "${_literal}")
endforeach()
foreach(_literal IN ITEMS
        "if not exist \"%xq_canonical_test_data_root%"
        "if not exist \"%xq_canonical_qt_build_dir%"
        "if not exist \"%xq_canonical_legacy_qt_root%"
        "if not exist \"%xq_canonical_python_root%"
        "if not exist \"%xq_canonical_python_exe%")
    xq_forbid_canonical_literal(
        "shared runtime environment" "${_canonical_env}" "${_literal}")
endforeach()

foreach(_literal IN ITEMS
        "xq_canonical_qt_platform=windows-x64-vascular"
        "%xq_canonical_qt_platform%/qt-6.7.0"
        "windows-x64/vtk-9.3.0"
        "windows-x64/itk-5.4.0"
        "windows-x64/gdcm-3.0.10"
        "windows-x64/hdf5-1.14.3"
        "windows-x64/tinyxml2-8.0.0"
        "windows-x64/python-3.11.0"
        "build_shell_a_on"
        "build_shell_a_off"
        "if /i \"%xq_canonical_build_on%\"==\"%xq_canonical_build_off%\""
        "xq_canonical_prefix_path=%xq_canonical_qt_root%;%xq_canonical_gdcm_root%;%xq_canonical_hdf5_root%;%xq_canonical_itk_root%;%xq_canonical_tinyxml2_root%;%xq_canonical_vtk_root%"
        "xq_canonical_app_exe=%xq_canonical_build_on%/xq_app.exe")
    xq_require_canonical_literal("environment" "${_canonical_env}" "${_literal}")
endforeach()

string(REGEX MATCH
    "set \"xq_canonical_prefix_path=[^\r\n]*\""
    _canonical_prefix_assignment "${_canonical_env}")
if(_canonical_prefix_assignment STREQUAL "")
    message(FATAL_ERROR "Canonical Shell A environment has no prefix assignment")
endif()
if(_canonical_prefix_assignment MATCHES "install/windows-x64(;|\")")
    message(FATAL_ERROR
        "Canonical Shell A CMAKE_PREFIX_PATH contains the aggregate install/windows-x64 root")
endif()

foreach(_literal IN ITEMS
        "canonical_shell_env.bat"
        "--fresh"
        "set \"cmake_prefix_path=\""
        "-dcmake_find_use_package_registry=false"
        "-dcmake_find_use_system_package_registry=false"
        "-dcmake_prefix_path=%xq_canonical_prefix_path%"
        "-dqt6_dir=%xq_canonical_qt_dir%"
        "-dvtk_dir=%xq_canonical_vtk_dir%"
        "-ditk_dir=%xq_canonical_itk_dir%"
        "-dtinyxml2_dir=%xq_canonical_tinyxml2_dir%"
        "-dpython3_root_dir=%xq_canonical_python_root%"
        "-dpython3_executable=%xq_canonical_python_exe%"
        "-dpython3_include_dir=%xq_canonical_python_include%"
        "-dpython3_library=%xq_canonical_python_library%"
        "-dxq_test_data_root=%xq_canonical_test_data_root%"
        "-dxq_enable_flow=%xq_canonical_flow_mode%"
        "-dxq_enable_tetgen=off"
        "-dxq_acknowledge_tetgen_research_only=off"
        "-dxq_enable_mmg=off"
        "xq_canonical_build_dir=%xq_canonical_build_on%"
        "xq_canonical_build_dir=%xq_canonical_build_off%"
        "-builddir \"%xq_canonical_qt_build_dir%\""
        "-qtroot \"%xq_canonical_qt_root%\""
        "-legacyqtroot \"%xq_canonical_legacy_qt_root%\""
        "-vtkroot \"%xq_canonical_vtk_root%\""
        "-itkroot \"%xq_canonical_itk_root%\""
        "-gdcmroot \"%xq_canonical_gdcm_root%\""
        "-hdf5root \"%xq_canonical_hdf5_root%\""
        "-pythonroot \"%xq_canonical_python_root%\""
        "-tinyxml2root \"%xq_canonical_tinyxml2_root%\""
        "gdcmconfig.cmake"
        "gdcmconfigversion.cmake"
        "hdf5-config.cmake"
        "hdf5-config-version.cmake"
        "--clean-first"
        "check_isolated_qtbase.ps1"
        "check_dependency_baseline.ps1")
    xq_require_canonical_literal("configure helper" "${_canonical_configure}" "${_literal}")
endforeach()
xq_forbid_canonical_literal(
    "configure helper" "${_canonical_configure}" "c:/software/anaconda")
xq_forbid_canonical_literal(
    "configure helper" "${_canonical_configure}" "install/windows-x64;")

xq_require_canonical_literal(
    "Flow ON wrapper" "${_canonical_on_wrapper}" "configure_canonical_shell.bat\" on")
xq_require_canonical_literal(
    "Flow OFF wrapper" "${_canonical_off_wrapper}" "configure_canonical_shell.bat\" off")
foreach(_wrapper IN ITEMS _canonical_on_wrapper _canonical_off_wrapper)
    xq_forbid_canonical_literal("public build wrapper" "${${_wrapper}}" "cmake.exe")
    xq_forbid_canonical_literal("public build wrapper" "${${_wrapper}}" "vcvars64.bat")
    xq_forbid_canonical_literal("public build wrapper" "${${_wrapper}}" "qt-6.7.0")
    xq_forbid_canonical_literal("public build wrapper" "${${_wrapper}}" "c:/software/anaconda")
endforeach()

foreach(_literal IN ITEMS
        "canonical_shell_env.bat"
        "xq_canonical_app_exe"
        "path=%xq_canonical_runtime_path%;%systemroot%/system32;%systemroot%;%systemroot%/system32/wbem"
        "qt_plugin_path=%xq_canonical_qt_plugin_path%"
        "pythonhome="
        "pythonpath="
        "conda_prefix="
        "virtual_env="
        "xq_canonical_python_root="
        "xq_canonical_python_exe="
        "xq_canonical_python_include="
        "xq_canonical_python_library="
        "xq_canonical_qpa_platform=windows"
        "%*")
    xq_require_canonical_literal("GUI run wrapper" "${_canonical_run}" "${_literal}")
endforeach()

foreach(_literal IN ITEMS
        "itkgdcm.cmake"
        "itkhdf5.cmake"
        "itk embedded gdcm_dir"
        "itk embedded hdf5_dir"
        "cmake_prefix_path[$index]"
        "expected the six locked roots"
        "cmake user package registry must be disabled"
        "cmake system package registry must be disabled"
        "exact version 3.0.10"
        "exact version 1.14.3")
    xq_require_canonical_literal(
        "dependency checker" "${_canonical_checker}" "${_literal}")
endforeach()
foreach(_literal IN ITEMS
        "%path%"
        "build_gui/xq_app.exe"
        "build_shell_noflow"
        "install/windows-x64/qt-6.7.0"
        "c:/software/anaconda"
        "mmg-5.3.9"
        "python-3.11.0")
    xq_forbid_canonical_literal("GUI run wrapper" "${_canonical_run}" "${_literal}")
endforeach()

foreach(_literal IN ITEMS
        "00-source-contract.log"
        "metadata.txt"
        "sample_id=%xq_canonical_sample_id%"
        "refusing to overwrite an existing evidence run"
        "01-flow-on-build.log"
        "03-flow-on-full.log"
        "04-flow-off-build.log"
        "06-flow-off-full.log"
        "07-negative-probes.log"
        "configure_canonical_shell.bat\" on"
        "configure_canonical_shell.bat\" off"
        "run_dependency_negative_probes.ps1"
        "ctest_parallel_level=1"
        "manual gui and requirement acceptance are still pending")
    xq_require_canonical_literal("verification entry" "${_canonical_verify}" "${_literal}")
endforeach()

set(_canonical_sequence)
foreach(_literal IN ITEMS
        "01-flow-on-build.log"
        "03-flow-on-full.log"
        "04-flow-off-build.log"
        "06-flow-off-full.log"
        "07-negative-probes.log")
    string(FIND "${_canonical_verify}" "${_literal}" _position)
    list(APPEND _canonical_sequence "${_position}")
endforeach()
list(GET _canonical_sequence 0 _on_build_position)
list(GET _canonical_sequence 1 _on_full_position)
list(GET _canonical_sequence 2 _off_build_position)
list(GET _canonical_sequence 3 _off_full_position)
list(GET _canonical_sequence 4 _negative_position)
if(NOT (_on_build_position LESS _on_full_position AND
        _on_full_position LESS _off_build_position AND
        _off_build_position LESS _off_full_position AND
        _off_full_position LESS _negative_position))
    message(FATAL_ERROR
        "Canonical Shell A verification must run Flow ON, then Flow OFF, then negative probes")
endif()

xq_require_canonical_literal(
    "documentation" "${_canonical_docs}" "pending manual acceptance")
xq_require_canonical_literal(
    "documentation" "${_canonical_docs}" "automated green output is evidence only")
xq_require_canonical_literal(
    "documentation" "${_canonical_docs}" "required manual review")
xq_require_canonical_literal(
    "documentation" "${_canonical_docs}" "cannot be copied or moved")

message(STATUS "Dependency baseline source contract OK")
