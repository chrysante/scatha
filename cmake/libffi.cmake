if(WIN32)
  #CPMAddPackage("gh:chrysante/Libffi-windows#master")
  CPMAddPackage(
    NAME libffi
    GIT_TAG master
    GITHUB_REPOSITORY chrysante/Libffi-windows
  )
else()
  include(libffi-unix.cmake)
endif()