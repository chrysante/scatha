add_executable(playground)
set_target_properties(playground PROPERTIES LINKER_LANGUAGE CXX)
SCSetCompilerOptions(playground)

target_include_directories(playground
    PRIVATE
      ${PROJECT_SOURCE_DIR}/scatha/include/scatha
      ${PROJECT_SOURCE_DIR}/scatha/lib
      ${PROJECT_SOURCE_DIR}/svm/include
)

target_link_libraries(playground
  PRIVATE
    scatha
    APMath
    utility
    range-v3
)

target_sources(playground
  PRIVATE
    src/Main.cc
    src/Volatile.h
    src/Volatile.cc
)

source_group(src REGULAR_EXPRESSION "src/*")
