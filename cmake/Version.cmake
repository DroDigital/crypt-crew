# Game version from `git describe --tags --always --dirty`, e.g. v0.1.0 or v0.1.0-3-gabc1234.
find_package(Git QUIET)
set(CC_VERSION "0.0.0-dev")
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE _desc OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET RESULT_VARIABLE _res)
  if(_res EQUAL 0 AND _desc)
    set(CC_VERSION "${_desc}")
  endif()
endif()
string(REGEX REPLACE "^v" "" CC_VERSION "${CC_VERSION}")
# Numeric x.y.z for bundle metadata (Info.plist, Windows resource).
set(CC_VERSION_NUMERIC "0.0.0")
if(CC_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
  set(CC_VERSION_NUMERIC "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
endif()
message(STATUS "Crypt Crew version: ${CC_VERSION} (${CC_VERSION_NUMERIC})")
