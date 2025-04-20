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
CPMAddPackage("gh:hanickadot/compile-time-regular-expressions@3.9.0")
CPMAddPackage("gh:chrysante/microtar#7585831c32a0d7d92850bee95246a640b2b60244")
CPMAddPackage("gh:chrysante/APMath#aa8feab74ac2cf1d2f1c390cdb51493da7492d5b")
CPMAddPackage("gh:chrysante/graphgen#3ca2abe5f8a44238d6eecb95270cb54121c7cd06")
set(termfmt_SHARED OFF)
CPMAddPackage("gh:chrysante/termfmt#0.0.1")
CPMAddPackage("gh:chrysante/csp@0.0.2")
CPMAddPackage("gh:chrysante/utility@0.1.0")
if(SCATHA_BUILD_EXECUTABLES)
  CPMAddPackage("gh:CLIUtils/CLI11@2.4.1")
  set(FTXUI_BUILD_EXAMPLES OFF)
  set(FTXUI_ENABLE_INSTALL OFF)
  set(FTXUI_QUIET          ON)
  CPMAddPackage("gh:ArthurSonzogni/FTXUI#e8589dd533fec9835831d818564524499decc4fa")
endif()
if(SCATHA_BUILD_TESTS)
  CPMAddPackage("gh:catchorg/Catch2@3.4.0")
  CPMAddPackage("gh:Neargye/magic_enum@0.9.7")
endif()

# Subprojects
include(cmake/scatha-debuginfo.cmake)
include(cmake/scatha.cmake)
include(cmake/scathac.cmake)
include(cmake/scbinutil.cmake)
include(cmake/svm.cmake)
include(cmake/scdis.cmake)
include(cmake/scathadb.cmake)
include(cmake/stdlib.cmake)

if(SCATHA_BUILD_EXAMPLES)
  set(INSTALL_DEST lib/scatha)
  install(
    TARGETS
      scatha
      scathac
      scdis
      scathadb
      svm
    DESTINATION
      ${INSTALL_DEST}/bin
  )
  install(
    FILES
      ${COMPILED_STDLIB}
    DESTINATION
      ${INSTALL_DEST}/stdlib
  )
endif()
