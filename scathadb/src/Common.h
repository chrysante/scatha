#ifndef SDB_COMMON_H_
#define SDB_COMMON_H_

#include <ftxui/component/component.hpp>

namespace sdb {

///
ftxui::Component separator();

///
ftxui::Component separatorEmpty();

///
ftxui::Component spacer();

///
ftxui::Component splitLeft(ftxui::Component main,
                           ftxui::Component back,
                           int size = 20);

///
ftxui::Component splitRight(ftxui::Component main,
                            ftxui::Component back,
                            int size = 20);

///
ftxui::Component splitTop(ftxui::Component main,
                          ftxui::Component back,
                          int size = 10);

///
ftxui::Component splitBottom(ftxui::Component main,
                             ftxui::Component back,
                             int size = 10);

///
void beep();

} // namespace sdb

#endif // SDB_COMMON_H_
