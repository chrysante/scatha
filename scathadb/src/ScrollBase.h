#ifndef SVM_SCROLLBASE_H_
#define SVM_SCROLLBASE_H_

#include <ftxui/component/component_base.hpp>

namespace sdb {

class ScrollBase: public ftxui::ComponentBase {
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

} // namespace sdb

#endif // SVM_SCROLLBASE_H_
