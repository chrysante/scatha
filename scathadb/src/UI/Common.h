#ifndef SDB_UI_COMMON_H_
#define SDB_UI_COMMON_H_

#include <string>

#include <ftxui/component/component.hpp>

namespace sdb {

///
int yExtend(ftxui::Box box);

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
ftxui::Component Toolbar(std::vector<ftxui::Component> components);

/// Groups components with a name used by `TabView()`
struct NamedComponent {
    std::string name;
    ftxui::Component component;
};

///
ftxui::Component TabView(std::vector<NamedComponent> children);

///
ftxui::Element placeholder(std::string message);

///
///
///
class ViewBase: public ftxui::ComponentBase {
public:
    virtual void refresh() {}
};

///
using View = std::shared_ptr<ViewBase>;

///
///
///
class ScrollBase: public ViewBase {
public:
    ftxui::Element Render() override;

    bool OnEvent(ftxui::Event event) override;

protected:
    /// Sets the scroll position to \p value
    void setScroll(long value);

    /// Adds \p offset to the currect scroll position
    void setScrollOffset(long offset);

    /// \Returns `true` if line \p line is currently in view
    bool isInView(long line) const;

    /// Centers the view around line \p line
    void center(long line, double ratio = 0.5);

    /// Helper function to be used when overriding `OnEvent()`
    bool handleScroll(ftxui::Event event, bool allowKeyScroll = true);

    /// \Returns the bounding box of this view
    ftxui::Box box() const { return _box; }

    /// \Returns the current scroll position
    long scrollPosition() const { return scrollPos; }

    /// Maximum scroll position based on the current view contents
    long maxScrollPositition() const;

private:
    bool isScrollUp(ftxui::Event event, bool allowKeyScroll) const;
    bool isScrollDown(ftxui::Event event, bool allowKeyScroll) const;
    void clampScroll();

    long scrollPos = 0;
    ftxui::Box _box, _lastBox;
};

///
void beep();

} // namespace sdb

#endif // SDB_UI_COMMON_H_