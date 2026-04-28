
# Independent addon library `imgui_timeline`; only needs ImGui headers present.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(imgui_timeline STATIC
  src/imgui_timeline.cpp
)

target_include_directories(imgui_timeline
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/imgui
)

# Link to a pre-existing `imgui` target if the host defines one.
if(TARGET imgui)
  target_link_libraries(imgui_timeline PUBLIC imgui)
endif()

if(IMGUI_TIMELINE_PEDANTIC_WARNINGS)
  if(MSVC)
    target_compile_options(imgui_timeline PRIVATE /W4)
  else()
    target_compile_options(imgui_timeline PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wpedantic)
  endif()
endif()

if(IMGUI_TIMELINE_ENABLE_ASAN AND NOT MSVC)
  target_compile_options(imgui_timeline PRIVATE -fsanitize=address)
  target_link_options(imgui_timeline PRIVATE -fsanitize=address)
endif()
