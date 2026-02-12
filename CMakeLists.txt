cmake_minimum_required(VERSION 3.20)

# =============================================================================
# FIX: Set MSVC runtime library policy BEFORE project() to avoid
# the CMake warning about conflicting CMAKE_MSVC_RUNTIME_LIBRARY
# =============================================================================
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(iRacingOverlay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set MSVC runtime library globally (static linking)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Platform specific settings
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)
    # NOTE: We do NOT set CMAKE_WIN32_EXECUTABLE ON here.
    # We want a console window for debug output.
endif()

# =============================================================================
# FetchContent - GLFW, ImGui, and stb (headers only)
# =============================================================================
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ImGui - use a stable release that actually exists
# v1.91.6 is the latest stable release as of early 2025
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp

    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)

# =============================================================================
# STB (header-only library)
# =============================================================================
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# Create interface library for stb headers
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# =============================================================================
# GLAD - Pre-generated static lib
# =============================================================================
add_library(glad STATIC src/glad.c)
target_include_directories(glad PUBLIC include)

# =============================================================================
# Main app
# =============================================================================
set(APP_SOURCES
    src/main.cpp
    src/ui/overlay_window.cpp
    src/ui/relative_widget.cpp
    src/ui/telemetry_widget.cpp
    src/data/irsdk_manager.cpp
    src/data/relative_calc.cpp
    src/data/irating_calc.cpp
    src/utils/config.cpp
    src/utils/yaml_parser.cpp
)

add_executable(iRacingOverlay ${APP_SOURCES})

# WIN32_EXECUTABLE FALSE = console subsystem (we get a console window)
set_target_properties(iRacingOverlay PROPERTIES
    WIN32_EXECUTABLE FALSE
)

target_include_directories(iRacingOverlay PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(iRacingOverlay PRIVATE
    glfw
    imgui
    glad
    stb
    opengl32
)

# Link dwmapi for DwmExtendFrameIntoClientArea
if(WIN32)
    target_link_libraries(iRacingOverlay PRIVATE dwmapi)
endif()

# Copy assets
if(EXISTS ${CMAKE_SOURCE_DIR}/assets)
    add_custom_command(TARGET iRacingOverlay POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
    )
endif()

# Install
install(TARGETS iRacingOverlay RUNTIME DESTINATION bin)
if(EXISTS ${CMAKE_SOURCE_DIR}/assets)
    install(DIRECTORY assets DESTINATION bin)
endif()
