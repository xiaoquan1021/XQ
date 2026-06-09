project(XQ_Monolith)

set(XQ_MONOLITH_CORE_SRCS
    Core/xq_ApplicationContext.cxx
    Core/xq_DataCatalogService.cxx
    Core/xq_DataHierarchyService.cxx
    Core/xq_DataImportService.cxx
    Core/xq_DataManagementService.cxx
    Core/xq_DataNodeRegistryService.cxx
    Core/xq_DataSelectionService.cxx
    Core/xq_PreferencesService.cxx
    Core/xq_ProjectService.cxx
    Core/xq_ProjectSessionService.cxx
    Core/xq_TaskRunner.cxx
    Core/xq_WorkflowActionService.cxx
    Core/xq_WorkflowContextService.cxx
    Core/xq_WorkflowOperationService.cxx
    Core/xq_WorkflowRegistry.cxx
    Core/xq_WorkflowSelectionService.cxx
)

set(XQ_MONOLITH_CORE_HDRS
    Core/xq_ApplicationContext.h
    Core/xq_DataCatalogService.h
    Core/xq_DataImportCommand.h
    Core/xq_DataHierarchyService.h
    Core/xq_DataImportService.h
    Core/xq_DataManagementService.h
    Core/xq_DataNodeRegistryService.h
    Core/xq_DataSelectionService.h
    Core/xq_PreferencesService.h
    Core/xq_ProjectFilePathProvider.h
    Core/xq_ProjectService.h
    Core/xq_ProjectSessionService.h
    Core/xq_RenderRefreshService.h
    Core/xq_TaskRunner.h
    Core/xq_WorkflowActionService.h
    Core/xq_WorkflowContextService.h
    Core/xq_WorkflowOperationService.h
    Core/xq_WorkflowRegistry.h
    Core/xq_WorkflowSelectionService.h
)

add_library(xqMonolithCore STATIC
    ${XQ_MONOLITH_CORE_SRCS}
    ${XQ_MONOLITH_CORE_HDRS}
)

set_target_properties(xqMonolithCore PROPERTIES
    AUTOMOC ON
)

