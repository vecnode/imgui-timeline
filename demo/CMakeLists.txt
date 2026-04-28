
cmake_minimum_required(VERSION 3.20)

set(IMGUI_TIMELINE_DEMO_LANGUAGES C CXX)
if(APPLE)
  list(APPEND IMGUI_TIMELINE_DEMO_LANGUAGES OBJC OBJCXX)
endif()
project(imgui_timeline_demo_app LANGUAGES ${IMGUI_TIMELINE_DEMO_LANGUAGES})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(DetectHostOS)
message(STATUS "Host OS detected: ${IMGUI_TIMELINE_HOST_OS} ${IMGUI_TIMELINE_OS_VERSION}")

# Backend selection (interactive via scripts; here we just honor the choice)
set(USE_BACKEND "AUTO" CACHE STRING "Renderer backend: AUTO, OPENGL, METAL (macOS), VULKAN")
set_property(CACHE USE_BACKEND PROPERTY STRINGS AUTO OPENGL METAL VULKAN)

# Recommend by platform if AUTO
if (USE_BACKEND STREQUAL "AUTO")
  if (IMGUI_TIMELINE_HOST_OS STREQUAL "MACOS")
    set(USE_BACKEND "METAL")
    message(STATUS "AUTO -> macOS detected: choosing METAL (recommended).")
  elseif (IMGUI_TIMELINE_HOST_OS STREQUAL "UBUNTU")
    set(USE_BACKEND "VULKAN")
    message(STATUS "AUTO -> Ubuntu detected: choosing VULKAN (recommended).")
  elseif (WIN32)
    set(USE_BACKEND "OPENGL")
    message(STATUS "AUTO -> Windows detected: choosing OPENGL (default).")
  else()
    set(USE_BACKEND "OPENGL")
    message(STATUS "AUTO -> Linux detected: choosing OPENGL (default).")
  endif()
else()
  message(STATUS "Backend explicitly set to ${USE_BACKEND}")
endif()

option(BUILD_VULKAN "Build Vulkan demo target (requires Vulkan SDK / MoltenVK on macOS)" OFF)
set(IMGUI_ENFORCE_COMMIT "" CACHE STRING "Optional SHA to enforce for libs/imgui")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Bring ImGui (demo-only). If not a submodule, we still build if headers exist.
# IMGUI_DIR is now set at root level
if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
  message(WARNING "Dear ImGui not found at ${IMGUI_DIR}. If using git, run 'git submodule update --init --recursive' or run build_app.sh to auto-fetch.")
  file(MAKE_DIRECTORY ${IMGUI_DIR}) # allow manual drop-in
endif()
include(ImGuiHelpers)
check_imgui_commit(IMGUI_DIR ${IMGUI_DIR} EXPECTED ${IMGUI_ENFORCE_COMMIT})

# Common ImGui core
set(IMGUI_CORE
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
)
add_library(imgui_core STATIC ${IMGUI_CORE})
target_include_directories(imgui_core PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# Choose backend
if (USE_BACKEND STREQUAL "OPENGL")
  include(FetchContent)
  FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)
  FetchContent_MakeAvailable(glfw)
  FetchContent_Declare(glad GIT_REPOSITORY https://github.com/Dav1dde/glad.git GIT_TAG v0.1.36)
  FetchContent_MakeAvailable(glad)

  add_library(imgui_backend STATIC
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_link_libraries(imgui_backend PUBLIC imgui_core)
  target_compile_definitions(imgui_backend PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)

  add_dependencies(imgui_timeline imgui_backend)

  add_executable(imgui_timeline_demo src/main.cpp)
  target_include_directories(imgui_timeline_demo PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
  if(APPLE)
    target_link_libraries(imgui_timeline_demo PRIVATE imgui_timeline imgui_backend glfw glad "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
  elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(imgui_timeline_demo PRIVATE imgui_timeline imgui_backend glfw glad ${X11_LIBRARIES} dl pthread GL)
  elseif(WIN32)
    target_link_libraries(imgui_timeline_demo PRIVATE imgui_timeline imgui_backend glfw glad opengl32)
  endif()
  message(STATUS "Configured OpenGL demo target: imgui_timeline_demo")

elseif (APPLE AND USE_BACKEND STREQUAL "METAL")
  find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
  find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
  find_library(IOKIT_FRAMEWORK IOKit REQUIRED)

  add_library(imgui_backend STATIC
    ${IMGUI_DIR}/backends/imgui_impl_osx.mm
    ${IMGUI_DIR}/backends/imgui_impl_metal.mm
  )
  set_source_files_properties(${IMGUI_DIR}/backends/imgui_impl_osx.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc -Wno-deprecated-declarations")
  set_source_files_properties(${IMGUI_DIR}/backends/imgui_impl_metal.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  target_link_libraries(imgui_backend PUBLIC imgui_core)

  add_dependencies(imgui_timeline imgui_backend)

  add_executable(imgui_timeline_metal src/main_metal.mm)
  target_include_directories(imgui_timeline_metal PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
  target_compile_definitions(imgui_timeline_metal PRIVATE IMGUI_TIMELINE_METAL_COMPATIBILITY)
  target_link_libraries(imgui_timeline_metal PRIVATE imgui_timeline imgui_backend
    ${COCOA_FRAMEWORK} ${QUARTZCORE_FRAMEWORK} ${METAL_FRAMEWORK} ${METALKIT_FRAMEWORK} ${IOKIT_FRAMEWORK} "-framework GameController")
  set_target_properties(imgui_timeline_metal PROPERTIES MACOSX_BUNDLE FALSE)
  message(STATUS "Configured Metal demo target: imgui_timeline_metal")

elseif (USE_BACKEND STREQUAL "VULKAN")
  find_package(Vulkan REQUIRED)
  include(FetchContent)
  FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)
  FetchContent_MakeAvailable(glfw)

  add_library(imgui_backend STATIC
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
  )
  target_link_libraries(imgui_backend PUBLIC imgui_core)
  add_dependencies(imgui_timeline imgui_backend)

  add_executable(imgui_timeline_vulkan src/main.cpp)
  target_compile_definitions(imgui_timeline_vulkan PRIVATE USE_VULKAN_BACKEND)
  target_include_directories(imgui_timeline_vulkan PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
  target_link_libraries(imgui_timeline_vulkan PRIVATE imgui_timeline imgui_backend glfw Vulkan::Vulkan)
  message(STATUS "Vulkan target: imgui_timeline_vulkan (fully implemented).")

else()
  message(FATAL_ERROR "Unsupported backend selection: ${USE_BACKEND}.")
endif()
