#ifndef SDB_VIEWS_H_
#define SDB_VIEWS_H_

#include <ftxui/component/component.hpp>

namespace sdb {

class Program;

///
///
///
ftxui::Component ControlView(Program* prog);

///
///
///
ftxui::Component InstructionView(Program* prog);

///
///
///
ftxui::Component RegisterView(Program* prog);

} // namespace sdb

#endif // SDB_VIEWS_H_
