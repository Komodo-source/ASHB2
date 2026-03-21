#!/bin/bash

# ASHB2 Cross-Platform Build Script
# Works on Linux and macOS (with WSL on Windows)

set -e

echo "Setting up vcpkg..."

# Check if vcpkg is already set up
if [ ! -f "vcpkg/vcpkg" ]; then
    echo "Bootstrapping vcpkg..."
    cd vcpkg

    # Detect platform and use appropriate bootstrap script
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Check if we're on Arch Linux
        if [ -f "/etc/arch-release" ]; then
            echo "Detected Arch Linux"
            echo "Note: For Arch Linux, consider using build_arch.sh instead for system packages"
            echo "Continuing with vcpkg bootstrap..."

            # Try to use system vcpkg first
            if command -v vcpkg >/dev/null 2>&1; then
                echo "Using system vcpkg"
                cd ..
                # Create a symlink to system vcpkg
                SYSTEM_VCPKG_PATH=$(which vcpkg)
                ln -sf "$SYSTEM_VCPKG_PATH" vcpkg/vcpkg
            else
                echo "System vcpkg not found. Installing vcpkg via pacman..."
                echo "Please run: sudo pacman -S vcpkg"
                echo "Or install build dependencies and bootstrap manually:"
                echo "  sudo pacman -S cmake ninja"
                echo "  cd vcpkg && ./bootstrap-vcpkg.sh"
                echo "Or use build_arch.sh for system packages instead"
                exit 1
            fi
        else
            echo "Detected Linux, using bootstrap-vcpkg.sh"
            chmod +x bootstrap-vcpkg.sh
            ./bootstrap-vcpkg.sh
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "Detected macOS, using bootstrap-vcpkg.sh"
        chmod +x bootstrap-vcpkg.sh
        ./bootstrap-vcpkg.sh
    else
        echo "Error: Unsupported platform $OSTYPE"
        echo "This script is designed for Linux and macOS."
        echo "For Windows, please use the Windows build instructions in README.md"
        exit 1
    fi

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
