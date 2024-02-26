cmake_minimum_required(VERSION 3.23)

file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

set(CMAKE_CXX_STANDARD 20)

project(
  scatha
  VERSION 0.0.1
  DESCRIPTION "Compiler library")

include(cmake/libffi.cmake)

CPMAddPackage("gh:ericniebler/range-v3#0.12.0")
CPMAddPackage("gh:nlohmann/json@3.11.3")
CPMAddPackage("gh:CLIUtils/CLI11@2.4.1")
CPMAddPackage("gh:catchorg/Catch2@3.4.0")
set(FTXUI_BUILD_EXAMPLES OFF)
set(FTXUI_ENABLE_INSTALL OFF)
set(FTXUI_QUIET          ON)
CPMAddPackage("gh:ArthurSonzogni/FTXUI#main")

CPMAddPackage("gh:chrysante/APMath#main")
CPMAddPackage("gh:chrysante/graphgen#main")
CPMAddPackage("gh:chrysante/termfmt#main")
CPMAddPackage("gh:chrysante/utility#main")

add_subdirectory(scatha)
add_subdirectory(scathac)
add_subdirectory(svm)
add_subdirectory(scathadb)
add_subdirectory(ffi-testlib)

install(
  TARGETS
    scatha
    scathac
    scathadb
    svm
  DESTINATION
    bin
)
