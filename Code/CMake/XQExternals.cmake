#-----------------------------------------------------------------------------
# XQ External Dependencies Configuration
# Sets paths for all external libraries installed in Externals
#-----------------------------------------------------------------------------

set(XQ_EXTERNALS_ROOT "$ENV{HOME}/Externals" CACHE PATH "Root directory for XQ external dependencies")
set(XQ_EXTERNALS_DIR "${XQ_EXTERNALS_ROOT}/install" CACHE PATH "Install directory for XQ external dependencies")

# Qt6
set(Qt6_DIR "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake/Qt6")
set(Qt6GuiTools_DIR "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake/Qt6GuiTools")
set(Qt6WidgetsTools_DIR "${XQ_EXTERNALS_DIR}/qt-6.7.0/lib/cmake/Qt6WidgetsTools")
set(QT_QMAKE_EXECUTABLE "${XQ_EXTERNALS_DIR}/qt-6.7.0/bin/qmake")

# VTK
set(VTK_DIR "${XQ_EXTERNALS_DIR}/vtk-9.3.0/lib/cmake/vtk-9.3")

# ITK
set(ITK_DIR "${XQ_EXTERNALS_DIR}/itk-5.4.0/lib/cmake/ITK-5.4")

# MITK (build tree for MITKConfig.cmake and headers)
set(MITK_DIR "$ENV{HOME}/Externals/src/MITK-2024.06/build/MITK-build")
# MITK (install tree for runtime .so files)
set(MITK_INSTALL_DIR "${XQ_EXTERNALS_DIR}/mitk-2024.06")

# OpenCASCADE
set(OpenCASCADE_DIR "${XQ_EXTERNALS_DIR}/opencascade-7.6.0/lib/cmake/opencascade")

# TinyXML2
set(TinyXML2_DIR "${XQ_EXTERNALS_DIR}/tinyxml2-8.0.0/lib/cmake/tinyxml2")

# GDCM
set(GDCM_DIR "${XQ_EXTERNALS_DIR}/gdcm-3.0.10/lib/cmake/gdcm-3.0")

# HDF5
set(HDF5_ROOT "${XQ_EXTERNALS_DIR}/hdf5-1.14.3")

# MMG
set(MMG_DIR "${XQ_EXTERNALS_DIR}/mmg-5.3.9")

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
set(SWIG_DIR "${XQ_EXTERNALS_DIR}/swig-3.0.12")
set(SWIG_EXECUTABLE "${SWIG_DIR}/bin/swig")

# FreeType
set(FREETYPE_DIR "${XQ_EXTERNALS_DIR}/freetype-2.13.0")

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
