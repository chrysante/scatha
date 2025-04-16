#ifndef SDB_VIEWS_FILEVIEWCOMMON_H_
#define SDB_VIEWS_FILEVIEWCOMMON_H_

#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "Model/UIHandle.h" // For BreakState
#include "UI/Common.h"

namespace sdb {

struct LineInfo {
    long lineIndex;
    bool isFocused     : 1;
    bool hasBreakpoint : 1;
    BreakState state   : 3;
};

ftxui::Element breakpointIndicator(LineInfo line);

ftxui::Element lineNumber(LineInfo line);

ftxui::ElementDecorator lineMessageDecorator(std::string message);

class FileViewBase: public ScrollBase {
protected:
    bool OnEvent(ftxui::Event event) override;

private:
    virtual void reload() = 0;
    virtual void clearBreakpoints() = 0;
};

} // namespace sdb

#endif // SDB_VIEWS_FILEVIEWCOMMON_H_
