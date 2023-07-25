add_library(dlimgedit INTERFACE IMPORTED)
set_target_properties(dlimgedit PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/dlimgedit/include)
install(DIRECTORY dlimgedit/bin/ DESTINATION ${BIN_INSTALL_DIR})
install(DIRECTORY dlimgedit/models/ DESTINATION ${DATA_INSTALL_DIR}/krita/ai_models)

set(kritatoolsegmentation_SOURCES
    SegmentationToolCommon.cpp
    SegmentationToolPlugin.cpp
    SelectSegmentFromPointTool.cpp
    SelectSegmentFromRectTool.cpp
    ../../tools/selectiontools/kis_selection_modifier_mapper.cc
)

qt5_add_resources(kritatoolsegmentation_SOURCES kritatoolsegmentation.qrc)

kis_add_library(kritatoolsegmentation MODULE ${kritatoolsegmentation_SOURCES})

generate_export_header(kritatoolsegmentation BASE_NAME kritatoolsegmentation)

target_include_directories(kritatoolsegmentation PRIVATE ../../tools/selectiontools)
target_link_libraries(kritatoolsegmentation PRIVATE kritaui kritabasicflakes kritaimage dlimgedit)

install(TARGETS kritatoolsegmentation DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES
    SelectSegmentFromPointTool.action
    SelectSegmentFromRectTool.action
    DESTINATION ${DATA_INSTALL_DIR}/krita/actions
)
