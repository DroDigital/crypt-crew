# Stages and archives a release build. Run with cmake -P (see the package_game target).
# Inputs: CC_PLATFORM, CC_VERSION, SRC, OUT, BIN (executable), BUNDLE (macOS .app)
if(NOT CC_PLATFORM OR NOT CC_VERSION OR NOT SRC OR NOT OUT)
  message(FATAL_ERROR "Package.cmake: missing inputs")
endif()
set(NAME "CryptCrew-${CC_VERSION}-${CC_PLATFORM}")
set(STAGE "${OUT}/${NAME}")
file(REMOVE_RECURSE "${STAGE}")
file(MAKE_DIRECTORY "${STAGE}")

if(CC_PLATFORM MATCHES "^macos")
  # The .app already carries assets/ and data/ in Contents/Resources (copied at build time).
  get_filename_component(APPNAME "${BUNDLE}" NAME)
  file(COPY "${BUNDLE}" DESTINATION "${STAGE}" USE_SOURCE_PERMISSIONS)
else()
  file(COPY "${BIN}" DESTINATION "${STAGE}" USE_SOURCE_PERMISSIONS)
  file(COPY "${SRC}/assets" "${SRC}/data" DESTINATION "${STAGE}")
endif()
file(COPY "${SRC}/CREDITS.md" DESTINATION "${STAGE}")
configure_file("${SRC}/packaging/README.txt" "${STAGE}/README.txt" COPYONLY)
if(CC_PLATFORM MATCHES "^linux")
  file(COPY "${SRC}/packaging/linux/crypt-crew.desktop" "${SRC}/packaging/linux/crypt-crew.png" DESTINATION "${STAGE}")
endif()

# Archive (macOS zips in CI with ditto after signing, to keep the bundle intact).
if(CC_PLATFORM MATCHES "^windows")
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar cf "${OUT}/${NAME}.zip" --format=zip "${NAME}"
                  WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE r)
elseif(CC_PLATFORM MATCHES "^linux")
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar czf "${OUT}/${NAME}.tar.gz" "${NAME}"
                  WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE r)
else()
  set(r 0)
endif()
if(NOT r EQUAL 0)
  message(FATAL_ERROR "archiving failed: ${r}")
endif()
message(STATUS "Packaged ${STAGE}")
