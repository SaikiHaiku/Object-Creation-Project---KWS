cmake_minimum_required(VERSION 3.20)
project(OCP VERSION 3.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MINGW)
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-variable -Wno-unused-parameter -Wno-missing-field-initializers)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")

# ============================================
# Third-party dependencies
# ============================================

# GLFW (prebuilt)
add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${DEPS_DIR}/glfw/glfw-3.4.bin.WIN64/lib-mingw-w64/libglfw3.a"
    INTERFACE_INCLUDE_DIRECTORIES "${DEPS_DIR}/glfw/glfw-3.4.bin.WIN64/include"
)

# GLAD (OpenGL 3.3 Core loader)
add_library(glad STATIC "${DEPS_DIR}/glad/src/gl.c")
target_include_directories(glad PUBLIC "${DEPS_DIR}/glad/include")

# Dear ImGui
set(IMGUI_DIR "${DEPS_DIR}/imgui/imgui-1.91.8")
add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
target_link_libraries(imgui PUBLIC glad glfw)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)

# GLM (header-only)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "${DEPS_DIR}/glm/glm-1.0.1")

# stb (header-only)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${DEPS_DIR}/stb")

# ============================================
# OCP Engine - Modular Architecture
#
# Dependency graph (each module depends only on those below it):
#
#   ocp_app (executable)
#       |
#   ocp_ui <-> ocp_plugin <-> ocp_platform
#       |         |              |
#   ocp_ai   ocp_editor      ocp_io
#       |         |              |
#   ocp_renderer               |
#       |                      |
#   ocp_scene_graph            |
#       |                      |
#   ocp_geometry  <------------+
#       |
#   ocp_core (foundation)
#
# ============================================

# Collect all include dirs for backward-compatible includes.
# Each module directory is added so that #include "math_utils.h" still works
# even though the file moved to core/math_utils.h.
set(MODULE_DIRS
    "${SRC_DIR}/core"
    "${SRC_DIR}/geometry"
    "${SRC_DIR}/scene_graph"
    "${SRC_DIR}/renderer"
    "${SRC_DIR}/ai"
    "${SRC_DIR}/editor"
    "${SRC_DIR}/ui"
    "${SRC_DIR}/io"
    "${SRC_DIR}/platform"
    "${SRC_DIR}/plugin"
    "${SRC_DIR}/app"
)

# Core module (math, types, camera, event bus, plugin API)
file(GLOB CORE_SRC "${SRC_DIR}/core/*.cpp")
add_library(ocp_core STATIC ${CORE_SRC})
target_include_directories(ocp_core PUBLIC ${MODULE_DIRS})
target_link_libraries(ocp_core PUBLIC glm)

# Geometry module (mesh, BMesh, primitives)
file(GLOB GEOM_SRC "${SRC_DIR}/geometry/*.cpp")
add_library(ocp_geometry STATIC ${GEOM_SRC})
target_include_directories(ocp_geometry PUBLIC ${MODULE_DIRS})
target_link_libraries(ocp_geometry PUBLIC ocp_core)

# Scene module (scene graph, serialization)
file(GLOB SCENE_SRC "${SRC_DIR}/scene_graph/*.cpp")
add_library(ocp_scene STATIC ${SCENE_SRC})
target_include_directories(ocp_scene PUBLIC ${MODULE_DIRS})
target_link_libraries(ocp_scene PUBLIC ocp_geometry)

# Renderer module (OpenGL rendering, shaders)
file(GLOB RENDERER_SRC "${SRC_DIR}/renderer/*.cpp")
add_library(ocp_renderer STATIC ${RENDERER_SRC})
target_include_directories(ocp_renderer PUBLIC ${MODULE_DIRS})
target_link_libraries(ocp_renderer PUBLIC ocp_scene glad imgui)

# AI module (prompt engine, decomposer, parametric builder)
file(GLOB AI_SRC "${SRC_DIR}/ai/*.cpp")
add_library(ocp_ai STATIC ${AI_SRC})
target_include_directories(ocp_ai PUBLIC ${MODULE_DIRS})
target_link_libraries(ocp_ai PUBLIC ocp_scene ocp_geometry)

# Editor module (edit tools, sculpt tools)
file(GLOB EDITOR_SRC "${SRC_DIR}/editor/*.cpp")
add_library(ocp_editor STATIC ${EDITOR_SRC})
target_include_directories(ocp_editor PUBLIC ${MODULE_DIRS})
target_link_libraries(ocp_editor PUBLIC ocp_geometry)

# IO module (import/export)
file(GLOB IO_SRC "${SRC_DIR}/io/*.cpp")
add_library(ocp_io STATIC ${IO_SRC})
target_include_directories(ocp_io PUBLIC ${MODULE_DIRS})
target_link_libraries(ocp_io PUBLIC ocp_scene ocp_renderer)

# Platform module (OS abstraction)
file(GLOB PLATFORM_SRC "${SRC_DIR}/platform/*.cpp")
add_library(ocp_platform STATIC ${PLATFORM_SRC})
target_include_directories(ocp_platform PUBLIC ${MODULE_DIRS})
target_link_libraries(ocp_platform PUBLIC ocp_core)

# Plugin module (plugin API, plugin manager)
file(GLOB PLUGIN_SRC "${SRC_DIR}/plugin/*.cpp")
add_library(ocp_plugin STATIC ${PLUGIN_SRC})
target_include_directories(ocp_plugin PUBLIC ${MODULE_DIRS})
target_link_libraries(ocp_plugin PUBLIC ocp_core)

# UI module (ImGui panels, input handling)
file(GLOB UI_SRC "${SRC_DIR}/ui/*.cpp")
add_library(ocp_ui STATIC ${UI_SRC})
target_include_directories(ocp_ui PUBLIC ${MODULE_DIRS})
target_link_libraries(ocp_ui PUBLIC
    ocp_scene ocp_renderer ocp_ai ocp_editor
    ocp_platform ocp_plugin ocp_io imgui
)

# Application (main loop, initialization)
file(GLOB APP_SRC "${SRC_DIR}/app/*.cpp")
add_executable(OCP WIN32 ${APP_SRC})
target_include_directories(OCP PRIVATE ${MODULE_DIRS})
target_link_libraries(OCP PRIVATE
    ocp_ui ocp_renderer ocp_scene ocp_geometry ocp_core
    ocp_ai ocp_editor ocp_io ocp_platform ocp_plugin
    glad imgui glfw glm stb opengl32
)

set_target_properties(OCP PROPERTIES
    OUTPUT_NAME "OCP"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
