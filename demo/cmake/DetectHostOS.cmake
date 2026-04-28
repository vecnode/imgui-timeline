# Detect host OS and distro details for backend selection.

set(IMGUI_TIMELINE_HOST_OS "UNKNOWN")
set(IMGUI_TIMELINE_IS_UBUNTU OFF)
set(IMGUI_TIMELINE_OS_VERSION "")

if(APPLE)
  set(IMGUI_TIMELINE_HOST_OS "MACOS")
  execute_process(
    COMMAND sw_vers -productVersion
    OUTPUT_VARIABLE _imgui_timeline_macos_ver
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(_imgui_timeline_macos_ver)
    set(IMGUI_TIMELINE_OS_VERSION "${_imgui_timeline_macos_ver}")
  endif()
elseif(WIN32)
  set(IMGUI_TIMELINE_HOST_OS "WINDOWS")
elseif(UNIX)
  set(IMGUI_TIMELINE_HOST_OS "LINUX")
  if(EXISTS "/etc/os-release")
    file(READ "/etc/os-release" _imgui_timeline_os_release)
    string(REGEX MATCH "(^|\n)ID=([^\n]+)" _id_match "${_imgui_timeline_os_release}")
    if(CMAKE_MATCH_2)
      string(REPLACE "\"" "" _distro_id "${CMAKE_MATCH_2}")
      string(TOLOWER "${_distro_id}" _distro_id)
      if(_distro_id STREQUAL "ubuntu")
        set(IMGUI_TIMELINE_IS_UBUNTU ON)
        set(IMGUI_TIMELINE_HOST_OS "UBUNTU")
      endif()
    endif()

    string(REGEX MATCH "(^|\n)VERSION_ID=([^\n]+)" _ver_match "${_imgui_timeline_os_release}")
    if(CMAKE_MATCH_2)
      string(REPLACE "\"" "" _ver_id "${CMAKE_MATCH_2}")
      set(IMGUI_TIMELINE_OS_VERSION "${_ver_id}")
    endif()
  endif()
endif()
