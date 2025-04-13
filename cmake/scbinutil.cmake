add_library(scbinutil STATIC)
SCSetCompilerOptions(scbinutil)

target_include_directories(scbinutil
  PUBLIC
    include
  PRIVATE
    src
)

target_link_libraries(scbinutil
  PUBLIC 
    utility
)

set(scbinutil_headers
    include/scbinutil/ProgramView.h
    include/scbinutil/OpCode.def.h
    include/scbinutil/OpCode.h
)

set(scbinutil_sources
    src/scbinutil/ProgramView.cc
    src/scbinutil/OpCode.cc
)

target_sources(scbinutil
  PRIVATE
    ${scbinutil_headers}
    ${scbinutil_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/include/scbinutil FILES ${scbinutil_headers})
source_group(TREE ${PROJECT_SOURCE_DIR}/src/scbinutil FILES ${scbinutil_sources})
