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
)

set(XQ_MONOLITH_PRESENTATION_HDRS
    Presentation/xq_DataHierarchyModel.h
    Presentation/xq_MainWindow.h
    Presentation/xq_QtFileImportPathProvider.h
)

add_library(xqMonolithPresentation STATIC
    ${XQ_MONOLITH_PRESENTATION_SRCS}
    ${XQ_MONOLITH_PRESENTATION_HDRS}
)

set_target_properties(xqMonolithPresentation PROPERTIES
    AUTOMOC ON
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
        Qt6::Core
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
