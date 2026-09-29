# Local development SDK: official headers + import libraries from installed OBS.
# No OBS implementation or CEF ABI is reconstructed here.
foreach(required ROADIE_OBS_SOURCE_DIR ROADIE_OBS_HOST_DIR ROADIE_OBS_IMPORT_DIR)
  if(NOT IS_DIRECTORY "${${required}}")
    message(FATAL_ERROR "${required} must name a prepared SDK directory")
  endif()
endforeach()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(OBS_RELEASE_CANDIDATE 0)
set(OBS_BETA 0)
configure_file("${ROADIE_OBS_SOURCE_DIR}/libobs/obsconfig.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/obsconfig.h")
add_library(OBS::libobs SHARED IMPORTED)
set_target_properties(OBS::libobs PROPERTIES
  IMPORTED_IMPLIB "${ROADIE_OBS_IMPORT_DIR}/obs.lib"
  IMPORTED_LOCATION "${ROADIE_OBS_HOST_DIR}/bin/64bit/obs.dll"
  INTERFACE_INCLUDE_DIRECTORIES "${ROADIE_OBS_SOURCE_DIR}/libobs;${CMAKE_CURRENT_BINARY_DIR}/generated")
add_library(OBS::obs-frontend-api SHARED IMPORTED)
set_target_properties(OBS::obs-frontend-api PROPERTIES
  IMPORTED_IMPLIB "${ROADIE_OBS_IMPORT_DIR}/obs-frontend-api.lib"
  IMPORTED_LOCATION "${ROADIE_OBS_HOST_DIR}/bin/64bit/obs-frontend-api.dll"
  INTERFACE_INCLUDE_DIRECTORIES "${ROADIE_OBS_SOURCE_DIR}/frontend/api"
  INTERFACE_LINK_LIBRARIES OBS::libobs)