add_executable(scathadb)
set_target_properties(scathadb PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(scathadb
  PRIVATE
    libsvm
    range-v3
    utility
    nlohmann_json
    CLI11::CLI11
    ftxui::screen
    ftxui::dom
    ftxui::component
)

target_include_directories(scathadb
  PRIVATE
    src
)

target_sources(scathadb
  PRIVATE
    src/App/Command.cc
    src/App/Command.h
    src/App/Debugger.cc
    src/App/Debugger.h
    src/App/Main.cc

    src/Model/Breakpoint.cc
    src/Model/Breakpoint.h
    src/Model/Disassembler.cc
    src/Model/Disassembler.h
    src/Model/Model.cc
    src/Model/Model.h
    src/Model/Options.cc
    src/Model/Options.h
    src/Model/SourceDebugInfo.cc
    src/Model/SourceDebugInfo.h
    src/Model/SourceFile.cc
    src/Model/SourceFile.h
    src/Model/UIHandle.cc
    src/Model/UIHandle.h

    src/UI/Common.cc
    src/UI/Common.h
    src/UI/ModalView.cc
    src/UI/ModalView.h

    src/Views/ConsoleView.cc
    src/Views/FileViewCommon.cc
    src/Views/FileViewCommon.h
    src/Views/HelpPanel.cc
    src/Views/HelpPanel.h
    src/Views/InstructionView.cc
    src/Views/OpenFilePanel.cc
    src/Views/QuitConfirm.cc
    src/Views/Settings.cc
    src/Views/SourceFileBrowser.cc
    src/Views/SourceView.cc
    src/Views/Toolbar.cc
    src/Views/VMStateView.cc
    src/Views/Views.h
)

source_group(src/App   REGULAR_EXPRESSION "src/App/*")
source_group(src/Model REGULAR_EXPRESSION "src/Model/*")
source_group(src/UI    REGULAR_EXPRESSION "src/UI/*")
source_group(src/Views REGULAR_EXPRESSION "src/Views/*")
