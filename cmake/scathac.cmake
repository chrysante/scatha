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

set(scathac_sources 
    src/scathac/Compiler.cc
    src/scathac/Compiler.h
    src/scathac/Graph.cc
    src/scathac/Graph.h
    src/scathac/Inspect.cc
    src/scathac/Inspect.h
    src/scathac/Options.cc
    src/scathac/Options.h
    src/scathac/Util.cc
    src/scathac/Util.h
    src/scathac/main.cc
)

target_sources(scathac
  PRIVATE ${scathac_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/src/scathac FILES ${scathac_sources})
