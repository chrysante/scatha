
add_library(scathadb-core STATIC)
SCSetCompilerOptions(scathadb-core)

target_link_libraries(scathadb-core
  PUBLIC
    libscdis
    range-v3
    utility

  PRIVATE
    scatha-debuginfo
    libsvm
    magic_enum
)

target_include_directories(scathadb-core
  PUBLIC
    include
  PRIVATE
    include/scathadb
    src/scathadb
)

set(scathadb_core_headers
    include/scathadb/Model/BreakpointManager.h
    include/scathadb/Model/BreakpointPatcher.h
    include/scathadb/Model/Events.h
    include/scathadb/Model/Executor.h
    include/scathadb/Model/Model.h
    include/scathadb/Model/Options.h
    include/scathadb/Model/Stdout.h
    include/scathadb/Model/SourceDebugInfo.h
    include/scathadb/Model/SourceFile.h

    include/scathadb/Util/Messenger.h
)

set(scathadb_core_sources
    src/scathadb/Model/BreakpointManager.cc
    src/scathadb/Model/BreakpointPatcher.cc
    src/scathadb/Model/Events.cc
    src/scathadb/Model/Executor.cc
    src/scathadb/Model/Model.cc
    src/scathadb/Model/Options.cc
    src/scathadb/Model/Stdout.cc
    src/scathadb/Model/SourceDebugInfo.cc
    src/scathadb/Model/SourceFile.cc

    src/scathadb/Util/Messenger.cc
)

target_sources(scathadb-core
  PRIVATE
    ${scathadb_core_headers}
    ${scathadb_core_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/include/scathadb FILES ${scathadb_core_headers})
source_group(TREE ${PROJECT_SOURCE_DIR}/src/scathadb FILES ${scathadb_core_sources})

if(SCATHA_BUILD_EXECUTABLES)

add_executable(scathadb)
set_target_properties(scathadb PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(scathadb PROPERTIES INSTALL_RPATH "@loader_path")
SCSetCompilerOptions(scathadb)

target_link_libraries(scathadb
  PRIVATE
    scathadb-core
    range-v3
    utility
    CLI11::CLI11
    ftxui::screen
    ftxui::dom
    ftxui::component
)

target_include_directories(scathadb
  PRIVATE
    include/scathadb
    src/scathadb
)

set(scathadb_sources
    src/scathadb/App/Command.cc
    src/scathadb/App/Command.h
    src/scathadb/App/Debugger.cc
    src/scathadb/App/Debugger.h
    src/scathadb/App/Main.cc

    src/scathadb/UI/Common.cc
    src/scathadb/UI/Common.h
    src/scathadb/UI/ModalView.cc
    src/scathadb/UI/ModalView.h

    src/scathadb/Views/ConsoleView.cc
    src/scathadb/Views/DisassemblyView.cc
    src/scathadb/Views/FileViewCommon.cc
    src/scathadb/Views/FileViewCommon.h
    src/scathadb/Views/HelpPanel.cc
    src/scathadb/Views/HelpPanel.h
    src/scathadb/Views/OpenFilePanel.cc
    src/scathadb/Views/Confirm.cc
    src/scathadb/Views/Settings.cc
    src/scathadb/Views/SourceFileBrowser.cc
    src/scathadb/Views/SourceView.cc
    src/scathadb/Views/Toolbar.cc
    src/scathadb/Views/VMStateView.cc
    src/scathadb/Views/Views.h
)

target_sources(scathadb
  PRIVATE
    ${scathadb_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/src/scathadb FILES ${scathadb_sources})

endif() # SCATHA_BUILD_EXECUTABLES

if(SCATHA_BUILD_TESTS)

add_executable(scathadb-test)
set_target_properties(scathadb-test PROPERTIES LINKER_LANGUAGE CXX)
SCSetCompilerOptions(scathadb-test)

target_include_directories(scathadb-test 
  PRIVATE 
    src/scathadb
)

target_link_libraries(scathadb-test 
  PRIVATE
    scatha
    scathadb-core
    Catch2::Catch2WithMain
)

set(scathadb_test_sources
    test/scathadb/Model/Model.t.cc
)

target_sources(scathadb-test 
  PRIVATE 
    ${scathadb_test_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/test/scathadb FILES ${scathadb_test_sources})

endif() # SCATHA_BUILD_TESTS
