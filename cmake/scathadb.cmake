if(NOT SCATHA_BUILD_EXECUTABLES)
  return()
endif()

add_executable(scathadb)
set_target_properties(scathadb PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(scathadb PROPERTIES INSTALL_RPATH "@loader_path")
SCSetCompilerOptions(scathadb)

target_link_libraries(scathadb
  PRIVATE
    scatha-debuginfo
    libsvm
    libscdis
    range-v3
    utility
    csp
    nlohmann_json
    magic_enum
    CLI11::CLI11
    ftxui::screen
    ftxui::dom
    ftxui::component
)

target_include_directories(scathadb
  PRIVATE
    src/scathadb
)

set(scathadb_sources
    src/scathadb/App/Command.cc
    src/scathadb/App/Command.h
    src/scathadb/App/Debugger.cc
    src/scathadb/App/Debugger.h
    src/scathadb/App/Main.cc
    src/scathadb/App/Messenger.cc
    src/scathadb/App/Messenger.h

    src/scathadb/Model/BreakpointManager.cc
    src/scathadb/Model/BreakpointManager.h
    src/scathadb/Model/Events.h
    src/scathadb/Model/Events.cc
    src/scathadb/Model/Executor.cc
    src/scathadb/Model/Executor.h
    src/scathadb/Model/Model.cc
    src/scathadb/Model/Model.h
    src/scathadb/Model/Options.cc
    src/scathadb/Model/Options.h
    src/scathadb/Model/Stdout.cc
    src/scathadb/Model/Stdout.h
    src/scathadb/Model/SourceDebugInfo.cc
    src/scathadb/Model/SourceDebugInfo.h
    src/scathadb/Model/SourceFile.cc
    src/scathadb/Model/SourceFile.h

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
