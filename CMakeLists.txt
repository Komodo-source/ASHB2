cmake_minimum_required(VERSION 3.10)
project(ASHB2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use vcpkg if available, otherwise use system packages
if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Find packages - try vcpkg first, then system
find_package(glfw3 CONFIG QUIET)
if(NOT glfw3_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    message(STATUS "Using system GLFW")
endif()

find_package(OpenGL REQUIRED)

# Chemins vers les bibliothèques locales (fallback)
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/libs/imgui")

# Fichiers ImGui
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    src/header/implot.cpp
    src/header/implot_items.cpp
)

# Include directories
include_directories(
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Créer l'exécutable
add_executable(app
src/main.cpp
src/UI.cpp
src/Entity.cpp
src/Disease.cpp
src/Logging.cpp
${IMGUI_SOURCES}
src/implem_free_will.cpp
src/SpatialMesh.cpp
src/movement.cpp
src/SaveLoad.cpp
src/Heritage.cpp
src/Graph.cpp


)

# Lier les bibliothèques
if(TARGET glfw)
    # vcpkg GLFW
    target_link_libraries(app PRIVATE glfw)
elseif(GLFW_LIBRARIES)
    # System GLFW (via pkg-config)
    target_link_libraries(app PRIVATE ${GLFW_LIBRARIES})
    target_include_directories(app PRIVATE ${GLFW_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "GLFW not found!")
endif()

# Common libraries
target_link_libraries(app PRIVATE OpenGL::GL)

if(WIN32)
    target_link_libraries(app PRIVATE gdi32)
else()
    target_link_libraries(app PRIVATE ${CMAKE_DL_LIBS} pthread)
endif()
