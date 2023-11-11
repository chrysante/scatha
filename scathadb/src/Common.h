#ifndef SDB_COMMON_H_
#define SDB_COMMON_H_

#include <filesystem>
#include <span>
#include <string>

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
ftxui::Component ModalView(std::string title,
                           ftxui::Component body,
                           bool* open);

///
///
///
class ViewBase: public ftxui::ComponentBase {
public:
    virtual void refresh() {}
};

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
    bool isInView(size_t line) const;

    /// Centers the view around line \p line
    void center(size_t line);

    /// Helper function to be used when overriding `OnEvent()`
    bool handleScroll(ftxui::Event event);

    /// \Returns the bounding box of this view
    ftxui::Box box() const { return _box; }

    /// \Returns the current scroll position
    long scrollPosition() const { return scrollPos; }

    /// Maximum scroll position based on the current view contents
    long maxScrollPositition() const;

private:
    bool isScrollUp(ftxui::Event event) const;
    bool isScrollDown(ftxui::Event event) const;
    void clampScroll();

    long scrollPos = 0;
    ftxui::Box _box, _lastBox;
};

///
void beep();

///
struct Options {
    std::filesystem::path filepath;
    std::vector<std::string> arguments;

    explicit operator bool() const {
        return !filepath.empty() || !arguments.empty();
    }
};

///
Options parseArguments(std::span<char*> arguments);

} // namespace sdb

#endif // SDB_COMMON_H_
