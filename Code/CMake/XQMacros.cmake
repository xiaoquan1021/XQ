#-----------------------------------------------------------------------------
# XQ Build Macros
# Provides xq_create_module() and xq_create_plugin() for building
# CppMicroServices-based modules and BlueBerry plugins.
#-----------------------------------------------------------------------------

include(GenerateExportHeader)

#-----------------------------------------------------------------------------
# xq_create_module()
#
# Creates a shared library module with CppMicroServices integration.
#
# Arguments:
#   TARGET           - Target name for the library
#   EXPORT_DIRECTIVE - Export macro name (generates ${EXPORT_DIRECTIVE}Exports.h)
#   LIBRARY_DEPENDS  - List of library targets to link against
#   PACKAGE_DEPENDS  - List of packages this module depends on
#-----------------------------------------------------------------------------
function(xq_create_module)
  cmake_parse_arguments(XQ_MOD "" "TARGET;EXPORT_DIRECTIVE" "LIBRARY_DEPENDS;PACKAGE_DEPENDS" ${ARGN})

  if(NOT XQ_MOD_TARGET)
    message(FATAL_ERROR "xq_create_module: TARGET argument is required")
  endif()

  # 1. Include files.cmake to get source file lists
  set(H_FILES "")
  set(CPP_FILES "")
  set(MOC_H_FILES "")
  set(UI_FILES "")
  set(RESOURCE_FILES "")
  set(QRC_FILES "")
  include(${CMAKE_CURRENT_SOURCE_DIR}/files.cmake)

  set(_xq_resource_working_dir "${CMAKE_CURRENT_SOURCE_DIR}")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resource")
    set(_xq_resource_working_dir "${CMAKE_CURRENT_SOURCE_DIR}/resource")
  endif()

  # CppMicroServices resources must be part of the target sources when using
  # LINK mode; otherwise the XML state machines are not available at runtime.
  if(RESOURCE_FILES)
    usFunctionGetResourceSource(TARGET ${XQ_MOD_TARGET} OUT CPP_FILES LINK)
  endif()

  # 2. CppMicroServices module init
  usFunctionGenerateModuleInit(CPP_FILES)

  # 3. Create shared library (use AUTOMOC/AUTOUIC/AUTORCC for Qt processing)
  add_library(${XQ_MOD_TARGET} SHARED
    ${H_FILES}
    ${CPP_FILES}
    ${MOC_H_FILES}
    ${UI_FILES}
    ${QRC_FILES}
  )
  set_target_properties(${XQ_MOD_TARGET} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
  )

  # 4. Set CppMicroServices module name
  set_property(TARGET ${XQ_MOD_TARGET} PROPERTY US_MODULE_NAME ${XQ_MOD_TARGET})
  target_compile_definitions(${XQ_MOD_TARGET} PRIVATE US_MODULE_NAME=${XQ_MOD_TARGET})

  # 8. Embed resources if RESOURCE_FILES defined
  if(RESOURCE_FILES)
    usFunctionAddResources(TARGET ${XQ_MOD_TARGET}
      MODULE_NAME ${XQ_MOD_TARGET}
      WORKING_DIRECTORY ${_xq_resource_working_dir}
      FILES ${RESOURCE_FILES}
    )
    usFunctionEmbedResources(TARGET ${XQ_MOD_TARGET}
      MODULE_NAME ${XQ_MOD_TARGET}
      WORKING_DIRECTORY ${_xq_resource_working_dir}
      LINK
    )
  endif()

  # 9. Link dependencies
  if(XQ_MOD_LIBRARY_DEPENDS)
    target_link_libraries(${XQ_MOD_TARGET} PUBLIC ${XQ_MOD_LIBRARY_DEPENDS})
  endif()

  # 9b. Find and link PACKAGE_DEPENDS
  if(XQ_MOD_PACKAGE_DEPENDS)
    foreach(_dep ${XQ_MOD_PACKAGE_DEPENDS})
      if(_dep MATCHES "^([^|]+)\\|(.+)$")
        set(_pkg_name "${CMAKE_MATCH_1}")
        string(REPLACE "+" ";" _components "${CMAKE_MATCH_2}")
        find_package(${_pkg_name} COMPONENTS ${_components} REQUIRED)
        foreach(_comp ${_components})
          target_link_libraries(${XQ_MOD_TARGET} PUBLIC ${_pkg_name}::${_comp})
        endforeach()
      else()
        find_package(${_dep} QUIET)
        if(${_dep}_LIBRARIES)
          target_link_libraries(${XQ_MOD_TARGET} PUBLIC ${${_dep}_LIBRARIES})
        endif()
      endif()
    endforeach()
  endif()

  # 10. Generate export header
  if(XQ_MOD_EXPORT_DIRECTIVE)
    string(TOUPPER ${XQ_MOD_EXPORT_DIRECTIVE} _UPPER_EXPORT)
    generate_export_header(${XQ_MOD_TARGET}
      EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/${XQ_MOD_EXPORT_DIRECTIVE}Exports.h"
      EXPORT_MACRO_NAME "${_UPPER_EXPORT}_EXPORT"
    )
  endif()

  # 11. Include directories
  target_include_directories(${XQ_MOD_TARGET} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
  )

