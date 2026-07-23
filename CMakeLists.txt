cmake_minimum_required(VERSION 3.10)
project(ASHB2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build options ─────────────────────────────────────────────────────────────
option(HEADLESS "Build without GUI (no GLFW/OpenGL/ImGui)" OFF)

# Default to an optimized build — the simulation was shipping at -O0, which
# cost roughly an order of magnitude of tick throughput. Use
# `cmake -DCMAKE_BUILD_TYPE=Debug` when you actually need a debugger.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Enable OpenMP for parallelization if available
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    add_compile_options(${OpenMP_CXX_FLAGS})
endif()

# ── Compile definition for headless mode ──────────────────────────────────────
if(HEADLESS)
    add_definitions(-DHEADLESS)
    message(STATUS "HEADLESS mode: building without GLFW/OpenGL/SDL/ImGui GUI")
endif()

# ── GUI libraries (only needed for interactive mode) ──────────────────────────
if(NOT HEADLESS)
    set(GLFW_DIR "${CMAKE_SOURCE_DIR}/libs/glfw")
    set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/libs/imgui")
    # ImGui sources
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

    # Include directories for GUI libs
    include_directories(
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${GLFW_DIR}/include
    )

    # Link directories for GUI libs
    link_directories(${GLFW_DIR}/lib-mingw-w64)

    # OpenGL (optional, only for GUI)
    find_package(OpenGL QUIET)

    # GUI source files (UI)
    set(GUI_SOURCES
        src/UI.cpp
    )
else()
    set(IMGUI_SOURCES "")
    set(GUI_SOURCES "")
endif()

# Common include directories (always needed)
include_directories(
    src/header
    src/validation
    src/scalability
    src/modules
    src/environment
    src/observability
    src/world
)

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

# Create the executable
add_executable(app
    src/main.cpp
    src/WorldSeed.cpp
    src/world/Noise.cpp
    src/world/Planet.cpp
    src/world/PlanetView.cpp
    src/world/Lexicon.cpp
    src/world/ResourceSystem.cpp
    src/world/Ecosystem.cpp
    src/Entity.cpp
    src/Drive.cpp
    src/JungianType.cpp
    src/Disease.cpp
    src/Logging.cpp
    ${IMGUI_SOURCES}
    src/implem_free_will.cpp
    src/SaveLoad.cpp
    src/Kinship.cpp
    src/SocialOrder.cpp
    ${GUI_SOURCES}
    src/SemanticMemory.cpp
    src/PlanningSystem.cpp
    src/PersonaSystem.cpp
    src/NarrativeEngine.cpp
    src/CivilizationEngine.cpp
    src/TechTree.cpp
    src/Diplomacy.cpp
    src/Economics.cpp

    # Emergence upgrade (Steps 2-5): data-driven items, GOAP, stigmergy, NEAT
    src/items/ItemSystem.cpp
    src/ai/GoapPlanner.cpp
    src/ai/neat/Neat.cpp
    # AI upgrade phases A-E (plans/ai-upgrade-2026-07.md)
    src/ai/MindUpgrade.cpp
    src/world/PheromoneField.cpp

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

# ── Link libraries ────────────────────────────────────────────────────────────
if(NOT HEADLESS)
    target_link_libraries(app
        ${GLFW_DIR}/lib-mingw-w64/libglfw3.a
        gdi32
        mingw32
    )

    # OpenGL linking (only if found)
    if(OpenGL_FOUND)
        target_link_libraries(app OpenGL::GL)
    endif()
endif()

# Link OpenMP if available
if(OpenMP_CXX_FOUND)
    target_link_libraries(app OpenMP::OpenMP_CXX)
endif()

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(app Threads::Threads)
