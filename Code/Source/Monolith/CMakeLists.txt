project(XQ_Monolith)

set(XQ_MONOLITH_CORE_SRCS
    Core/xq_ApplicationContext.cxx
    Core/xq_DataCatalogService.cxx
    Core/xq_DataImportService.cxx
    Core/xq_PreferencesService.cxx
    Core/xq_ProjectService.cxx
    Core/xq_TaskRunner.cxx
    Core/xq_WorkflowRegistry.cxx
)

set(XQ_MONOLITH_CORE_HDRS
    Core/xq_ApplicationContext.h
    Core/xq_DataCatalogService.h
    Core/xq_DataImportService.h
    Core/xq_PreferencesService.h
    Core/xq_ProjectService.h
    Core/xq_TaskRunner.h
    Core/xq_WorkflowRegistry.h
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

add_executable(XQMonolith
    main.cxx
    Presentation/xq_MainWindow.cxx
    Presentation/xq_MainWindow.h
)

set_target_properties(XQMonolith PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NOT XQ_BUILD_LEGACY_BLUEBERRY)
    set_target_properties(XQMonolith PROPERTIES OUTPUT_NAME XQ)
endif()

target_include_directories(XQMonolith
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(XQMonolith
    PRIVATE
        xqMonolithCore
        MitkCore
        MitkQtWidgets
        Qt6::Core
        Qt6::Widgets
)
