include(SourceFiles.cmake)

add_library(scatha SHARED)
set_target_properties(scatha PROPERTIES LINKER_LANGUAGE CXX)
SCSetCompilerOptions(scatha)

target_include_directories(scatha
    PUBLIC
      ${CMAKE_CURRENT_BINARY_DIR}/include
      include
    PRIVATE
      include/scatha
      lib
      ${PROJECT_SOURCE_DIR}/svm/include
)

target_link_libraries(scatha
  PUBLIC
    range-v3
    APMath
    utility
  PRIVATE
    nlohmann_json
    graphgen
    termfmt
)

target_sources(scatha
  PRIVATE
    ${scatha_headers}
    ${scatha_sources}
)
source_group(TREE scatha FILES ${scatha_headers} ${scatha_sources})

# Tests
add_executable(scatha-test)
set_target_properties(scatha-test PROPERTIES LINKER_LANGUAGE CXX)
SCSetCompilerOptions(scatha-test)

target_include_directories(scatha-test
    PRIVATE
      include/scatha
      lib
      test
)

target_link_libraries(scatha-test
  PRIVATE
    scatha
    libsvm

    range-v3
    utility
    termfmt

    Catch2::Catch2
)

add_dependencies(scatha-test ffi-testlib)

target_sources(scatha-test
  PRIVATE
    ${scatha_test_sources}
)
source_group(TREE scatha/test FILES ${scatha_test_sources})

# Library used to test foreign function import
add_library(ffi-testlib SHARED test/ffi-testlib/lib.cc)
# We create a copy of the library in a nested folder to test importing nested
# path names
add_custom_command(TARGET ffi-testlib POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:ffi-testlib>
    $<TARGET_FILE_DIR:ffi-testlib>/nested/$<TARGET_FILE_NAME:ffi-testlib>
)
