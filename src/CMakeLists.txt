
set(kritavisionml_SOURCES
    VisionMLPlugin.cpp
    segmentation/SegmentationToolHelper.cpp
    segmentation/SegmentationToolShared.cpp
    segmentation/SelectSegmentFromPointTool.cpp
    segmentation/SelectSegmentFromRectTool.cpp
    ../../tools/selectiontools/kis_selection_modifier_mapper.cc
)

qt5_add_resources(kritavisionml_SOURCES ../icons/kritavisionml.qrc)

kis_add_library(kritavisionml MODULE ${kritavisionml_SOURCES})

generate_export_header(kritavisionml BASE_NAME kritavisionml)

target_include_directories(kritavisionml PRIVATE ../../tools/selectiontools)
target_compile_features(kritavisionml PRIVATE cxx_std_20)
target_link_libraries(kritavisionml PRIVATE kritaui kritabasicflakes kritaimage visioncpp)

# Install into krita install prefix to run & test
install(TARGETS kritavisionml DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install(FILES vision_ml.action DESTINATION ${DATA_INSTALL_DIR}/krita/actions)

# Install into isolated directory to distribute just the plugin
# if(WIN32)
#     set(PACKAGE_DIR ${CMAKE_INSTALL_PREFIX}/../krita-plugin-segmentation)
#     install(DIRECTORY ${dlimgedit_SOURCE_DIR}/bin/ DESTINATION ${PACKAGE_DIR}/${BIN_INSTALL_DIR})
#     install(TARGETS kritatoolsegmentation DESTINATION ${PACKAGE_DIR}/${KRITA_PLUGIN_INSTALL_DIR})
# else() # Linux AppImage
#     set(PACKAGE_DIR ${CMAKE_INSTALL_PREFIX}/../krita-plugin-segmentation/usr)
#     install(TARGETS dlimgedit DESTINATION ${PACKAGE_DIR}/lib/kritaplugins/toolsegmentation)
#     install(DIRECTORY ${ONNXRUNTIME_LIB_DIR}/ DESTINATION ${PACKAGE_DIR}/lib/kritaplugins/toolsegmentation ${ONNX_FILTER})
#     install(TARGETS kritatoolsegmentation DESTINATION ${PACKAGE_DIR}/lib/kritaplugins)
# endif()
# install(DIRECTORY ${dlimgedit_SOURCE_DIR}/models/ DESTINATION ${PACKAGE_DIR}/${DATA_INSTALL_DIR}/krita/ai_models ${MODELS_FILTER})
# install(FILES vision_ml.action DESTINATION ${PACKAGE_DIR}/${DATA_INSTALL_DIR}/krita/actions)
