#ifndef SVM_SCROLLBASE_H_
#define SVM_SCROLLBASE_H_

#include <ftxui/component/component_base.hpp>

namespace sdb {

class ScrollBase: public ftxui::ComponentBase {
public:
    ftxui::Element Render() override;

    bool OnEvent(ftxui::Event event) override;

protected:
    void setScroll(long value);
    void setScrollOffset(long offset);

    bool isInView(size_t index) const;
    void center(size_t index);

private:
    bool isScrollUp(ftxui::Event event) const;
    bool isScrollDown(ftxui::Event event) const;
    bool handleScroll(ftxui::Event event);
    void clampScroll();
    long max() const;

    long scrollPos = 0;
    ftxui::Box box;
};

} // namespace sdb

#endif // SVM_SCROLLBASE_H_
