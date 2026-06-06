#-----------------------------------------------------------------------------
# XQ External Dependencies Configuration
# Sets paths for all external libraries installed in the Externals entry repo.
#-----------------------------------------------------------------------------

get_filename_component(_xq_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT DEFINED XQ_EXTERNALS_PLATFORM)
  if(WIN32)
    set(XQ_EXTERNALS_PLATFORM "windows-x64")
  else()
    set(XQ_EXTERNALS_PLATFORM "")
  endif()
endif()
set(XQ_EXTERNALS_PLATFORM "${XQ_EXTERNALS_PLATFORM}" CACHE STRING "Optional platform suffix under <externals-root>/install")

function(_xq_externals_install_dir root out_var)
  if(DEFINED XQ_EXTERNALS_PLATFORM AND NOT "${XQ_EXTERNALS_PLATFORM}" STREQUAL "")
    set(_xq_platform_install "${root}/install/${XQ_EXTERNALS_PLATFORM}")
    if(EXISTS "${_xq_platform_install}")
      set(${out_var} "${_xq_platform_install}" PARENT_SCOPE)
      return()
    endif()
  endif()

  set(${out_var} "${root}/install" PARENT_SCOPE)
endfunction()

function(_xq_mitk_config_candidates root install_dir out_var)
  set(_xq_candidates)

  if(DEFINED XQ_MITK_BUILD_DIR AND NOT "${XQ_MITK_BUILD_DIR}" STREQUAL "")
    list(APPEND _xq_candidates "${XQ_MITK_BUILD_DIR}/MITKConfig.cmake")
  endif()

  if(DEFINED XQ_EXTERNALS_PLATFORM AND NOT "${XQ_EXTERNALS_PLATFORM}" STREQUAL "")
    list(APPEND _xq_candidates
      "${root}/build/${XQ_EXTERNALS_PLATFORM}/MITK/MITK-build/MITKConfig.cmake"
      "${root}/src/MITK-2024.06/build/${XQ_EXTERNALS_PLATFORM}/MITK-build/MITKConfig.cmake"
    )
  endif()

  list(APPEND _xq_candidates
    "${root}/src/MITK-2024.06/build/MITK-build/MITKConfig.cmake"
    "${install_dir}/mitk-2024.06/MITKConfig.cmake"
    "${install_dir}/mitk-2024.06/lib/cmake/MITK/MITKConfig.cmake"
  )

  set(${out_var} ${_xq_candidates} PARENT_SCOPE)
endfunction()

