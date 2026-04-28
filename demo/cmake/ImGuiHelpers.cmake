
function(check_imgui_commit)
  set(options)
  set(oneValueArgs IMGUI_DIR EXPECTED)
  cmake_parse_arguments(CIC "${options}" "${oneValueArgs}" "" ${ARGN})
  if (NOT EXISTS "${CIC_IMGUI_DIR}/.git")
    message(WARNING "ImGui at ${CIC_IMGUI_DIR} is not a git checkout. Skipping commit check.")
    return()
  endif()
  find_package(Git QUIET)
  if (NOT Git_FOUND)
    message(WARNING "Git not found; cannot verify imgui commit.")
    return()
  endif()
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${CIC_IMGUI_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE IMGUI_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if (IMGUI_COMMIT)
    message(STATUS "Dear ImGui commit: ${IMGUI_COMMIT}")
    if (CIC_EXPECTED AND NOT IMGUI_COMMIT STREQUAL CIC_EXPECTED)
      message(FATAL_ERROR "Dear ImGui submodule at ${CIC_IMGUI_DIR} is ${IMGUI_COMMIT} but ${CIC_EXPECTED} was required. Checkout the expected commit.")
    endif()
  endif()
endfunction()
