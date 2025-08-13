
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
  EXPECTED_HASH "SHA256=b546366475e3ad744bb2eaf7634df88e9aaf25f6622797d2de300f5a530831f7"
)
message(STATUS "[VisionML Plugin] Downloading for models/BiRefNet-lite-F16.gguf")
file(DOWNLOAD
  "https://huggingface.co/Acly/BiRefNet-GGUF/resolve/main/BiRefNet-lite-F16.gguf"
  ${CMAKE_CURRENT_LIST_DIR}/vision.cpp/models/birefnet/BiRefNet-lite-F16.gguf
  EXPECTED_HASH "SHA256=7b5397a2c98d66677f8f74317774bbeac49dbb321b8a3dc744af913db71d4fa5"
)
message(STATUS "[VisionML Plugin] Downloading for models/MIGAN-512-places2-F16.gguf")
file(DOWNLOAD
  "https://huggingface.co/Acly/MIGAN-GGUF/resolve/main/MIGAN-512-places2-F16.gguf"
  ${CMAKE_CURRENT_LIST_DIR}/vision.cpp/models/migan/MIGAN-512-places2-F16.gguf
  EXPECTED_HASH "SHA256=3e47592bf716d0dc306f8dc02d4476cfcdaf2c055fa3c3c8e0ced4db775eb64b"
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
              
install(FILES ${lib_files} DESTINATION ${install_dir}/vision_tools/lib)
install(DIRECTORY python/ DESTINATION ${install_dir}/vision_tools) 
install(DIRECTORY vision.cpp/models/ DESTINATION ${install_dir}/vision_tools/models FILES_MATCHING PATTERN "*.gguf")
install(FILES src/vision_tools.action src/vision_filters.action DESTINATION ${install_dir}/vision_tools)
install(FILES vision_tools.desktop DESTINATION ${install_dir})

if(WIN32)
  find_file(libomp NAMES libomp.dll REQUIRED)
  install(FILES ${libomp} DESTINATION ${install_dir}/vision_tools/lib)
  install(FILES $<TARGET_FILE:visioncpp> DESTINATION ${install_dir}/vision_tools/lib)
else()
  install(FILES $<TARGET_FILE:visioncpp> DESTINATION ${install_dir}/vision_tools/lib RENAME libvisioncpp.so.0)
endif()
