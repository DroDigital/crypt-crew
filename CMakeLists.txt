cmake_minimum_required(VERSION 3.25)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(CryptCrew LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/Dependencies.cmake)

# ---- Version from git describe ----
include(cmake/Version.cmake)

# ---- Warnings helper ----
function(cc_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
endfunction()

# ---- Engine (game-agnostic systems) ----
file(GLOB_RECURSE CC_ENGINE_SOURCES CONFIGURE_DEPENDS src/engine/*.cpp src/engine/*.h)
add_library(cc_engine STATIC ${CC_ENGINE_SOURCES})
target_include_directories(cc_engine PUBLIC src)
target_include_directories(cc_engine SYSTEM PUBLIC ${CGLTF_INCLUDE_DIR})
target_link_libraries(cc_engine PUBLIC raylib Jolt nlohmann_json::nlohmann_json)
target_compile_definitions(cc_engine PUBLIC CC_VERSION="${CC_VERSION}" $<$<CONFIG:Debug>:CC_DEBUG=1>)
cc_warnings(cc_engine)

# ---- Tools ----
add_executable(asset_report tools/asset_report/main.cpp)
target_link_libraries(asset_report PRIVATE cc_engine)
cc_warnings(asset_report)

# ---- Game logic (library so tests can link it) ----
file(GLOB_RECURSE CC_GAME_SOURCES CONFIGURE_DEPENDS src/game/*.cpp src/game/*.h)
add_library(cc_game STATIC ${CC_GAME_SOURCES})
target_link_libraries(cc_game PUBLIC cc_engine)
cc_warnings(cc_game)

# ---- Game executable ----
add_executable(CryptCrew src/main.cpp)
target_link_libraries(CryptCrew PRIVATE cc_game)
cc_warnings(CryptCrew)

if(WIN32)
  string(REPLACE "." "," CC_VER_COMMA "${CC_VERSION_NUMERIC}")
  set(CC_ICON_PATH "${CMAKE_SOURCE_DIR}/packaging/windows/CryptCrew.ico")
  configure_file(packaging/windows/CryptCrew.rc.in ${CMAKE_BINARY_DIR}/CryptCrew.rc @ONLY)
  target_sources(CryptCrew PRIVATE ${CMAKE_BINARY_DIR}/CryptCrew.rc)
  if(MSVC)
    # GUI subsystem in release (no console window), keep the standard main() entry point.
    target_link_options(CryptCrew PRIVATE $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>)
  endif()
  set(CC_PLATFORM "windows-x64")
elseif(APPLE)
  set(CC_ICNS ${CMAKE_SOURCE_DIR}/packaging/macos/CryptCrew.icns)
  target_sources(CryptCrew PRIVATE ${CC_ICNS})
  set_source_files_properties(${CC_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(CryptCrew PROPERTIES
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "Crypt Crew"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.cryptcrew.game"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${CC_VERSION_NUMERIC}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${CC_VERSION_NUMERIC}")
  add_custom_command(TARGET CryptCrew POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets "$<TARGET_BUNDLE_CONTENT_DIR:CryptCrew>/Resources/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data "$<TARGET_BUNDLE_CONTENT_DIR:CryptCrew>/Resources/data"
    COMMENT "Copying assets and data into the app bundle")
  if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64" AND CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
    set(CC_PLATFORM "macos-universal")
  else()
    set(CC_PLATFORM "macos-${CMAKE_SYSTEM_PROCESSOR}")
  endif()
else()
  # Broad glibc/libstdc++ compatibility: link the C++ runtime statically.
  target_link_options(CryptCrew PRIVATE -static-libstdc++ -static-libgcc)
  set(CC_PLATFORM "linux-x64")
endif()

set(CC_BUNDLE_ARG "")
if(APPLE)
  set(CC_BUNDLE_ARG "-DBUNDLE=$<TARGET_BUNDLE_DIR:CryptCrew>")
endif()
add_custom_target(package_game
  COMMAND ${CMAKE_COMMAND} -DCC_PLATFORM=${CC_PLATFORM} -DCC_VERSION=${CC_VERSION}
          -DBIN=$<TARGET_FILE:CryptCrew> ${CC_BUNDLE_ARG}
          -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${CMAKE_BINARY_DIR}/dist -P ${CMAKE_SOURCE_DIR}/cmake/Package.cmake
  DEPENDS CryptCrew
  COMMENT "Packaging Crypt Crew ${CC_VERSION} for ${CC_PLATFORM}")

# ---- Unit tests (doctest) ----
enable_testing()
file(GLOB CC_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(cc_tests ${CC_TEST_SOURCES})
target_link_libraries(cc_tests PRIVATE cc_game doctest::doctest)
cc_warnings(cc_tests)
add_test(NAME unit COMMAND cc_tests)
