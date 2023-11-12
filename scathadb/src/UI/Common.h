#ifndef SDB_UI_COMMON_H_
#define SDB_UI_COMMON_H_

#include <string>

#include <ftxui/component/component.hpp>

namespace sdb {

/// Computes the horizontal extend of the box \p box
int xExtend(ftxui::Box box);

/// Computes the vertical extend of the box \p box
int yExtend(ftxui::Box box);

/// Line separator element
ftxui::Element separator();

/// Blank separator element
ftxui::Element separatorBlank();

/// Element dynamically filling available space
ftxui::Element spacer();

/// Placeholder text element
ftxui::Element placeholder(std::string message);

/// Line separator component
ftxui::Component Separator();

/// Blank separator component
ftxui::Component SeparatorBlank();

/// Component dynamically filling available space
ftxui::Component Spacer();

/// Placeholder text component
ftxui::Component Placeholder(std::string message);

///
ftxui::Component SplitLeft(ftxui::Component main,
                           ftxui::Component back,
                           int size = 20);

///
ftxui::Component SplitRight(ftxui::Component main,
                            ftxui::Component back,
                            int size = 20);

///
ftxui::Component SplitTop(ftxui::Component main,
                          ftxui::Component back,
                          int size = 10);

///
ftxui::Component SplitBottom(ftxui::Component main,
                             ftxui::Component back,
                             int size = 10);

/// Option struct for `Toolbar()`
struct ToolbarOptions {
    /// Separator element to draw between toolbar components
    std::function<ftxui::Element()> separator = [] {
        return sdb::separatorBlank();
    };
};

/// Toolbar with separators between elements
ftxui::Component Toolbar(std::vector<ftxui::Component> components,
                         ToolbarOptions options = {});

/// Groups components with a name used by `TabView()`
struct NamedComponent {
    std::string name;
    ftxui::Component component;
};

/// Automatically configured tab view
ftxui::Component TabView(std::vector<NamedComponent> children);

/// Common base class for views in this project
class ViewBase: public ftxui::ComponentBase {
public:
    /// Command to rebuild this view from scratch
    virtual void refresh() {}
};

/// Analoguous to `ftxui::Component`
using View = std::shared_ptr<ViewBase>;

/// Base class for scrollable views
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

/// Makes the terminal emit a "beep" sound
void beep();

} // namespace sdb

#endif // SDB_UI_COMMON_H_
