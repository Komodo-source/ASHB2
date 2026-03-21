@echo off
REM ASHB2 Windows Build Script

echo Setting up vcpkg for Windows...
if not exist "vcpkg\vcpkg.exe" (
    echo Bootstrapping vcpkg...
    cd vcpkg
    call bootstrap-vcpkg.bat
    cd ..
)

echo Installing dependencies with vcpkg...
vcpkg\vcpkg install glfw3

echo Creating build directory...
if not exist "build" mkdir build
cd build

echo Configuring with CMake...
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake

echo Building project...
cmake --build .

echo Build complete! Run app.exe to start the simulation.
pause
