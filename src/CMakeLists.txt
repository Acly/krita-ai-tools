
set(kritavisionml_SOURCES
    VisionML.cpp
    VisionMLPlugin.cpp
    filters/BackgroundRemovalFilter.cpp
    inpaint/InpaintTool.cpp
    segmentation/SegmentationToolHelper.cpp
    segmentation/SelectSegmentFromPointTool.cpp
    segmentation/SelectSegmentFromRectTool.cpp
    ../../tools/selectiontools/kis_selection_modifier_mapper.cc
)

qt5_add_resources(kritavisionml_SOURCES ../icons/kritavisionml.qrc)

kis_add_library(kritavisionml MODULE ${kritavisionml_SOURCES})

generate_export_header(kritavisionml BASE_NAME kritavisionml)

target_include_directories(kritavisionml PRIVATE . ../../tools/selectiontools)
target_compile_features(kritavisionml PRIVATE cxx_std_20)
target_link_libraries(kritavisionml PRIVATE kritaui kritabasicflakes kritaimage visioncpp)
