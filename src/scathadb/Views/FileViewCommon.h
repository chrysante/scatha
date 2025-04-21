#ifndef SDB_VIEWS_FILEVIEWCOMMON_H_
#define SDB_VIEWS_FILEVIEWCOMMON_H_

#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "Model/Events.h" // For BreakState
#include "UI/Common.h"

namespace sdb {

struct LineInfo {
    long lineIndex;
    bool isFocused;
    bool hasBreakpoint;
    BreakState state;
};

ftxui::Element breakpointIndicator(LineInfo line);

ftxui::Element lineNumber(LineInfo line);

ftxui::ElementDecorator lineMessageDecorator(std::string message);

class FileViewBase: public ScrollBase {
protected:
    bool OnEvent(ftxui::Event event) override;

private:
    virtual void clearBreakpoints() = 0;
};

} // namespace sdb

#endif // SDB_VIEWS_FILEVIEWCOMMON_H_
