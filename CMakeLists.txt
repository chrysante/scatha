cmake_minimum_required(VERSION 3.23)
set(CMAKE_CXX_STANDARD 20)

option(SCATHA_BUILD_EXECUTABLES "Enable to build the compiler, interpreter and debugger executables. Disable to only use as a library" ON)
option(SCATHA_ENABLE_INSTALL "Generate the install target" ON)
option(SCATHA_BUILD_EXAMPLES "Enable to build examples" PROJECT_IS_TOP_LEVEL)
option(SCATHA_BUILD_TESTS "Enable to build tests" PROJECT_IS_TOP_LEVEL)
option(SCATHA_DEV_MODE "Enable sanitizers and more compiler warnings" PROJECT_IS_TOP_LEVEL)

include(cmake/CPM.cmake)
include(cmake/CompilerOptions.cmake)

project(
  scatha
  VERSION 0.0.1
  DESCRIPTION "Compiler library"
)

# To make range-v3 happy
set(CMAKE_BUILD_TYPE Release)

# Dependencies
include(cmake/libffi.cmake)
CPMAddPackage("gh:ericniebler/range-v3#0.12.0")
CPMAddPackage("gh:nlohmann/json@3.11.3")
CPMAddPackage("gh:chrysante/APMath#main")
CPMAddPackage("gh:chrysante/graphgen#main")
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
    DESTINATION
      bin
  )
endif()
