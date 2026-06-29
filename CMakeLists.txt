cmake_minimum_required(VERSION 3.10)
project(ASHB2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable OpenMP for parallelization if available
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    add_compile_options(${OpenMP_CXX_FLAGS})
endif()

# Chemins vers les bibliothèques
set(GLFW_DIR "${CMAKE_SOURCE_DIR}/libs/glfw")
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/libs/imgui")
set(SDL2_DIR "${CMAKE_SOURCE_DIR}/libs/SDL2")

# Fichiers ImGui (optional - only if libraries exist)
if(EXISTS ${IMGUI_DIR}/imgui.cpp)
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
else()
    set(IMGUI_SOURCES "")
endif()

# Include directories
include_directories(
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLFW_DIR}/include
    ${SDL2_DIR}/include
    ${SDL2_DIR}/include/SDL2
    src/header
    src/validation
    src/scalability
    src/modules
    src/environment
    src/observability
    src/world
)

# Link directories
link_directories(${GLFW_DIR}/lib-mingw-w64 ${SDL2_DIR}/lib)

# OpenGL is optional - only needed for visualization
# find_package(OpenGL REQUIRED)
find_package(OpenGL QUIET)

# New modular source files
set(VALIDATION_SOURCES
    src/validation/ValidationFramework.cpp
)

set(SCALABILITY_SOURCES
    src/scalability/Scalability.cpp
)

set(MODULES_SOURCES
    src/modules/BehavioralModule.cpp
)

set(ENVIRONMENT_SOURCES
    src/environment/EnvironmentModel.cpp
)

set(OBSERVABILITY_SOURCES
    src/observability/Observability.cpp
)

# Créer l'exécutable
add_executable(app
src/main.cpp
src/WorldSeed.cpp
src/world/Noise.cpp
src/world/Planet.cpp
src/world/PlanetView.cpp
src/world/Lexicon.cpp
src/world/ResourceSystem.cpp
src/world/Ecosystem.cpp
src/UI.cpp
src/Entity.cpp
src/Drive.cpp
src/JungianType.cpp
src/Disease.cpp
src/Logging.cpp
${IMGUI_SOURCES}
src/implem_free_will.cpp
src/SaveLoad.cpp
src/Heritage.cpp
src/Kinship.cpp
src/SocialOrder.cpp
src/Graph.cpp
src/SDLEngine.cpp
src/Image.cpp
src/SemanticMemory.cpp
src/PlanningSystem.cpp
src/PersonaSystem.cpp
src/NarrativeEngine.cpp
src/CivilizationEngine.cpp
src/TechTree.cpp
src/Diplomacy.cpp
src/Economics.cpp

# New AI / cognitive simulation source files
src/Action.cpp
src/CognitiveArchitecture.cpp
src/EmotionalComplexity.cpp
src/EnvironmentalInteraction.cpp
src/LearningAdaptation.cpp
src/LifeCourse.cpp
src/SocialDynamics.cpp

# Modular components
${VALIDATION_SOURCES}
${SCALABILITY_SOURCES}
${MODULES_SOURCES}
${ENVIRONMENT_SOURCES}
${OBSERVABILITY_SOURCES}


)

# Lier les bibliothèques
target_link_libraries(app
    ${GLFW_DIR}/lib-mingw-w64/libglfw3.a
    gdi32
    mingw32
    SDL2main
    SDL2
    SDL2_image
)

# OpenGL linking (only if found)
if(OpenGL_FOUND)
    target_link_libraries(app OpenGL::GL)
endif()

# Link OpenMP if available
if(OpenMP_CXX_FOUND)
    target_link_libraries(app OpenMP::OpenMP_CXX)
endif()

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(app Threads::Threads)
