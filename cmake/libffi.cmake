if(WIN32)
  CPMAddPackage(
    NAME libffi
    GIT_TAG master
    GITHUB_REPOSITORY chrysante/Libffi-windows
  )
else()
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/libffi-unix.cmake)
endif()
