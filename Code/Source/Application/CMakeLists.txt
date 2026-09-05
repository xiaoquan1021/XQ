project(XQ_Application)

set(XQ_EXE "XQ")
set(CXXSRCS main.cxx)
set(MITK_USE_HDF5 OFF)

# Include plugin list
include(${CMAKE_CURRENT_SOURCE_DIR}/../ImagingWorkbench/Plugins/PluginList.cmake)

mitkFunctionCreateBlueBerryApplication(
    NAME ${XQ_EXE}
    SOURCES ${CXXSRCS}
    DESCRIPTION "XQ Medical Imaging Software"
)

# Allow unresolved symbols in transitive shared libs (HDF5 1.12 vs 1.14 ABI mismatch
# in ITK's ITKIOHDF5 — our code doesn't use HDF5 directly)
if(NOT WIN32)
    target_link_options(${XQ_EXE} PRIVATE "LINKER:--allow-shlib-undefined")
endif()
