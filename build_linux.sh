#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

JOBS=$(nproc 2>/dev/null || echo 4)

echo "=== Building ASHB2 (Linux, $JOBS jobs) ==="

g++ -std=c++17 -O2 \
  -DHEADLESS \
  src/main.cpp src/WorldSeed.cpp src/world/Noise.cpp src/world/Planet.cpp \
  src/world/PlanetView.cpp src/world/Lexicon.cpp src/world/ResourceSystem.cpp \
  src/world/Ecosystem.cpp src/UI.cpp src/Entity.cpp src/Drive.cpp \
  src/JungianType.cpp src/Disease.cpp src/Logging.cpp src/implem_free_will.cpp \
  src/SaveLoad.cpp src/Kinship.cpp src/SocialOrder.cpp src/SDLEngine.cpp \
  src/Image.cpp src/SemanticMemory.cpp src/PlanningSystem.cpp \
  src/PersonaSystem.cpp src/NarrativeEngine.cpp src/CivilizationEngine.cpp \
  src/TechTree.cpp src/Diplomacy.cpp src/Economics.cpp \
  src/items/ItemSystem.cpp src/ai/GoapPlanner.cpp src/ai/neat/Neat.cpp \
  src/world/PheromoneField.cpp src/Action.cpp src/CognitiveArchitecture.cpp \
  src/EmotionalComplexity.cpp src/EnvironmentalInteraction.cpp \
  src/LearningAdaptation.cpp src/LifeCourse.cpp src/SocialDynamics.cpp \
  src/validation/ValidationFramework.cpp src/scalability/Scalability.cpp \
  src/modules/BehavioralModule.cpp src/environment/EnvironmentModel.cpp \
  src/observability/Observability.cpp \
  libs/imgui/imgui.cpp libs/imgui/imgui_draw.cpp libs/imgui/imgui_tables.cpp \
  libs/imgui/imgui_widgets.cpp libs/imgui/backends/imgui_impl_glfw.cpp \
  libs/imgui/backends/imgui_impl_opengl3.cpp \
  src/header/implot.cpp src/header/implot_items.cpp \
  -Ilibs/imgui -Ilibs/imgui/backends \
  -Isrc/header -Isrc/validation -Isrc/scalability -Isrc/modules \
  -Isrc/environment -Isrc/observability -Isrc/world \
  -I/usr/local/include \
  -lpthread -ldl -lm -lGL \
  -o app

echo ""
echo "=== Build complete! ==="
echo "Run: ./app --headless 500 --seed 42 --entities 40 --region 1 --chaos 1.3"
