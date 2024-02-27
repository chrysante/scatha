if(NOT SCATHA_BUILD_EXECUTABLES)
  return()
endif()

add_executable(scathac)
set_target_properties(scathac PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(scathac PROPERTIES INSTALL_RPATH "@loader_path")
SCSetCompilerOptions(scathac)

target_link_libraries(scathac
  PRIVATE
    scatha
    CLI11::CLI11
    termfmt
)

target_sources(scathac
  PRIVATE
    src/Compiler.cc
    src/Compiler.h
    src/Graph.cc
    src/Graph.h
    src/Inspect.cc
    src/Inspect.h
    src/Options.cc
    src/Options.h
    src/Util.cc
    src/Util.h
    src/main.cc
)

source_group(src REGULAR_EXPRESSION "src/*")
