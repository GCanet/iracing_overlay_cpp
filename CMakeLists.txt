cmake_minimum_required(VERSION 3.20)
project(iRacingOverlay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()

# =============================================================================
# FetchContent - GLFW & ImGui (no external/ folders)
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

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.5
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

set_target_properties(iRacingOverlay PROPERTIES
    WIN32_EXECUTABLE FALSE
)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(iRacingOverlay PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(iRacingOverlay PRIVATE
    glfw
    imgui
    glad
    opengl32
)

# Copy assets
add_custom_command(TARGET iRacingOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
)

# Install
install(TARGETS iRacingOverlay RUNTIME DESTINATION bin)
install(DIRECTORY assets DESTINATION bin)
