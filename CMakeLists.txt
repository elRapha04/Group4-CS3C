cmake_minimum_required(VERSION 3.10)
project(AutomataSimulator)

set(CMAKE_CXX_STANDARD 17)

# --- Dependencies ---
# Ideally, you have ImGui/GLFW installed or submoduled.
# This script assumes a standard "local" setup or system libs.

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

# Include directories
include_directories(src)
include_directories(src/gui)
include_directories(src/lexer)
include_directories(src/parser)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

# ImGui Sources (User must provide these in dependencies/imgui)
# We assume they are in dependencies/imgui
include_directories(dependencies/imgui)
include_directories(dependencies/imgui/backends)

set(IMGUI_SOURCES
    dependencies/imgui/imgui.cpp
    dependencies/imgui/imgui_draw.cpp
    dependencies/imgui/imgui_tables.cpp
    dependencies/imgui/imgui_widgets.cpp
    dependencies/imgui/imgui_demo.cpp
    dependencies/imgui/backends/imgui_impl_glfw.cpp
    dependencies/imgui/backends/imgui_impl_opengl3.cpp
)

# Executable
add_executable(AutomataSimulator ${SOURCES} ${IMGUI_SOURCES})

# Limits
target_link_libraries(AutomataSimulator glfw OpenGL::GL)
