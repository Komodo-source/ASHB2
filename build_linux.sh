#!/usr/bin/env bash
# Build ASHB2 on Linux — including GitHub Codespaces, plain containers and CI
# runners, none of which have a display, a GPU driver or a GUI toolchain.
#
#   ./build_linux.sh              headless build (default) — no GUI dependencies
#   ./build_linux.sh --gui        full build with the window, graphs and inspector
#   ./build_linux.sh --clean      discard cached objects and rebuild everything
#
# The headless binary runs the entire simulation and prints the realism report;
# it is what `--headless <ticks>` has always driven. Only the render path needs
# GLFW/OpenGL/Dear ImGui, and it is compiled out here rather than linked against
# libraries a server does not have. That is why the default build needs nothing
# beyond g++ and make.
set -euo pipefail
cd "$(dirname "$0")"

GUI=0
CLEAN=0
for arg in "$@"; do
    case "$arg" in
        --gui)   GUI=1 ;;
        --clean) CLEAN=1 ;;
        -h|--help)
            sed -n '2,10p' "$0" | sed 's/^# \?//'
            exit 0 ;;
        *) echo "unknown option '$arg' (try --help)" >&2; exit 2 ;;
    esac
done

JOBS=$(nproc 2>/dev/null || echo 4)
OUT=app
OBJDIR=build/linux$([ "$GUI" = 1 ] && echo "-gui" || echo "")

command -v g++ >/dev/null 2>&1 || {
    echo "g++ not found. On Debian/Ubuntu (and Codespaces):" >&2
    echo "  sudo apt-get update && sudo apt-get install -y build-essential" >&2
    exit 1
}

# ── Sources ──────────────────────────────────────────────────────────────────
# The simulation itself. Nothing in this list includes a GUI header.
SRC=(
  src/main.cpp src/WorldSeed.cpp src/world/Noise.cpp src/world/Planet.cpp
  src/world/Lexicon.cpp src/world/ResourceSystem.cpp src/world/Ecosystem.cpp
  src/Entity.cpp src/Drive.cpp src/JungianType.cpp src/Disease.cpp
  src/Logging.cpp src/implem_free_will.cpp src/SaveLoad.cpp src/DbExport.cpp
  src/BackupExport.cpp
  src/Kinship.cpp
  src/SocialOrder.cpp src/SemanticMemory.cpp src/PlanningSystem.cpp
  src/PersonaSystem.cpp src/NarrativeEngine.cpp src/CivilizationEngine.cpp
  src/TechTree.cpp src/QISystem.cpp src/VectorUtility.cpp src/Diplomacy.cpp src/Economics.cpp
  src/items/ItemSystem.cpp src/ai/GoapPlanner.cpp src/ai/neat/Neat.cpp
  src/ai/MindUpgrade.cpp
  src/world/PheromoneField.cpp src/Action.cpp src/CognitiveArchitecture.cpp
  src/EmotionalComplexity.cpp src/EnvironmentalInteraction.cpp
  src/LearningAdaptation.cpp src/LifeCourse.cpp src/SocialDynamics.cpp
  src/validation/ValidationFramework.cpp src/scalability/Scalability.cpp
  src/modules/BehavioralModule.cpp src/environment/EnvironmentModel.cpp
  src/observability/Observability.cpp
)

# The render path, added only for --gui.
GUI_SRC=(
  src/UI.cpp src/world/PlanetView.cpp
  src/header/implot.cpp src/header/implot_items.cpp
  libs/imgui/imgui.cpp libs/imgui/imgui_draw.cpp libs/imgui/imgui_tables.cpp
  libs/imgui/imgui_widgets.cpp libs/imgui/backends/imgui_impl_glfw.cpp
  libs/imgui/backends/imgui_impl_opengl3.cpp
)

INCLUDES=(-Isrc -Isrc/header -Isrc/validation -Isrc/scalability -Isrc/modules
          -Isrc/environment -Isrc/observability -Isrc/world)
CXXFLAGS=(-std=c++17 -O2 -pipe -Wno-deprecated-declarations)
LDLIBS=(-lpthread -lm)
# libdl is folded into glibc from 2.34 on, so link it only when it exists.
if echo 'int main(){}' | g++ -x c++ - -ldl -o /dev/null >/dev/null 2>&1; then
    LDLIBS+=(-ldl)
fi

if [ "$GUI" = 1 ]; then
    # A GUI build needs GLFW and OpenGL headers/libs present. Say exactly what
    # to install rather than dying inside the compiler.
    if ! pkg-config --exists glfw3 2>/dev/null; then
        echo "GUI build needs GLFW and OpenGL development packages." >&2
        echo "On Debian/Ubuntu (and Codespaces):" >&2
        echo "  sudo apt-get update && sudo apt-get install -y libglfw3-dev libgl1-mesa-dev pkg-config" >&2
        echo "Or build the headless version instead: ./build_linux.sh" >&2
        exit 1
    fi
    SRC+=("${GUI_SRC[@]}")
    INCLUDES+=(-Ilibs/imgui -Ilibs/imgui/backends)
    # shellcheck disable=SC2207
    LDLIBS+=($(pkg-config --libs glfw3) -lGL)
else
    CXXFLAGS+=(-DHEADLESS)
fi

[ "$CLEAN" = 1 ] && rm -rf "$OBJDIR"
mkdir -p "$OBJDIR"

echo "=== Building ASHB2 ($([ "$GUI" = 1 ] && echo GUI || echo headless), $JOBS jobs) ==="

# ── Compile ──────────────────────────────────────────────────────────────────
# One object per source, in parallel, skipped when the object is newer than its
# source: a one-line edit costs seconds instead of the four-minute rebuild a
# single g++ invocation over forty translation units used to take.
compile_one() {
    src="$1"; obj="$2"; shift 2
    if [ -f "$obj" ] && [ "$obj" -nt "$src" ]; then exit 0; fi
    mkdir -p "$(dirname "$obj")"
    echo "  CXX $src"
    g++ "$@" -c "$src" -o "$obj"
}
export -f compile_one

OBJS=()
JOBFILE=$(mktemp)
trap 'rm -f "$JOBFILE"' EXIT
for src in "${SRC[@]}"; do
    [ -f "$src" ] || { echo "missing source: $src" >&2; exit 1; }
    obj="$OBJDIR/${src//\//_}.o"
    OBJS+=("$obj")
    printf '%s\n' "$src|$obj" >> "$JOBFILE"
done

# xargs returns non-zero if any child failed, and -e stops us here.
tr '|' '\n' < "$JOBFILE" | xargs -P "$JOBS" -n 2 bash -c \
    'compile_one "$0" "$1" '"${CXXFLAGS[*]} ${INCLUDES[*]}"

echo "  LD  $OUT"
g++ "${OBJS[@]}" "${LDLIBS[@]}" -o "$OUT"

echo ""
echo "=== Build complete: ./$OUT ==="
if [ "$GUI" = 1 ]; then
    echo "Run windowed:  ./$OUT"
else
    echo "This build has no window (that is the point on a server)."
fi
echo "Run a world:   ./$OUT --headless 1000 --seed mars --entities 60 --region 1 --chaos 1.3"
echo "Resume later:  ./$OUT --headless 1000 --save-every 250 --save-file myworld.txt"
echo "               ./$OUT --headless 500 --load myworld.txt"
