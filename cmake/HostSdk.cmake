# Local development SDK: official headers + import libraries from installed OBS.
# No OBS implementation or CEF ABI is reconstructed here.
# On macOS the "host" is the installed OBS.app: link against its own libobs
# framework, obs-frontend-api and Qt, with headers from the matching OBS tag.
if(APPLE)
  set(ROADIE_OBS_APP_DIR "/Applications/OBS.app" CACHE PATH "Installed OBS.app to link against")
  foreach(required ROADIE_OBS_SOURCE_DIR ROADIE_OBS_APP_DIR)
    if(NOT IS_DIRECTORY "${${required}}")
      message(FATAL_ERROR "${required} must name a prepared SDK directory")
    endif()
  endforeach()
  set(_obs_fw "${ROADIE_OBS_APP_DIR}/Contents/Frameworks")
  set(_libobs_bin "${_obs_fw}/libobs.framework/libobs")
  set(_frontend_bin "${_obs_fw}/obs-frontend-api.dylib")
  foreach(bin _libobs_bin _frontend_bin)
    if(NOT EXISTS "${${bin}}")
      message(FATAL_ERROR "Expected ${${bin}} inside the installed OBS.app")
    endif()
  endforeach()
else()
  foreach(required ROADIE_OBS_SOURCE_DIR ROADIE_OBS_HOST_DIR ROADIE_OBS_IMPORT_DIR)
    if(NOT IS_DIRECTORY "${${required}}")
      message(FATAL_ERROR "${required} must name a prepared SDK directory")
    endif()
  endforeach()
endif()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(OBS_RELEASE_CANDIDATE 0)
set(OBS_BETA 0)
configure_file("${ROADIE_OBS_SOURCE_DIR}/libobs/obsconfig.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/obsconfig.h")
set(frontend_headers "${ROADIE_OBS_SOURCE_DIR}/frontend/api")
if(NOT EXISTS "${frontend_headers}/obs-frontend-api.h")
  set(frontend_headers "${ROADIE_OBS_SOURCE_DIR}/UI/obs-frontend-api")
endif()
# OBS 31+ no longer carries SIMDe in-tree, yet libobs' public headers include it.
set(ROADIE_SIMDE_INCLUDE_DIR "" CACHE PATH "SIMDe headers (directory containing simde/) for OBS 31+ sources")
set(libobs_includes "${ROADIE_OBS_SOURCE_DIR}/libobs;${CMAKE_CURRENT_BINARY_DIR}/generated")
if(NOT IS_DIRECTORY "${ROADIE_OBS_SOURCE_DIR}/libobs/util/simde")
  if(NOT EXISTS "${ROADIE_SIMDE_INCLUDE_DIR}/simde/x86/sse2.h")
    message(FATAL_ERROR "This OBS source tree needs ROADIE_SIMDE_INCLUDE_DIR pointing at SIMDe headers")
  endif()
  list(APPEND libobs_includes "${ROADIE_SIMDE_INCLUDE_DIR}")
endif()
add_library(OBS::libobs SHARED IMPORTED)
add_library(OBS::obs-frontend-api SHARED IMPORTED)
if(APPLE)
  set_target_properties(OBS::libobs PROPERTIES
    IMPORTED_LOCATION "${_libobs_bin}"
    INTERFACE_INCLUDE_DIRECTORIES "${libobs_includes}")
  set_target_properties(OBS::obs-frontend-api PROPERTIES
    IMPORTED_LOCATION "${_frontend_bin}"
    INTERFACE_INCLUDE_DIRECTORIES "${frontend_headers}"
    INTERFACE_LINK_LIBRARIES OBS::libobs)
else()
  set_target_properties(OBS::libobs PROPERTIES
    IMPORTED_IMPLIB "${ROADIE_OBS_IMPORT_DIR}/obs.lib"
    IMPORTED_LOCATION "${ROADIE_OBS_HOST_DIR}/bin/64bit/obs.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${libobs_includes}")
  set_target_properties(OBS::obs-frontend-api PROPERTIES
    IMPORTED_IMPLIB "${ROADIE_OBS_IMPORT_DIR}/obs-frontend-api.lib"
    IMPORTED_LOCATION "${ROADIE_OBS_HOST_DIR}/bin/64bit/obs-frontend-api.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${frontend_headers}"
    INTERFACE_LINK_LIBRARIES OBS::libobs)
endif()
