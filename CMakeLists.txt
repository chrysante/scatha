cmake_minimum_required(VERSION 3.23)

project(
  scatha
  VERSION 0.0.1
  DESCRIPTION "Compiler library"
)

include(cmake/BuildOptions.cmake)
include(cmake/CPM.cmake)
include(cmake/CompilerOptions.cmake)

set(CMAKE_CXX_STANDARD 20)

# To make range-v3 happy
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Dependencies
include(cmake/libffi.cmake)
CPMAddPackage("gh:ericniebler/range-v3#0.12.0")
CPMAddPackage("gh:nlohmann/json@3.11.3")
CPMAddPackage("gh:chrysante/microtar#master")
CPMAddPackage("gh:chrysante/APMath#main")
CPMAddPackage("gh:chrysante/graphgen#main")
set(termfmt_SHARED ON)
CPMAddPackage("gh:chrysante/termfmt#main")
CPMAddPackage("gh:chrysante/utility#main")
if(SCATHA_BUILD_EXECUTABLES)
  CPMAddPackage("gh:CLIUtils/CLI11@2.4.1")
  set(FTXUI_BUILD_EXAMPLES OFF)
  set(FTXUI_ENABLE_INSTALL OFF)
  set(FTXUI_QUIET          ON)
  CPMAddPackage("gh:ArthurSonzogni/FTXUI#main")
endif()
if(SCATHA_BUILD_TESTS)
  CPMAddPackage("gh:catchorg/Catch2@3.4.0")
endif()

# Subprojects
add_subdirectory(scatha)
add_subdirectory(scathac)
add_subdirectory(svm)
add_subdirectory(scathadb)

if(SCATHA_BUILD_EXAMPLES)
  add_subdirectory(playground)
  install(
    TARGETS
      scatha
      scathac
      scathadb
      svm
      libsvm
      termfmt
    DESTINATION
      bin
  )
endif()
