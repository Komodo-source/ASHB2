#!/bin/bash

# ASHB2 Build Script for Arch Linux (using system packages)
# This script uses system-installed packages instead of vcpkg

set -e

echo "ASHB2 Arch Linux Build Script (System Packages)"
echo "=============================================="

# Check if we're on Arch Linux
if [ ! -f "/etc/arch-release" ]; then
    echo "Error: This script is designed for Arch Linux."
    echo "For other distributions, use build_linux.sh"
    exit 1
fi

echo "Installing dependencies..."
sudo pacman -S --needed \
    cmake \
    ninja \
    glfw-x11 \
    glu \
    base-devel

echo "Creating build directory..."
mkdir -p build
cd build

echo "Configuring with CMake (system packages)..."
cmake .. -G Ninja

echo "Building project..."
ninja

echo "Build complete! Run ./app to start the simulation."
