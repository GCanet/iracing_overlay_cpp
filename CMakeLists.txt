cmake_minimum_required(VERSION 3.15)
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

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
    ${CMAKE_SOURCE_DIR}/external/glfw/include
    ${CMAKE_SOURCE_DIR}/external/glad/include
)

# ImGui sources
set(IMGUI_SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

# GLFW
add_subdirectory(external/glfw EXCLUDE_FROM_ALL)

# GLAD
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# Main application sources
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

# Create executable
add_executable(iRacingOverlay 
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
)

# Link libraries
target_link_libraries(iRacingOverlay
    glfw
    glad
    opengl32
)

# Copy assets to build directory
add_custom_command(TARGET iRacingOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
)

# Install target
install(TARGETS iRacingOverlay
    RUNTIME DESTINATION bin
)

install(DIRECTORY assets
    DESTINATION bin
)