target_include_directories(xqMonolithCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(xqMonolithCore
    PUBLIC
        MitkCore
        Qt6::Core
)

set(XQ_MONOLITH_DOMAIN_SRCS
    Domain/xq_ImagePreprocessingWorkflowService.cxx
    Domain/xq_WorkflowActionHandlers.cxx
)

set(XQ_MONOLITH_DOMAIN_HDRS
    Domain/xq_ImagePreprocessingWorkflowService.h
    Domain/xq_WorkflowActionHandlers.h
)

add_library(xqMonolithDomain STATIC
    ${XQ_MONOLITH_DOMAIN_SRCS}
    ${XQ_MONOLITH_DOMAIN_HDRS}
)

set_target_properties(xqMonolithDomain PROPERTIES
    AUTOMOC ON
)

target_include_directories(xqMonolithDomain
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(xqMonolithDomain
    PUBLIC
        xqMonolithCore
        Qt6::Core
)

set(XQ_MONOLITH_INFRASTRUCTURE_SRCS
    Infrastructure/xq_DataNodeImportService.cxx
    Infrastructure/xq_MitkFileDataImportCommand.cxx
    Infrastructure/xq_MitkFileImportService.cxx
    Infrastructure/xq_MitkRenderRefreshService.cxx
    Infrastructure/xq_ImagePreprocessingAlgorithmAdapter.cxx
    Infrastructure/xq_ImagePreprocessingApplicationCommitService.cxx
    Infrastructure/xq_ImagePreprocessingCatalogCommitService.cxx
    Infrastructure/xq_ImagePreprocessingExecutionService.cxx
    Infrastructure/xq_ImagePreprocessingMitkImageAdapter.cxx
    Infrastructure/xq_ImagePreprocessingResultNodeFactory.cxx
    Infrastructure/xq_ImagePreprocessingStorageCommitService.cxx
    Infrastructure/xq_ImagePreprocessingWorkflowActionHandler.cxx
    Infrastructure/xq_PathWorkflowActionHandler.cxx
    Infrastructure/xq_SegmentationWorkflowActionHandler.cxx
    Infrastructure/xq_ModelingWorkflowActionHandler.cxx
    Infrastructure/xq_MeshingWorkflowActionHandler.cxx
    Infrastructure/xq_FlowSimulationWorkflowActionHandler.cxx
    Infrastructure/xq_RomSimulationWorkflowActionHandler.cxx
    Infrastructure/xq_MultiPhysicsWorkflowActionHandler.cxx
    Infrastructure/xq_PythonApiWorkflowActionHandler.cxx
)

set(XQ_MONOLITH_INFRASTRUCTURE_HDRS
    Infrastructure/xq_DataNodeImportService.h
    Infrastructure/xq_MitkFileDataImportCommand.h
    Infrastructure/xq_MitkFileImportService.h
    Infrastructure/xq_MitkRenderRefreshService.h
    Infrastructure/xq_ImagePreprocessingAlgorithmAdapter.h
    Infrastructure/xq_ImagePreprocessingApplicationCommitService.h
    Infrastructure/xq_ImagePreprocessingCatalogCommitService.h
    Infrastructure/xq_ImagePreprocessingExecutionService.h
    Infrastructure/xq_ImagePreprocessingMitkImageAdapter.h
    Infrastructure/xq_ImagePreprocessingResultNodeFactory.h
    Infrastructure/xq_ImagePreprocessingStorageCommitService.h
    Infrastructure/xq_ImagePreprocessingWorkflowActionHandler.h
    Infrastructure/xq_PathWorkflowActionHandler.h
    Infrastructure/xq_SegmentationWorkflowActionHandler.h
    Infrastructure/xq_ModelingWorkflowActionHandler.h
    Infrastructure/xq_MeshingWorkflowActionHandler.h
    Infrastructure/xq_FlowSimulationWorkflowActionHandler.h
    Infrastructure/xq_RomSimulationWorkflowActionHandler.h
    Infrastructure/xq_MultiPhysicsWorkflowActionHandler.h
    Infrastructure/xq_PythonApiWorkflowActionHandler.h
)

add_library(xqMonolithInfrastructure STATIC
    ${XQ_MONOLITH_INFRASTRUCTURE_SRCS}
    ${XQ_MONOLITH_INFRASTRUCTURE_HDRS}
)

set_target_properties(xqMonolithInfrastructure PROPERTIES
    AUTOMOC ON
)

target_include_directories(xqMonolithInfrastructure
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(xqMonolithInfrastructure
    PUBLIC
        xqMonolithDomain
        xqModuleCommon
        xqModuleImageProcessing
        xqModulePath
        xqModuleSegmentation
        xqModuleModelCommon
        xqModuleMeshCommon
        xqModuleSimulation
        xqModuleROMSimulation
        xqModuleMultiPhysics
        xqModulePythonApi
        Qt6::Core
)

set(XQ_MONOLITH_PRESENTATION_SRCS
    Presentation/xq_DataHierarchyModel.cxx
    Presentation/xq_MainWindow.cxx
    Presentation/xq_QtFileImportPathProvider.cxx
    Presentation/xq_QtProjectFilePathProvider.cxx
    ../ImagingWorkbench/Plugins/org.xq.core.application/resources/xqApplication.qrc
)

set(XQ_MONOLITH_PRESENTATION_HDRS
    Presentation/xq_DataHierarchyModel.h
    Presentation/xq_MainWindow.h
    Presentation/xq_QtFileImportPathProvider.h
    Presentation/xq_QtProjectFilePathProvider.h
)

add_library(xqMonolithPresentation STATIC
    ${XQ_MONOLITH_PRESENTATION_SRCS}
    ${XQ_MONOLITH_PRESENTATION_HDRS}
)

set_target_properties(xqMonolithPresentation PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_include_directories(xqMonolithPresentation
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(xqMonolithPresentation
    PUBLIC
        xqMonolithCore
        Qt6::Core
        Qt6::Widgets
)

add_library(xqMonolithApplication STATIC
    xq_MonolithApplication.cxx
    xq_MonolithApplication.h
)

set_target_properties(xqMonolithApplication PROPERTIES
    AUTOMOC ON
)

target_include_directories(xqMonolithApplication
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(xqMonolithApplication
    PUBLIC
        xqMonolithInfrastructure
        xqMonolithPresentation
        MitkQtWidgets
        Qt6::Core
        Qt6::OpenGLWidgets
        Qt6::Widgets
)

set(XQ_MONOLITH_TARGET XQ)
if(XQ_BUILD_LEGACY_BLUEBERRY)
    set(XQ_MONOLITH_TARGET XQMonolith)
endif()

add_executable(${XQ_MONOLITH_TARGET}
    main.cxx
)

set_target_properties(${XQ_MONOLITH_TARGET} PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(${XQ_MONOLITH_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${XQ_MONOLITH_TARGET}
    PRIVATE
        xqMonolithApplication
        xqMonolithDomain
        xqMonolithPresentation
        MitkQtWidgets
        Qt6::Widgets
        Qt6::OpenGLWidgets
)

if(WIN32)
    set(XQ_WINDOWS_RUNTIME_DIRS
        "${XQ_EXTERNALS_DIR}/python-3.11.0"
        "${XQ_EXTERNALS_DIR}/hdf5-1.14.3/bin"
        "${XQ_EXTERNALS_DIR}/gdcm-3.0.10/bin"
        "${XQ_EXTERNALS_DIR}/vtk-9.3.0/bin"
        "${XQ_EXTERNALS_DIR}/itk-5.4.0/bin"
        "${XQ_EXTERNALS_DIR}/qt-6.7.0/bin"
        "${XQ_EXTERNALS_DIR}/mitk-2024.06/bin"
        "${XQ_EXTERNALS_DIR}/opencascade-7.6.0/bin"
        "${XQ_EXTERNALS_DIR}/opencascade-7.6.0/win64/vc14/bin"
        "${XQ_EXTERNALS_DIR}/freetype-2.13.0/bin"
        "${XQ_EXTERNALS_ROOT}/build/${XQ_EXTERNALS_PLATFORM}/MITK/MITK-build/bin"
        "${XQ_EXTERNALS_ROOT}/build/${XQ_EXTERNALS_PLATFORM}/MITK/MITK-build/bin/plugins"
        "${XQ_EXTERNALS_ROOT}/build/${XQ_EXTERNALS_PLATFORM}/MITK/ep/src/CTK-build/CTK-build/bin"
    )
    set(XQ_WINDOWS_RUNTIME_FILES)
    set(XQ_ZSTD_RUNTIME_DLL "" CACHE FILEPATH "Optional zstd.dll used by the Qt runtime when Qt was built with host zstd support")
    set(_xq_qt_build_cache "${XQ_EXTERNALS_ROOT}/build/${XQ_EXTERNALS_PLATFORM}/Qt/CMakeCache.txt")
    if(EXISTS "${_xq_qt_build_cache}")
        file(STRINGS "${_xq_qt_build_cache}" _xq_qt_zstd_cache_entry
            REGEX "^zstd_DIR:PATH="
        )
        if(_xq_qt_zstd_cache_entry)
            string(REGEX REPLACE "^zstd_DIR:PATH=(.*)$" "\\1" _xq_qt_zstd_dir "${_xq_qt_zstd_cache_entry}")
            if(EXISTS "${_xq_qt_zstd_dir}")
                set(zstd_DIR "${_xq_qt_zstd_dir}" CACHE PATH "" FORCE)
            endif()
        endif()
    endif()
    find_package(zstd CONFIG QUIET)
    if(TARGET zstd::libzstd_shared)
        get_target_property(_xq_zstd_dll zstd::libzstd_shared IMPORTED_LOCATION_RELEASE)
        if(EXISTS "${_xq_zstd_dll}")
            list(APPEND XQ_WINDOWS_RUNTIME_FILES "${_xq_zstd_dll}")
        endif()
    endif()
    if(NOT XQ_WINDOWS_RUNTIME_FILES)
        if(EXISTS "${XQ_ZSTD_RUNTIME_DLL}")
            list(APPEND XQ_WINDOWS_RUNTIME_FILES "${XQ_ZSTD_RUNTIME_DLL}")
        elseif(DEFINED ENV{XQ_ZSTD_RUNTIME_DLL} AND EXISTS "$ENV{XQ_ZSTD_RUNTIME_DLL}")
            list(APPEND XQ_WINDOWS_RUNTIME_FILES "$ENV{XQ_ZSTD_RUNTIME_DLL}")
        endif()
    endif()
    add_custom_target(xqStageWindowsRuntime ALL
        COMMAND ${CMAKE_COMMAND}
            "-DXQ_STAGE_RUNTIME_DESTINATION=$<TARGET_FILE_DIR:${XQ_MONOLITH_TARGET}>"
            "-DXQ_STAGE_RUNTIME_FILES=${XQ_WINDOWS_RUNTIME_FILES}"
            "-DXQ_STAGE_RUNTIME_DIRS=${XQ_WINDOWS_RUNTIME_DIRS}"
            "-DXQ_STAGE_QT_PLUGINS_DIR=${XQ_EXTERNALS_DIR}/qt-6.7.0/plugins"
            -P "${XQ_CMAKE_DIR}/XQStageWindowsRuntime.cmake"
        DEPENDS ${XQ_MONOLITH_TARGET}
        COMMENT "Staging Windows runtime DLLs for direct XQ startup"
        VERBATIM
    )
endif()