function(_xq_find_mitk_config root install_dir out_var)
  _xq_mitk_config_candidates("${root}" "${install_dir}" _xq_candidates)
  foreach(_xq_candidate IN LISTS _xq_candidates)
    if(EXISTS "${_xq_candidate}")
      get_filename_component(_xq_mitk_dir "${_xq_candidate}" DIRECTORY)
      set(${out_var} "${_xq_mitk_dir}" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  set(${out_var} "" PARENT_SCOPE)
endfunction()

function(_xq_externals_required_files root out_var)
  _xq_externals_install_dir("${root}" _xq_install_dir)
  set(_xq_required_files
    "${_xq_install_dir}/qt-6.7.0/lib/cmake/Qt6/Qt6Config.cmake"
    "${_xq_install_dir}/vtk-9.3.0/lib/cmake/vtk-9.3/vtk-config.cmake"
    "${_xq_install_dir}/itk-5.4.0/lib/cmake/ITK-5.4/ITKConfig.cmake"
  )
  set(${out_var} ${_xq_required_files} PARENT_SCOPE)
endfunction()

function(_xq_is_valid_externals_root root out_var)
  _xq_externals_install_dir("${root}" _xq_install_dir)
  _xq_externals_required_files("${root}" _xq_required_files)
  foreach(_xq_required_file IN LISTS _xq_required_files)
    if(NOT EXISTS "${_xq_required_file}")
      set(${out_var} FALSE PARENT_SCOPE)
      return()
    endif()
  endforeach()

  _xq_find_mitk_config("${root}" "${_xq_install_dir}" _xq_mitk_dir)
  if("${_xq_mitk_dir}" STREQUAL "")
    set(${out_var} FALSE PARENT_SCOPE)
    return()
  endif()

  set(${out_var} TRUE PARENT_SCOPE)
endfunction()

if(DEFINED XQ_EXTERNALS_ROOT AND NOT "${XQ_EXTERNALS_ROOT}" STREQUAL "")
  set(_xq_externals_candidates "${XQ_EXTERNALS_ROOT}")
elseif(DEFINED ENV{XQ_EXTERNALS_ROOT} AND NOT "$ENV{XQ_EXTERNALS_ROOT}" STREQUAL "")
  set(_xq_externals_candidates "$ENV{XQ_EXTERNALS_ROOT}")
else()
  set(_xq_externals_candidates
    "${_xq_repo_root}/../Externals"
    "${_xq_repo_root}/../svExternals"
    "${_xq_repo_root}/../External"
  )
  if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    list(APPEND _xq_externals_candidates
      "$ENV{HOME}/Externals"
      "$ENV{HOME}/svExternals"
      "$ENV{HOME}/External"
      "$ENV{HOME}/Simvascular/Externals"
    )
  endif()
  if(WIN32 AND DEFINED ENV{USERPROFILE} AND NOT "$ENV{USERPROFILE}" STREQUAL "")
    list(APPEND _xq_externals_candidates
      "$ENV{USERPROFILE}/Externals"
      "$ENV{USERPROFILE}/Simvascular/Externals"
    )
  endif()
endif()

set(_xq_default_externals_root "")
foreach(_xq_candidate IN LISTS _xq_externals_candidates)
  _xq_is_valid_externals_root("${_xq_candidate}" _xq_candidate_valid)
  if(_xq_candidate_valid)
    get_filename_component(_xq_default_externals_root "${_xq_candidate}" ABSOLUTE)
    break()
  endif()
endforeach()

if("${_xq_default_externals_root}" STREQUAL "")
  set(_xq_error_message "Missing required XQ external dependency files.\nChecked candidate roots:")
  foreach(_xq_candidate IN LISTS _xq_externals_candidates)
    string(APPEND _xq_error_message "\n  - ${_xq_candidate}")
  endforeach()
  _xq_externals_required_files("<externals-root>" _xq_expected_files)
  string(APPEND _xq_error_message "\nExpected files under a valid root:")
  foreach(_xq_expected_file IN LISTS _xq_expected_files)
    string(APPEND _xq_error_message "\n  - ${_xq_expected_file}")
  endforeach()
  _xq_mitk_config_candidates("<externals-root>" "<externals-install>" _xq_mitk_candidates)
  string(APPEND _xq_error_message "\nExpected one MITKConfig.cmake at:")
  foreach(_xq_mitk_candidate IN LISTS _xq_mitk_candidates)
    string(APPEND _xq_error_message "\n  - ${_xq_mitk_candidate}")
  endforeach()
  string(APPEND _xq_error_message "\nFix: fetch sources from Externals/externals.manifest, build Externals, then configure with -DXQ_EXTERNALS_ROOT=<externals-root>")
  message(FATAL_ERROR "${_xq_error_message}")
endif()

set(XQ_EXTERNALS_ROOT "${_xq_default_externals_root}" CACHE PATH "Root directory for XQ external dependencies")
get_filename_component(XQ_EXTERNALS_ROOT "${XQ_EXTERNALS_ROOT}" ABSOLUTE)
set(XQ_EXTERNALS_ROOT "${XQ_EXTERNALS_ROOT}" CACHE PATH "Root directory for XQ external dependencies" FORCE)
_xq_externals_install_dir("${XQ_EXTERNALS_ROOT}" _xq_resolved_externals_dir)
set(XQ_EXTERNALS_DIR "${_xq_resolved_externals_dir}" CACHE PATH "Install directory for XQ external dependencies" FORCE)
_xq_find_mitk_config("${XQ_EXTERNALS_ROOT}" "${XQ_EXTERNALS_DIR}" _xq_resolved_mitk_dir)

# Qt6
set(Qt6_DIR "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake/Qt6" CACHE PATH "" FORCE)
set(Qt6GuiTools_DIR "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake/Qt6GuiTools" CACHE PATH "" FORCE)
set(Qt6WidgetsTools_DIR "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake/Qt6WidgetsTools" CACHE PATH "" FORCE)
if(WIN32)
  set(QT_QMAKE_EXECUTABLE "${XQ_EXTERNALS_DIR}/qt-6.7.0/bin/qmake.exe" CACHE FILEPATH "" FORCE)
else()
  set(QT_QMAKE_EXECUTABLE "${XQ_EXTERNALS_DIR}/qt-6.7.0/bin/qmake" CACHE FILEPATH "" FORCE)
endif()

# VTK / ITK
set(VTK_DIR "${XQ_EXTERNALS_DIR}/vtk-9.3.0/lib/cmake/vtk-9.3" CACHE PATH "" FORCE)
set(ITK_DIR "${XQ_EXTERNALS_DIR}/itk-5.4.0/lib/cmake/ITK-5.4" CACHE PATH "" FORCE)

# MITK
set(MITK_DIR "${_xq_resolved_mitk_dir}" CACHE PATH "" FORCE)
set(MITK_BUILD_DIR "${MITK_DIR}" CACHE PATH "MITK build directory containing MITKConfig.cmake" FORCE)
set(MITK_INSTALL_DIR "${XQ_EXTERNALS_DIR}/mitk-2024.06" CACHE PATH "" FORCE)
get_filename_component(MITK_EXTERNAL_PROJECT_PREFIX "${MITK_DIR}/../ep" ABSOLUTE)
set(MITK_EXTERNAL_PROJECT_PREFIX "${MITK_EXTERNAL_PROJECT_PREFIX}" CACHE PATH "MITK superbuild external project prefix" FORCE)
set(DCMQI_DIR "${MITK_EXTERNAL_PROJECT_PREFIX}/src/DCMQI-build" CACHE PATH "" FORCE)

# OpenCASCADE / TinyXML2 / GDCM / HDF5 / MMG
set(OpenCASCADE_DIR "${XQ_EXTERNALS_DIR}/opencascade-7.6.0/cmake" CACHE PATH "" FORCE)
set(TinyXML2_DIR "${XQ_EXTERNALS_DIR}/tinyxml2-8.0.0/lib/cmake/tinyxml2" CACHE PATH "" FORCE)
set(GDCM_DIR "${XQ_EXTERNALS_DIR}/gdcm-3.0.10/lib/cmake/gdcm-3.0" CACHE PATH "" FORCE)
set(HDF5_ROOT "${XQ_EXTERNALS_DIR}/hdf5-1.14.3" CACHE PATH "" FORCE)
set(MMG_DIR "${XQ_EXTERNALS_DIR}/mmg-5.3.9" CACHE PATH "" FORCE)

# Python
set(PYTHON_DIR "${XQ_EXTERNALS_DIR}/python-3.11.0")
set(Python3_ROOT_DIR "${PYTHON_DIR}")
if(WIN32)
  set(Python3_EXECUTABLE "${PYTHON_DIR}/python.exe")
  set(Python3_INCLUDE_DIR "${PYTHON_DIR}/include")
  set(Python3_LIBRARY "${PYTHON_DIR}/libs/python311.lib")
  if(EXISTS "${PYTHON_DIR}")
    set(ENV{PATH} "${PYTHON_DIR};$ENV{PATH}")
  endif()
else()
  set(Python3_EXECUTABLE "${PYTHON_DIR}/bin/python3")
  set(Python3_INCLUDE_DIR "${PYTHON_DIR}/include/python3.11")
  set(Python3_LIBRARY "${PYTHON_DIR}/lib/libpython3.11.so")
  if(EXISTS "${PYTHON_DIR}/lib")
    if(DEFINED ENV{LD_LIBRARY_PATH})
      set(ENV{LD_LIBRARY_PATH} "${PYTHON_DIR}/lib:$ENV{LD_LIBRARY_PATH}")
    else()
      set(ENV{LD_LIBRARY_PATH} "${PYTHON_DIR}/lib")
    endif()
  endif()
endif()

# SWIG / FreeType
set(SWIG_DIR "${XQ_EXTERNALS_DIR}/swig-3.0.12" CACHE PATH "" FORCE)
if(WIN32)
  set(SWIG_EXECUTABLE "${SWIG_DIR}/bin/swig.exe" CACHE FILEPATH "" FORCE)
else()
  set(SWIG_EXECUTABLE "${SWIG_DIR}/bin/swig" CACHE FILEPATH "" FORCE)
endif()
set(FREETYPE_DIR "${XQ_EXTERNALS_DIR}/freetype-2.13.0" CACHE PATH "" FORCE)

#-----------------------------------------------------------------------------
# Aggregate CMAKE_PREFIX_PATH
#-----------------------------------------------------------------------------
list(APPEND CMAKE_PREFIX_PATH
  "${XQ_EXTERNALS_DIR}/qt-6.7.0"
  "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake"
  "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake/Qt6"
  "${XQ_EXTERNALS_DIR}/vtk-9.3.0"
  "${XQ_EXTERNALS_DIR}/vtk-9.3.0/lib/cmake/vtk-9.3"
  "${XQ_EXTERNALS_DIR}/itk-5.4.0"
  "${XQ_EXTERNALS_DIR}/itk-5.4.0/lib/cmake/ITK-5.4"
  "${MITK_DIR}"
  "${MITK_EXTERNAL_PROJECT_PREFIX}"
  "${DCMQI_DIR}"
  "${MITK_INSTALL_DIR}"
  "${XQ_EXTERNALS_DIR}/opencascade-7.6.0/cmake"
  "${XQ_EXTERNALS_DIR}/tinyxml2-8.0.0/lib/cmake/tinyxml2"
  "${XQ_EXTERNALS_DIR}/gdcm-3.0.10/lib/cmake/gdcm-3.0"
  "${XQ_EXTERNALS_DIR}/hdf5-1.14.3"
  "${XQ_EXTERNALS_DIR}/mmg-5.3.9"
  "${PYTHON_DIR}"
  "${SWIG_DIR}"
  "${FREETYPE_DIR}"
)
