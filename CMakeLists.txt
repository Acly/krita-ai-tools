
# Add vision.cpp (and GGML) dependency

if(NOT APPLE)
    set(VISP_VULKAN ON)
    set(_vulkan_target ggml-vulkan)
endif()

add_subdirectory(vision.cpp)

foreach(tgt ggml ggml-cpu ggml-base ${_vulkan_target})
    target_compile_options(${tgt} PRIVATE
        -Wno-cast-align -Wno-sign-compare -Wno-macro-redefined -Wno-unused-parameter
        -Wno-missing-prototypes -Wno-undef -Wno-unreachable-code)
endforeach()

# Add tools plugin

add_subdirectory(src)
