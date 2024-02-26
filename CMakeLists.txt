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
CPMAddPackage("gh:ArthurSonzogni/FTXUI#ce5ac6b12fccd4a451fceaf65ca95951ed4cf468")

CPMAddPackage("gh:chrysante/APMath#5770e9708d701c5a00483b68b1de8144b243ade0")
CPMAddPackage("gh:chrysante/graphgen#edc70a91294475fd11312eb35f7de1d2a55e2c47")
CPMAddPackage("gh:chrysante/termfmt#273498d74a90e9e9cbec41f24dc8b45e8827180b")
CPMAddPackage("gh:chrysante/utility#bd4bb0b0a0fa76f19febe9d8585e6a46e60051fc")

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
