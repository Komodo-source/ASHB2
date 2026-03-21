#!/bin/bash

# ASHB2 Linux Build Script

set -e

echo "Setting up vcpkg for Linux..."
if [ ! -f "vcpkg/vcpkg" ]; then
    cd vcpkg
    ./bootstrap-vcpkg.sh
    cd ..
fi

echo "Installing dependencies with vcpkg..."
./vcpkg/vcpkg install glfw3

echo "Creating build directory..."
mkdir -p build
cd build

echo "Configuring with CMake..."
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake

echo "Building project..."
cmake --build .

echo "Build complete! Run ./app to start the simulation."
