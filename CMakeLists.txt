cmake_minimum_required(VERSION 3.10)
project(ASHB2)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Chemins vers les bibliothèques
set(GLFW_DIR "${CMAKE_SOURCE_DIR}/libs/glfw")
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
    ${GLFW_DIR}/include
)

# Link directories
link_directories(${GLFW_DIR}/lib-mingw-w64)  # Ajustez selon votre compilateur

find_package(OpenGL REQUIRED)

# Créer l'exécutable
add_executable(app
src/main.cpp
src/UI.cpp
src/Entity.cpp
src/Disease.cpp
${IMGUI_SOURCES}
src/implem_free_will.cpp
src/SpatialMesh.cpp
src/Movement.cpp
src/SaveLoad.cpp

)

# Lier les bibliothèques
target_link_libraries(app
    ${GLFW_DIR}/lib-mingw-w64/libglfw3.a
    OpenGL::GL
    gdi32
)
