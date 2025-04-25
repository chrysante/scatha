#ifndef SCATHADB_UI_COMMON_H_
#define SCATHADB_UI_COMMON_H_

#include <atomic>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

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
ftxui::Component SplitLeft(ftxui::Component main, ftxui::Component back,
                           ftxui::Ref<int> size = 20);

///
ftxui::Component SplitRight(ftxui::Component main, ftxui::Component back,
                            ftxui::Ref<int> size = 20);

///
ftxui::Component SplitTop(ftxui::Component main, ftxui::Component back,
                          ftxui::Ref<int> size = 10);

///
ftxui::Component SplitBottom(ftxui::Component main, ftxui::Component back,
                             ftxui::Ref<int> size = 10);

/// Option struct for `Toolbar()`
struct ToolbarOptions {
    /// Separator element to draw between toolbar components
    std::function<ftxui::Element()> separator = [] {
        return sdb::separatorBlank();
    };

    /// Whether to render separators next to the left and right most elements
    bool enclosingSeparators = false;
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
class ViewBase: public ftxui::ComponentBase {};

/// Analoguous to `ftxui::Component`
using View = std::shared_ptr<ViewBase>;

/// Base class for scrollable views
class ScrollBase: public ViewBase {
public:
    /// \Returns `true` if line \p line is currently in view
    bool isInView(long line) const;

    /// \Returns the bounding box of this view
    ftxui::Box box() const { return _box; }

    /// \Returns the current scroll position
    long scrollPosition() const { return _scrollPos; }

    /// Maximum scroll position based on the current view contents
    long maxScrollPositition() const;

    /// \Returns the index of the focused line
    long focusLine() const { return _focusLine; }

    /// Add an offset to which line is focused and move view if necessary
    void focusLineOffset(long offset);

protected:
    /// Sets the scroll position to \p value
    void setScroll(long value);

    /// Adds \p offset to the currect scroll position
    void setScrollOffset(long offset);

    /// Centers the view around line \p line
    void center(long line, double ratio = 0.5);

    /// Helper function to be used when overriding `OnEvent()`
    bool handleScroll(ftxui::Event event);

    /// Move view up or down if necessary
    void scrollToLine(long line);

    ///
    void setFocusLine(long line);

    ftxui::Element OnRender() override;
    bool OnEvent(ftxui::Event event) override;
    /// Scroll views are always focusable to allow keyboard navigation
    bool Focusable() const override { return true; }

private:
    bool isScrollUp(ftxui::Event event) const;
    bool isScrollDown(ftxui::Event event) const;
    [[nodiscard]] long clampScroll(long value);

    long _scrollPos = 0;
    long _focusLine = 0;
    ftxui::Box _box, _lastBox;
};

/// Makes the terminal emit a "beep" sound
void beep();

} // namespace sdb

#endif // SCATHADB_UI_COMMON_H_
