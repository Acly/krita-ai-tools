
# Add vision.cpp (and GGML) dependency

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

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

# Download models

message(STATUS "[VisionML Plugin] Downloading for models/MobileSAM-F16.gguf")
file(DOWNLOAD
  "https://huggingface.co/Acly/MobileSAM-GGUF/resolve/main/MobileSAM-F16.gguf"
  ${CMAKE_CURRENT_LIST_DIR}/vision.cpp/models/sam/MobileSAM-F16.gguf
  EXPECTED_HASH "SHA256=e556f62189f632608090fbe5a6979ce79ce9dcaa2418a28660a31b87cc674584"
)
message(STATUS "[VisionML Plugin] Downloading for models/BiRefNet-lite-F16.gguf")
file(DOWNLOAD
  "https://huggingface.co/Acly/BiRefNet-GGUF/resolve/main/BiRefNet-lite-F16.gguf"
  ${CMAKE_CURRENT_LIST_DIR}/vision.cpp/models/birefnet/BiRefNet-lite-F16.gguf
  EXPECTED_HASH "SHA256=33eb4c4b045f4cbb179afe261e5ba2c0e5b28b5ca6033aa68ca00f050dd027e6"
)
message(STATUS "[VisionML Plugin] Downloading for models/MIGAN-512-places2-F16.gguf")
file(DOWNLOAD
  "https://huggingface.co/Acly/MIGAN-GGUF/resolve/main/MIGAN-512-places2-F16.gguf"
  ${CMAKE_CURRENT_LIST_DIR}/vision.cpp/models/migan/MIGAN-512-places2-F16.gguf
  EXPECTED_HASH "SHA256=c9f241e96fb5a791f9494fc7d4c2dd793297ae95f05b8423f547d19bea465b81"
)

# Installation

# Install into python extension directory
# * can be installed like a Python plugin for deployment
# * can be symlinked into the user pykrita folder for development
set(install_dir ${CMAKE_INSTALL_PREFIX}/krita-ai-tools)

set(lib_files $<TARGET_FILE:kritavisionml>)
foreach(tgt ${ggml_targets})
    list(APPEND lib_files $<TARGET_FILE:${tgt}>)
endforeach()
              
install(FILES ${lib_files} DESTINATION ${install_dir}/ai_tools/lib)
install(DIRECTORY python/ DESTINATION ${install_dir}/ai_tools) 
install(DIRECTORY vision.cpp/models/ DESTINATION ${install_dir}/ai_tools/models FILES_MATCHING PATTERN "*.gguf")
install(FILES src/vision_tools.action src/vision_filters.action DESTINATION ${install_dir}/ai_tools)
install(FILES ai_tools.desktop DESTINATION ${install_dir})

if(WIN32)
  find_file(libomp NAMES libomp.dll REQUIRED)
  install(FILES ${libomp} DESTINATION ${install_dir}/ai_tools/lib)
  install(FILES $<TARGET_FILE:visioncpp> DESTINATION ${install_dir}/ai_tools/lib)
else()
  install(FILES $<TARGET_FILE:visioncpp> DESTINATION ${install_dir}/ai_tools/lib RENAME libvisioncpp.so.0)
endif()
