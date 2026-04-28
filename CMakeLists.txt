
cmake_minimum_required(VERSION 3.20)
project(imgui_timeline_addon LANGUAGES CXX)

# Set build directory to build/ folder
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)

# Set ImGui directory at root level
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui)

# Add cmake modules path for ImGui helpers
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/demo/cmake)

option(BUILD_DEMO "Build demo application (GLFW/Metal/Vulkan selectable)" ON)
option(IMGUI_TIMELINE_PEDANTIC_WARNINGS "Enable extra warnings" ON)
option(IMGUI_TIMELINE_ENABLE_ASAN "Enable AddressSanitizer in Debug (non-MSVC)" OFF)

add_subdirectory(addon)
if(BUILD_DEMO)
  add_subdirectory(demo)
endif()
