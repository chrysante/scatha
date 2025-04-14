# libscdis

add_library(libscdis STATIC)
set_target_properties(libscdis PROPERTIES LINKER_LANGUAGE CXX)
SCSetCompilerOptions(libscdis)
set_target_properties(libscdis PROPERTIES OUTPUT_NAME "scdis")

target_link_libraries(libscdis
  PUBLIC
    scatha-debuginfo
    utility
    scbinutil
  PRIVATE
    termfmt
    range-v3
)

target_include_directories(libscdis
    PUBLIC
      include
    PRIVATE
      src
)

set(libscdis_headers
    include/scdis/Disassembler.h
    include/scdis/Disassembly.h
    include/scdis/Print.h
)

set(libscdis_sources
    src/scdis/Disassembler.cc
    src/scdis/Disassembly.cc
    src/scdis/Print.cc
)

target_sources(libscdis
  PRIVATE
    ${libscdis_headers}
    ${libscdis_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/include/scdis FILES ${libscdis_headers})
source_group(TREE ${PROJECT_SOURCE_DIR}/src/scdis FILES ${libscdis_sources})

# scdis

if(SCATHA_BUILD_EXECUTABLES)

add_executable(scdis)
set_target_properties(scdis PROPERTIES INSTALL_RPATH "@loader_path")
SCSetCompilerOptions(scdis)

target_link_libraries(scdis
  PRIVATE
    libscdis
    CLI11::CLI11
)

target_include_directories(scdis
  PRIVATE
    src
)

set(scdis_sources
    src/scdis/Main.cc
)

target_sources(scdis
  PRIVATE
    ${scdis_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/src/scdis FILES ${scdis_sources})

endif() # SCATHA_BUILD_EXECUTABLES
