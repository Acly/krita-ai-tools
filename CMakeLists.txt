
# Add vision.cpp (and GGML) dependency

set(ggml_targets ggml ggml-cpu ggml-base)

if(NOT APPLE)
    set(VISP_VULKAN ON)
    list(APPEND ggml_targets ggml-vulkan)
endif()

set(VISP_CI ON) # deployment build
set(VISP_FMT_LIB ON) # C++20 <format> not supported in Krita's Linux build environment
add_subdirectory(vision.cpp)

foreach(tgt ${ggml_targets})
    target_compile_options(${tgt} PRIVATE
        -Wno-cast-align -Wno-sign-compare -Wno-macro-redefined -Wno-unused-parameter
        -Wno-missing-prototypes -Wno-undef -Wno-unreachable-code)
endforeach()

# Add krita plugin library

add_subdirectory(src)

# Installation

# Install into python extension directory
# * can be installed like a Python plugin for deployment
# * can be symlinked into the user pykrita folder for development
set(install_dir ${CMAKE_INSTALL_PREFIX}/krita-ai-tools)

set(lib_files $<TARGET_FILE:kritavisionml> $<TARGET_FILE:visioncpp>)
foreach(tgt ${ggml_targets})
    list(APPEND lib_files $<TARGET_FILE:${tgt}>)
endforeach()
              
install(FILES ${lib_files} DESTINATION ${install_dir}/ai_tools/lib)
install(DIRECTORY python/ DESTINATION ${install_dir}/ai_tools) 
install(FILES src/vision_tools.action src/vision_filters.action DESTINATION ${install_dir}/ai_tools)
install(FILES ai_tools.desktop DESTINATION ${install_dir})

if(WIN32)
    find_file(libomp NAMES libomp.dll REQUIRED)
    install(FILES ${libomp} DESTINATION ${install_dir}/ai_tools/lib)
endif()
