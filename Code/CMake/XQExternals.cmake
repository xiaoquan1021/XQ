#-----------------------------------------------------------------------------
# XQ External Dependencies Configuration
# Sets paths for all external libraries installed in a local externals tree
#-----------------------------------------------------------------------------

get_filename_component(_xq_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

function(_xq_externals_required_files root out_var)
  set(${out_var}
    "${root}/install/qt-6.7.0/lib/cmake/Qt6/Qt6Config.cmake"
    "${root}/install/vtk-9.3.0/lib/cmake/vtk-9.3/vtk-config.cmake"
    "${root}/install/itk-5.4.0/lib/cmake/ITK-5.4/ITKConfig.cmake"
    "${root}/src/MITK-2024.06/build/MITK-build/MITKConfig.cmake"
    PARENT_SCOPE
  )
endfunction()

function(_xq_is_valid_externals_root root out_var)
  _xq_externals_required_files("${root}" _xq_required_files)
  foreach(_xq_required_file IN LISTS _xq_required_files)
    if(NOT EXISTS "${_xq_required_file}")
      set(${out_var} FALSE PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${out_var} TRUE PARENT_SCOPE)
endfunction()

if(DEFINED XQ_EXTERNALS_ROOT AND NOT "${XQ_EXTERNALS_ROOT}" STREQUAL "")
  set(_xq_externals_candidates "${XQ_EXTERNALS_ROOT}")
else()
  set(_xq_externals_candidates
    "${_xq_repo_root}/../svExternals"
    "${_xq_repo_root}/../External"
    "${_xq_repo_root}/../Externals"
  )
  if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    list(APPEND _xq_externals_candidates
      "$ENV{HOME}/svExternals"
      "$ENV{HOME}/External"
      "$ENV{HOME}/Externals"
      "$ENV{HOME}/Simvascular/Externals"
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
  string(APPEND _xq_error_message "\nFix: cmake -S <XQ> -B <build> -DXQ_EXTERNALS_ROOT=/path/to/svExternals")
  message(FATAL_ERROR "${_xq_error_message}")
endif()

set(XQ_EXTERNALS_ROOT "${_xq_default_externals_root}" CACHE PATH "Root directory for XQ external dependencies")
get_filename_component(XQ_EXTERNALS_ROOT "${XQ_EXTERNALS_ROOT}" ABSOLUTE)
set(XQ_EXTERNALS_ROOT "${XQ_EXTERNALS_ROOT}" CACHE PATH "Root directory for XQ external dependencies" FORCE)
set(XQ_EXTERNALS_DIR "${XQ_EXTERNALS_ROOT}/install" CACHE PATH "Install directory for XQ external dependencies" FORCE)

# Qt6
set(Qt6_DIR "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake/Qt6" CACHE PATH "" FORCE)
set(Qt6GuiTools_DIR "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake/Qt6GuiTools" CACHE PATH "" FORCE)
set(Qt6WidgetsTools_DIR "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake/Qt6WidgetsTools" CACHE PATH "" FORCE)
set(QT_QMAKE_EXECUTABLE "${XQ_EXTERNALS_DIR}/qt-6.7.0/bin/qmake" CACHE FILEPATH "" FORCE)

# VTK
set(VTK_DIR "${XQ_EXTERNALS_DIR}/vtk-9.3.0/lib/cmake/vtk-9.3" CACHE PATH "" FORCE)

# ITK
set(ITK_DIR "${XQ_EXTERNALS_DIR}/itk-5.4.0/lib/cmake/ITK-5.4" CACHE PATH "" FORCE)

# MITK (build tree for MITKConfig.cmake and headers)
set(MITK_DIR "${XQ_EXTERNALS_ROOT}/src/MITK-2024.06/build/MITK-build" CACHE PATH "" FORCE)
# MITK (install tree for runtime .so files)
set(MITK_INSTALL_DIR "${XQ_EXTERNALS_DIR}/mitk-2024.06" CACHE PATH "" FORCE)

# OpenCASCADE
set(OpenCASCADE_DIR "${XQ_EXTERNALS_DIR}/opencascade-7.6.0/lib/cmake/opencascade" CACHE PATH "" FORCE)

# TinyXML2
set(TinyXML2_DIR "${XQ_EXTERNALS_DIR}/tinyxml2-8.0.0/lib/cmake/tinyxml2" CACHE PATH "" FORCE)

# GDCM
set(GDCM_DIR "${XQ_EXTERNALS_DIR}/gdcm-3.0.10/lib/cmake/gdcm-3.0" CACHE PATH "" FORCE)

# HDF5
set(HDF5_ROOT "${XQ_EXTERNALS_DIR}/hdf5-1.14.3" CACHE PATH "" FORCE)

# MMG
set(MMG_DIR "${XQ_EXTERNALS_DIR}/mmg-5.3.9" CACHE PATH "" FORCE)

# Python
set(PYTHON_DIR "${XQ_EXTERNALS_DIR}/python-3.11.0")
set(Python3_ROOT_DIR "${PYTHON_DIR}")
set(Python3_EXECUTABLE "${PYTHON_DIR}/bin/python3")
set(Python3_INCLUDE_DIR "${PYTHON_DIR}/include/python3.11")
set(Python3_LIBRARY "${PYTHON_DIR}/lib/libpython3.11.so")

# CMake's FindPython3 runs the interpreter while finding VTK's Python-backed
# modules. The local Python build needs its lib directory in the loader path.
if(EXISTS "${PYTHON_DIR}/lib")
  if(DEFINED ENV{LD_LIBRARY_PATH})
    set(ENV{LD_LIBRARY_PATH} "${PYTHON_DIR}/lib:$ENV{LD_LIBRARY_PATH}")
  else()
    set(ENV{LD_LIBRARY_PATH} "${PYTHON_DIR}/lib")
  endif()
endif()

# SWIG
set(SWIG_DIR "${XQ_EXTERNALS_DIR}/swig-3.0.12" CACHE PATH "" FORCE)
set(SWIG_EXECUTABLE "${SWIG_DIR}/bin/swig" CACHE FILEPATH "" FORCE)

# FreeType
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
  "${XQ_EXTERNALS_ROOT}/src/MITK-2024.06/build/MITK-build"
  "${XQ_EXTERNALS_DIR}/mitk-2024.06"
  "${XQ_EXTERNALS_DIR}/opencascade-7.6.0/lib/cmake/opencascade"
  "${XQ_EXTERNALS_DIR}/tinyxml2-8.0.0/lib/cmake/tinyxml2"
  "${XQ_EXTERNALS_DIR}/gdcm-3.0.10/lib/cmake/gdcm-3.0"
  "${XQ_EXTERNALS_DIR}/hdf5-1.14.3"
  "${XQ_EXTERNALS_DIR}/mmg-5.3.9"
  "${XQ_EXTERNALS_DIR}/python-3.11.0"
  "${XQ_EXTERNALS_DIR}/swig-3.0.12"
  "${XQ_EXTERNALS_DIR}/freetype-2.13.0"
)