endfunction()


#-----------------------------------------------------------------------------
# xq_create_plugin()
#
# Creates a shared library plugin for the BlueBerry framework with
# CppMicroServices integration.
#
# Arguments:
#   TARGET           - Target name for the plugin
#   EXPORT_DIRECTIVE - Export macro name (generates ${EXPORT_DIRECTIVE}Exports.h)
#   MODULE_DEPENDS   - List of XQ module targets to link against
#   PACKAGE_DEPENDS  - List of packages this plugin depends on
#-----------------------------------------------------------------------------
function(xq_create_plugin)
  cmake_parse_arguments(XQ_PLG "" "TARGET;EXPORT_DIRECTIVE" "MODULE_DEPENDS;PACKAGE_DEPENDS" ${ARGN})

  if(NOT XQ_PLG_TARGET)
    message(FATAL_ERROR "xq_create_plugin: TARGET argument is required")
  endif()

  # 1. Include files.cmake to get source file lists
  set(H_FILES "")
  set(CPP_FILES "")
  set(MOC_H_FILES "")
  set(UI_FILES "")
  set(RESOURCE_FILES "")
  set(QRC_FILES "")
  set(CACHED_RESOURCE_FILES "")
  include(${CMAKE_CURRENT_SOURCE_DIR}/files.cmake)

  # 1b. Prepend src/internal/ to bare CPP_FILES (plugin convention)
  set(_full_cpp_files "")
  foreach(_f ${CPP_FILES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
      list(APPEND _full_cpp_files "${_f}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/${_f}")
      list(APPEND _full_cpp_files "src/internal/${_f}")
    else()
      list(APPEND _full_cpp_files "${_f}")
    endif()
  endforeach()
  set(CPP_FILES ${_full_cpp_files})

  # 2. CppMicroServices module init
  usFunctionGenerateModuleInit(CPP_FILES)

  # Derive plugin symbolic name (dots) from target name (underscores)
  string(REPLACE "_" "." _plugin_symbolic_name "${XQ_PLG_TARGET}")

  # 3. Read manifest_headers.cmake and generate embedded MANIFEST.MF
  set(_manifest_qrc_srcs "")
  set(Plugin-SymbolicName "${_plugin_symbolic_name}")
  set(Plugin-Name "")
  set(Plugin-Version "")
  set(Plugin-Vendor "")
  set(Plugin-ContactAddress "")
  set(Require-Plugin "")
  set(Plugin-ActivationPolicy "")

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/manifest_headers.cmake")
    include(${CMAKE_CURRENT_SOURCE_DIR}/manifest_headers.cmake)
  endif()

  # Build MANIFEST.MF content (CTK format)
  set(_manifest_content "Plugin-SymbolicName: ${_plugin_symbolic_name}")
  if(Plugin-ActivationPolicy)
    set(_manifest_content "${_manifest_content}\nPlugin-ActivationPolicy: ${Plugin-ActivationPolicy}")
  endif()
  if(Plugin-Name)
    set(_manifest_content "${_manifest_content}\nPlugin-Name: ${Plugin-Name}")
  endif()
  if(Plugin-Version)
    set(_manifest_content "${_manifest_content}\nPlugin-Version: ${Plugin-Version}")
  endif()
  if(Plugin-Vendor)
    set(_manifest_content "${_manifest_content}\nPlugin-Vendor: ${Plugin-Vendor}")
  endif()
  if(Plugin-ContactAddress)
    set(_manifest_content "${_manifest_content}\nPlugin-ContactAddress: ${Plugin-ContactAddress}")
  endif()
  if(Require-Plugin)
    string(REPLACE ";" "," _require_str "${Require-Plugin}")
    set(_manifest_content "${_manifest_content}\nRequire-Plugin: ${_require_str}")
  endif()
  set(_manifest_content "${_manifest_content}\n")

  # Write MANIFEST.MF
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/MANIFEST.MF" "${_manifest_content}")

  # Create .qrc to embed MANIFEST.MF at :/plugin.symbolic.name/META-INF/MANIFEST.MF
  set(_manifest_qrc_content
"<!DOCTYPE RCC><RCC version=\"1.0\">
<qresource prefix=\"/${_plugin_symbolic_name}/META-INF\">
 <file>MANIFEST.MF</file>
</qresource>
</RCC>
")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${XQ_PLG_TARGET}_manifest.qrc" "${_manifest_qrc_content}")
  qt_add_resources(_manifest_qrc_srcs "${CMAKE_CURRENT_BINARY_DIR}/${XQ_PLG_TARGET}_manifest.qrc")

  # 3b. Embed CACHED_RESOURCE_FILES (plugin.xml, icons, etc.) as Qt resources
  set(_cached_qrc_srcs "")
  if(CACHED_RESOURCE_FILES)
    set(_cached_qrc_content
"<!DOCTYPE RCC><RCC version=\"1.0\">
<qresource prefix=\"/${_plugin_symbolic_name}\">
")
    foreach(_res ${CACHED_RESOURCE_FILES})
      configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${_res}" "${CMAKE_CURRENT_BINARY_DIR}/${_res}" COPYONLY)
      set(_cached_qrc_content "${_cached_qrc_content}<file>${_res}</file>\n")
    endforeach()
    set(_cached_qrc_content "${_cached_qrc_content}</qresource>\n</RCC>\n")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${XQ_PLG_TARGET}_cached.qrc" "${_cached_qrc_content}")
    qt_add_resources(_cached_qrc_srcs "${CMAKE_CURRENT_BINARY_DIR}/${XQ_PLG_TARGET}_cached.qrc")
  endif()

  # 4. Create shared library (use AUTOMOC/AUTOUIC/AUTORCC for Qt processing)
  add_library(${XQ_PLG_TARGET} SHARED
    ${H_FILES}
    ${CPP_FILES}
    ${MOC_H_FILES}
    ${UI_FILES}
    ${QRC_FILES}
    ${_manifest_qrc_srcs}
    ${_cached_qrc_srcs}
  )
  set_target_properties(${XQ_PLG_TARGET} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/plugins"
  )
  if(WIN32)
    # MITK's provisioning generator writes CTK plug-in DLLs as lib<target>.dll.
    set_target_properties(${XQ_PLG_TARGET} PROPERTIES PREFIX "lib")
  endif()

  # 5. Set CppMicroServices module name
  set_property(TARGET ${XQ_PLG_TARGET} PROPERTY US_MODULE_NAME ${XQ_PLG_TARGET})
  target_compile_definitions(${XQ_PLG_TARGET} PRIVATE US_MODULE_NAME=${XQ_PLG_TARGET})

  # 9. Embed resources if RESOURCE_FILES defined
  if(RESOURCE_FILES)
    usFunctionGetResourceSource(TARGET ${XQ_PLG_TARGET} OUT CPP_FILES)
    usFunctionAddResources(TARGET ${XQ_PLG_TARGET}
      MODULE_NAME ${XQ_PLG_TARGET}
      FILES ${RESOURCE_FILES}
    )
    usFunctionEmbedResources(TARGET ${XQ_PLG_TARGET}
      MODULE_NAME ${XQ_PLG_TARGET}
    )
  endif()

  # 10. Link core frameworks + module dependencies
  target_link_libraries(${XQ_PLG_TARGET} PUBLIC
    MitkCore
    MitkQtWidgets
    org_blueberry_core_runtime
    org_blueberry_ui_qt
    org_mitk_gui_common
    org_mitk_gui_qt_common
    Qt6::Core Qt6::Widgets Qt6::Gui
  )
  if(XQ_PLG_MODULE_DEPENDS)
    target_link_libraries(${XQ_PLG_TARGET} PUBLIC ${XQ_PLG_MODULE_DEPENDS})
  endif()

  # 10b. Find and link PACKAGE_DEPENDS
  if(XQ_PLG_PACKAGE_DEPENDS)
    foreach(_dep ${XQ_PLG_PACKAGE_DEPENDS})
      if(_dep MATCHES "^([^|]+)\\|(.+)$")
        set(_pkg_name "${CMAKE_MATCH_1}")
        string(REPLACE "+" ";" _components "${CMAKE_MATCH_2}")
        find_package(${_pkg_name} COMPONENTS ${_components} REQUIRED)
        foreach(_comp ${_components})
          target_link_libraries(${XQ_PLG_TARGET} PUBLIC ${_pkg_name}::${_comp})
        endforeach()
      else()
        find_package(${_dep} QUIET)
        if(${_dep}_LIBRARIES)
          target_link_libraries(${XQ_PLG_TARGET} PUBLIC ${${_dep}_LIBRARIES})
        endif()
      endif()
    endforeach()
  endif()

  # 11. Generate export header
  if(XQ_PLG_EXPORT_DIRECTIVE)
    string(TOUPPER ${XQ_PLG_EXPORT_DIRECTIVE} _UPPER_PLG_EXPORT)
    generate_export_header(${XQ_PLG_TARGET}
      EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/${XQ_PLG_EXPORT_DIRECTIVE}Exports.h"
      EXPORT_MACRO_NAME "${_UPPER_PLG_EXPORT}_EXPORT"
    )
  endif()

  # 12. Include directories
  target_include_directories(${XQ_PLG_TARGET} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
    ${CMAKE_CURRENT_BINARY_DIR}
  )

  # 13. Register with CTK plugin framework so the plugin appears in provisioning
  set(XQ_PLUGIN_LIBRARIES ${XQ_PLUGIN_LIBRARIES} ${XQ_PLG_TARGET}
      CACHE INTERNAL "XQ CTK plug-in targets" FORCE)

endfunction()
