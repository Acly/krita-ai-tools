#!/bin/sh
#
# This script builds part of the Vulkan SDK from source.
# It is meant to be run from the Krita AppImage Docker container.
set -e
cd /home/appimage/appimage-workspace
echo "Building Vulkan SDK from source"

export VULKAN_SDK=/home/appimage/appimage-workspace/vulkan-sdk
export VK_SDK_PATH=/home/appimage/appimage-workspace/vulkan-sdk

if [ -d "vulkan-sdk" ]; then
    echo "Vulkan SDK found, skipping build."
    exit 0
fi

echo "Building Vulkan-Headers"
git clone https://github.com/KhronosGroup/Vulkan-Headers.git
cd Vulkan-Headers/
cmake -S . -B build
cmake --install build --prefix ../vulkan-sdk
cd ..

echo "Building Vulkan-Loader"
git clone https://github.com/KhronosGroup/Vulkan-Loader.git
cd Vulkan-Loader/
cmake -S . -B build -D UPDATE_DEPS=ON
cmake --build build
cmake --install build --prefix ../vulkan-sdk
cd ..

echo "Building glslang"
git clone https://github.com/KhronosGroup/glslang.git
cd glslang/
./update_glslang_sources.py 
cmake . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../vulkan-sdk
cmake --build build
cd ..

echo "Building shaderc (glslc)"
git clone https://github.com/google/shaderc
cd shaderc
./utils/git-sync-deps 
cmake . -B build -G Ninja -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=../vulkan-sdk
cmake --build build
cmake --build build --target install
cd ..

ls -l /home/appimage/appimage-workspace/vulkan-sdk/bin
ls -l /home/appimage/appimage-workspace/vulkan-sdk/lib
/home/appimage/appimage-workspace/vulkan-sdk/bin/glslc --version
