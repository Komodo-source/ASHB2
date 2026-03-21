cmake_minimum_required(VERSION 3.10)
project(ASHB2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use vcpkg if available
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Find packages
find_package(glfw3 CONFIG REQUIRED)
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
src/Movement.cpp
src/SaveLoad.cpp
src/Heritage.cpp
src/Graph.cpp


)

# Lier les bibliothèques
if(WIN32)
    target_link_libraries(app
        glfw
        OpenGL::GL
        gdi32
    )
else()
    target_link_libraries(app
        glfw
        OpenGL::GL
        ${CMAKE_DL_LIBS}
        pthread
    )
endif()
