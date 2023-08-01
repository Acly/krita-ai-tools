add_library(dlimgedit INTERFACE IMPORTED)
set_target_properties(dlimgedit PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/dlimgedit/include)

set(kritatoolsegmentation_SOURCES
    SegmentationToolHelper.cpp
    SegmentationToolPlugin.cpp
    SegmentationToolShared.cpp
    SelectSegmentFromPointTool.cpp
    SelectSegmentFromRectTool.cpp
    ../../tools/selectiontools/kis_selection_modifier_mapper.cc
)

qt5_add_resources(kritatoolsegmentation_SOURCES kritatoolsegmentation.qrc)

kis_add_library(kritatoolsegmentation MODULE ${kritatoolsegmentation_SOURCES})

generate_export_header(kritatoolsegmentation BASE_NAME kritatoolsegmentation)

target_include_directories(kritatoolsegmentation PRIVATE ../../tools/selectiontools)
target_link_libraries(kritatoolsegmentation PRIVATE kritaui kritabasicflakes kritaimage dlimgedit)

# Install into krita install prefix to test & run
install(DIRECTORY dlimgedit/bin/ DESTINATION ${BIN_INSTALL_DIR})
install(DIRECTORY dlimgedit/models/ DESTINATION ${DATA_INSTALL_DIR}/krita/ai_models)
install(TARGETS kritatoolsegmentation DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install(FILES
    SelectSegmentFromPointTool.action
    SelectSegmentFromRectTool.action
    DESTINATION ${DATA_INSTALL_DIR}/krita/actions
)

# Install into isolated directory for packaging the plugin only
set(PACKAGE_DIR ${CMAKE_INSTALL_PREFIX}/../krita-plugin-segmentation)
install(TARGETS kritatoolsegmentation DESTINATION ${PACKAGE_DIR}/${KRITA_PLUGIN_INSTALL_DIR})
install(DIRECTORY dlimgedit/bin/ DESTINATION ${PACKAGE_DIR}/${BIN_INSTALL_DIR})
install(DIRECTORY dlimgedit/models/ 
    DESTINATION ${PACKAGE_DIR}/${DATA_INSTALL_DIR}/krita/ai_models
    FILES_MATCHING PATTERN "mobile_sam_image_encoder.onnx" PATTERN "sam_mask_decoder_single.onnx"
)
install(FILES
    SelectSegmentFromPointTool.action
    SelectSegmentFromRectTool.action
    DESTINATION ${PACKAGE_DIR}/${DATA_INSTALL_DIR}/krita/actions
)
