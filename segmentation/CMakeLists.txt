# dlimgedit dependency
add_library(dlimgedit_dynamic INTERFACE IMPORTED)

include(FetchContent)
if(WIN32)
    # Download binaries pre-built with MSVC (onnxruntime does not support mingw)
    FetchContent_Declare(dlimgedit
        DOWNLOAD_EXTRACT_TIMESTAMP true
        URL https://github.com/Acly/dlimgedit/releases/download/v1.0.0/dlimgedit-windows-x64-1.0.0.zip
    )
    FetchContent_MakeAvailable(dlimgedit)
    set_target_properties(dlimgedit_dynamic PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${dlimgedit_SOURCE_DIR}/include)
else()
    # Build from source to ensure same system compatibility
    FetchContent_Declare(dlimgedit
        DOWNLOAD_EXTRACT_TIMESTAMP true
        GIT_REPOSITORY https://github.com/Acly/dlimgedit.git
        GIT_TAG        5a6f44ac9e8c7c2156d2d55ff3f91a739b841765
    )
    set(BUILD_SHARED_LIBS_OLD ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_MakeAvailable(dlimgedit)
    set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_OLD})
    set_target_properties(dlimgedit_dynamic PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${dlimgedit_SOURCE_DIR}/src/include)
endif()


# Segmentation tools plugin

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
target_link_libraries(kritatoolsegmentation PRIVATE kritaui kritabasicflakes kritaimage dlimgedit_dynamic $<$<PLATFORM_ID:Linux>:dl>)

# Install
set(MODELS_FILTER FILES_MATCHING PATTERN "mobile_sam_image_encoder.onnx" PATTERN "sam_mask_decoder_single.onnx")
set(ONNX_FILTER FILES_MATCHING PATTERN "*onnxruntime.*")

# Install into krita install prefix to run & test
install(TARGETS kritatoolsegmentation DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
if(WIN32)
    install(DIRECTORY ${dlimgedit_SOURCE_DIR}/bin/ DESTINATION ${BIN_INSTALL_DIR})
else()
    get_target_property(ONNXRUNTIME_LIB_FILE onnxruntime IMPORTED_LOCATION)
    cmake_path(GET ONNXRUNTIME_LIB_FILE PARENT_PATH ONNXRUNTIME_LIB_DIR)
    install(TARGETS dlimgedit DESTINATION ${KRITA_PLUGIN_INSTALL_DIR}/toolsegmentation)
    install(DIRECTORY ${ONNXRUNTIME_LIB_DIR}/ DESTINATION ${KRITA_PLUGIN_INSTALL_DIR}/toolsegmentation ${ONNX_FILTER})
endif()    
install(DIRECTORY ${dlimgedit_SOURCE_DIR}/models/ DESTINATION ${DATA_INSTALL_DIR}/krita/ai_models ${MODELS_FILTER})
install(FILES segmentation_tools.action DESTINATION ${DATA_INSTALL_DIR}/krita/actions)

# Install into isolated directory to distribute just the plugin
if(WIN32)
    set(PACKAGE_DIR ${CMAKE_INSTALL_PREFIX}/../krita-plugin-segmentation)
    install(DIRECTORY ${dlimgedit_SOURCE_DIR}/bin/ DESTINATION ${PACKAGE_DIR}/${BIN_INSTALL_DIR})
    install(TARGETS kritatoolsegmentation DESTINATION ${PACKAGE_DIR}/${KRITA_PLUGIN_INSTALL_DIR})
else() # Linux AppImage
    set(PACKAGE_DIR ${CMAKE_INSTALL_PREFIX}/../krita-plugin-segmentation/usr)
    install(TARGETS dlimgedit DESTINATION ${PACKAGE_DIR}/lib/kritaplugins/toolsegmentation)
    install(DIRECTORY ${ONNXRUNTIME_LIB_DIR}/ DESTINATION ${PACKAGE_DIR}/lib/kritaplugins/toolsegmentation ${ONNX_FILTER})
    install(TARGETS kritatoolsegmentation DESTINATION ${PACKAGE_DIR}/lib/kritaplugins)
endif()
install(DIRECTORY ${dlimgedit_SOURCE_DIR}/models/ DESTINATION ${PACKAGE_DIR}/${DATA_INSTALL_DIR}/krita/ai_models ${MODELS_FILTER})
install(FILES segmentation_tools.action DESTINATION ${PACKAGE_DIR}/${DATA_INSTALL_DIR}/krita/actions)
