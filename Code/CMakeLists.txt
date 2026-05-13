#-----------------------------------------------------------------------------
# XQ Medical Imaging Software - Code-Level CMake Configuration
#-----------------------------------------------------------------------------

set(XQ_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

#-----------------------------------------------------------------------------
# 1. Include build options
#-----------------------------------------------------------------------------
include(${XQ_CMAKE_DIR}/XQOptions.cmake)

#-----------------------------------------------------------------------------
# 2. Include external dependency paths
#-----------------------------------------------------------------------------
include(${XQ_CMAKE_DIR}/XQExternals.cmake)

#-----------------------------------------------------------------------------
# 3. Find external packages
#-----------------------------------------------------------------------------

# Qt6
find_package(Qt6 REQUIRED COMPONENTS
  Core Widgets Gui Xml OpenGL OpenGLWidgets
  Network WebEngineWidgets Svg UiTools
)

# VTK (save VTK_DIR before ITK overwrites it)
set(_saved_VTK_DIR ${VTK_DIR})
find_package(VTK REQUIRED)

# ITK (ITK's find can overwrite VTK_DIR — save and restore)
find_package(ITK REQUIRED)
set(VTK_DIR ${_saved_VTK_DIR})
include(${ITK_USE_FILE})

# OpenMP (required by MITK's MitkAlgorithmsExt)
find_package(OpenMP QUIET)

# MITK
find_package(MITK REQUIRED)

# OpenCASCADE
if(XQ_USE_OPENCASCADE)
  find_package(OpenCASCADE REQUIRED)
endif()

# TinyXML2
find_package(tinyxml2 REQUIRED)

# HDF5 (needed by ITK's HDF5 IO)
set(HDF5_DIR "${XQ_EXTERNALS_DIR}/hdf5-1.14.3/cmake")
find_package(HDF5 QUIET COMPONENTS CXX C)
if(HDF5_FOUND)
  link_directories(${HDF5_LIBRARY_DIRS})
else()
  # Fallback: add HDF5 lib dir directly
  link_directories("${XQ_EXTERNALS_DIR}/hdf5-1.14.3/lib")
endif()

#-----------------------------------------------------------------------------
# 3b. Convenience library variables
#     MITK exports targets (MitkCore etc.), not a MITK_LIBRARIES variable.
#     Define convenience variables so modules can use ${MITK_LIBRARIES} etc.
#-----------------------------------------------------------------------------
set(MITK_LIBRARIES MitkCore)
set(QT_LIBRARIES Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Xml Qt6::Network)
set(TINYXML2_LIBRARIES tinyxml2::tinyxml2)

# VTK_LIBRARIES — populated by find_package(VTK) only with COMPONENTS.
# Define common targets if empty.
if(NOT VTK_LIBRARIES)
  set(VTK_LIBRARIES
    VTK::CommonCore VTK::CommonDataModel VTK::CommonExecutionModel
    VTK::FiltersCore VTK::FiltersGeneral VTK::FiltersSources VTK::FiltersGeometry
    VTK::IOCore VTK::IOXML VTK::IOLegacy VTK::IOGeometry
    VTK::RenderingCore VTK::RenderingOpenGL2 VTK::InteractionStyle
    VTK::InteractionWidgets
  )
endif()

# ITK_LIBRARIES is set by include(${ITK_USE_FILE})
# Remove HDF5-dependent ITK modules — ABI mismatch between ITK's HDF5 1.12 and our 1.14
list(REMOVE_ITEM ITK_LIBRARIES "ITKIOHDF5" "ITKIOTransformHDF5" "ITKHDF5"
     "itkIOHDF5" "itkIOTransformHDF5" "itkhdf5_cpp" "itkhdf5")

#-----------------------------------------------------------------------------
# 4. Include build macros
#-----------------------------------------------------------------------------
include(${XQ_CMAKE_DIR}/XQMacros.cmake)

#-----------------------------------------------------------------------------
# 5. CppMicroServices setup
#-----------------------------------------------------------------------------
set(US_CMAKE_DIR "${XQ_CMAKE_DIR}/CppMicroServices")
set(US_MODULE_INIT_TEMPLATE "${US_CMAKE_DIR}/usModuleInit.cpp")
set(US_CMAKE_RESOURCE_DEPENDENCIES_CPP "${US_CMAKE_DIR}/usCMakeResourceDependencies.cpp")
set(US_RESOURCE_RC_TEMPLATE "${US_CMAKE_DIR}/us_resources.rc.in")

include(${US_CMAKE_DIR}/usFunctionCheckCompilerFlags.cmake)
include(${US_CMAKE_DIR}/usFunctionCheckResourceLinking.cmake)
include(${US_CMAKE_DIR}/usFunctionGenerateModuleInit.cmake)
include(${US_CMAKE_DIR}/usFunctionGetResourceSource.cmake)
include(${US_CMAKE_DIR}/usFunctionAddResources.cmake)
include(${US_CMAKE_DIR}/usFunctionEmbedResources.cmake)

# Check resource linking capability
usFunctionCheckResourceLinking()

# Locate CppMicroServices resource compiler from MITK build
find_program(US_RCC_EXECUTABLE
  NAMES usResourceCompiler
  PATHS "${MITK_DIR}/../../bin"
        "${MITK_INSTALL_DIR}/bin"
  NO_DEFAULT_PATH
)
set(US_RCC_EXECUTABLE_NAME usResourceCompiler)

#-----------------------------------------------------------------------------
# 6. MITK include directories
#-----------------------------------------------------------------------------
set(MITK_SRC_DIR "$ENV{HOME}/Externals/src/MITK-2024.06")
set(MITK_BUILD_DIR "${MITK_SRC_DIR}/build/MITK-build")

# MITK build-tree headers (Modules, Plugins)
include_directories(
  ${MITK_BUILD_DIR}/Modules
  ${MITK_BUILD_DIR}/Plugins
  ${MITK_SRC_DIR}/Modules
  ${MITK_SRC_DIR}/Plugins
)

# MITK install-tree headers
include_directories(
  ${MITK_INSTALL_DIR}/include/mitk
  ${MITK_INSTALL_DIR}/include/ctk
)

# BlueBerry plugin include directories
set(BERRY_PLUGIN_SOURCE_DIRS
  "${MITK_SRC_DIR}/Plugins/org.blueberry.core.runtime/src"
  "${MITK_SRC_DIR}/Plugins/org.blueberry.core.commands/src"
  "${MITK_SRC_DIR}/Plugins/org.blueberry.ui.qt/src"
  "${MITK_SRC_DIR}/Plugins/org.mitk.gui.common/src"
  "${MITK_SRC_DIR}/Plugins/org.mitk.gui.qt.common/src"
  "${MITK_SRC_DIR}/Plugins/org.mitk.gui.qt.application/src"
  "${MITK_SRC_DIR}/Plugins/org.mitk.gui.qt.ext/src"
  "${MITK_SRC_DIR}/Plugins/org.mitk.gui.qt.extapplication/src"
  "${MITK_SRC_DIR}/Plugins/org.mitk.gui.qt.datamanager/src"
)

# Add build-tree generated headers for each plugin
set(BERRY_PLUGIN_BUILD_DIRS
  "${MITK_BUILD_DIR}/Plugins/org.blueberry.core.runtime"
  "${MITK_BUILD_DIR}/Plugins/org.blueberry.core.commands"
  "${MITK_BUILD_DIR}/Plugins/org.blueberry.ui.qt"
  "${MITK_BUILD_DIR}/Plugins/org.mitk.gui.common"
  "${MITK_BUILD_DIR}/Plugins/org.mitk.gui.qt.common"
  "${MITK_BUILD_DIR}/Plugins/org.mitk.gui.qt.application"
  "${MITK_BUILD_DIR}/Plugins/org.mitk.gui.qt.ext"
  "${MITK_BUILD_DIR}/Plugins/org.mitk.gui.qt.extapplication"
  "${MITK_BUILD_DIR}/Plugins/org.mitk.gui.qt.datamanager"
)

include_directories(
  ${BERRY_PLUGIN_SOURCE_DIRS}
  ${BERRY_PLUGIN_BUILD_DIRS}
)

#-----------------------------------------------------------------------------
# 7. Module library name variables
#-----------------------------------------------------------------------------
set(XQ_LIB_MODULE_COMMON           "xqModuleCommon")
set(XQ_LIB_MODULE_PATH             "xqModulePath")
set(XQ_LIB_MODULE_SEGMENTATION     "xqModuleSegmentation")
set(XQ_LIB_MODULE_MODELING_COMMON  "xqModuleModelCommon")
set(XQ_LIB_MODULE_MODELING_OCCT    "xqModuleModelOCCT")
set(XQ_LIB_MODULE_MESH_COMMON      "xqModuleMeshCommon")
set(XQ_LIB_MODULE_SIMULATION       "xqModuleSimulation")
set(XQ_LIB_MODULE_PROJECTMGMT     "xqModuleProjectManagement")

#-----------------------------------------------------------------------------
# 8. Output directories
#-----------------------------------------------------------------------------
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Link directories for runtime libraries
link_directories("${MITK_INSTALL_DIR}/lib")
link_directories("${XQ_EXTERNALS_DIR}/hdf5-1.14.3/lib")

#-----------------------------------------------------------------------------
# 9. Add Module subdirectories
#-----------------------------------------------------------------------------
set(XQ_MODULE_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source/xq4gui/Modules")

# Add each module subdirectory (only if CMakeLists.txt exists)
set(XQ_MODULE_DIRS
  Common
  Path
  Segmentation
  Model/Common
  Model/OCCT
  Mesh/Common
  Simulation
  ProjectManagement
)

foreach(_mod_dir ${XQ_MODULE_DIRS})
  if(EXISTS "${XQ_MODULE_BASE_DIR}/${_mod_dir}/CMakeLists.txt")
    add_subdirectory("${XQ_MODULE_BASE_DIR}/${_mod_dir}")
  endif()
endforeach()

#-----------------------------------------------------------------------------
# 10. Add Plugin subdirectories (CTK-integrated)
#-----------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/Source/xq4gui/Plugins/PluginList.cmake)

# Register XQ plugin libraries variable with CTK plugin framework.
# ctkFunctionGetAllPluginTargets() reads CTK_PLUGIN_LIBRARIES_VARS to discover
# all known plugins. We add our own variable so our plugins appear in provisioning.
set(XQ_PLUGIN_LIBRARIES "" CACHE INTERNAL "XQ CTK plug-in targets" FORCE)
set_property(GLOBAL APPEND PROPERTY CTK_PLUGIN_LIBRARIES_VARS XQ_PLUGIN_LIBRARIES)

# Define GetMyTargetLibraries so ctkMacroGetAllProjectTargetLibraries can
# identify XQ plugin targets as "project" targets (not external).
macro(GetMyTargetLibraries all_target_libraries varname)
  set(re_ctkplugin "^org_xq_[a-zA-Z0-9_]+$")
  set(_tmp_list)
  list(APPEND _tmp_list ${all_target_libraries})
  ctkMacroListFilter(_tmp_list re_ctkplugin OUTPUT_VARIABLE ${varname})
endmacro()

set(XQ_PLUGIN_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source/xq4gui/Plugins")

foreach(_plugin_entry ${XQ_PLUGINS})
  # Parse "plugin_name:ON/OFF"
  string(REPLACE ":" ";" _plugin_parts "${_plugin_entry}")
  list(GET _plugin_parts 0 _plugin_name)
  list(GET _plugin_parts 1 _plugin_enabled)

  if(_plugin_enabled STREQUAL "ON")
    if(EXISTS "${XQ_PLUGIN_BASE_DIR}/${_plugin_name}/CMakeLists.txt")
      add_subdirectory("${XQ_PLUGIN_BASE_DIR}/${_plugin_name}")
    endif()
  endif()
endforeach()

#-----------------------------------------------------------------------------
# 11. Add Application subdirectory
#-----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Application/CMakeLists.txt")
  add_subdirectory(Source/Application)
endif()

#-----------------------------------------------------------------------------
# 12. Add Testing subdirectory
#-----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Testing/CMakeLists.txt")
  add_subdirectory(Testing)
endif()
