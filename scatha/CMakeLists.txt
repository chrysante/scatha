include(SourceFiles.cmake)

# scatha
add_library(scatha SHARED)
set_target_properties(scatha PROPERTIES LINKER_LANGUAGE CXX)
SCSetCompilerOptions(scatha)
target_compile_definitions(scatha
  PRIVATE SC_APIEXPORT
  INTERFACE SC_APIIMPORT)

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
    utility
  PRIVATE
    nlohmann_json
    ctre
    microtar
    APMath
    graphgen
    termfmt
)

target_sources(scatha
  PRIVATE
    ${scatha_headers}
    ${scatha_sources}
)
source_group(TREE ${PROJECT_SOURCE_DIR}/scatha FILES ${scatha_headers} ${scatha_sources})

if(NOT SCATHA_BUILD_TESTS)
  return()
endif()

# scatha-test
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
    APMath
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
source_group(TREE ${PROJECT_SOURCE_DIR}/scatha/test FILES ${scatha_test_sources})

# Library used to test foreign function import
add_library(ffi-testlib SHARED test/ffi-testlib/lib.cc)
# We create a copy of the library in a nested folder to test importing nested
# path names
add_custom_command(TARGET ffi-testlib POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:ffi-testlib>
    $<TARGET_FILE_DIR:ffi-testlib>/nested/$<TARGET_FILE_NAME:ffi-testlib>
)

# scatha-benchmark
add_executable(scatha-benchmark)
set_target_properties(scatha-benchmark PROPERTIES LINKER_LANGUAGE CXX)
SCSetCompilerOptions(scatha-benchmark)

target_include_directories(scatha-benchmark
    PRIVATE
      include
      benchmark
)

target_link_libraries(scatha-benchmark
  PRIVATE
    scatha
    libsvm
    Catch2::Catch2WithMain
)

target_sources(scatha-benchmark
  PRIVATE
    ${scatha_benchmark_sources}
)
source_group(TREE ${PROJECT_SOURCE_DIR}/scatha/benchmark FILES ${scatha_benchmark_sources})
