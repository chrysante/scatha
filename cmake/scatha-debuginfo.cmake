add_library(scatha-debuginfo STATIC)
SCSetCompilerOptions(scatha-debuginfo)

target_link_libraries(scatha-debuginfo
  PUBLIC
    utility
    nlohmann_json
    range-v3
)

target_include_directories(scatha-debuginfo
  PUBLIC
    include
  PRIVATE
    include/scatha
    src/scatha
)

set(sdbi_headers
    include/scatha/DebugInfo/DebugInfo.h
)

set(sdbi_sources
    src/scatha/DebugInfo/DebugInfo.cc
)

target_sources(scatha-debuginfo
  PRIVATE
    ${sdbi_headers}
    ${sdbi_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/include/scatha FILES ${sdbi_headers})
source_group(TREE ${PROJECT_SOURCE_DIR}/src/scatha FILES ${sdbi_sources})
