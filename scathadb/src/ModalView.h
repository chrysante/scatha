#ifndef SVM_MODALVIEW_H_
#define SVM_MODALVIEW_H_

#include <string>

#include <ftxui/component/component.hpp>

namespace sdb {

using OpenModalCommand = std::function<void()>;

///
class ModalView {
public:
    struct State {
        bool open;
    };

    static std::shared_ptr<State> makeState() {
        return std::make_shared<State>();
    }

    explicit ModalView(std::string title,
                       ftxui::Component body,
                       std::shared_ptr<State> = nullptr);

    ///
    ftxui::Component component() { return _comp; }

    ///
    void open();

    ///
    OpenModalCommand openCommand();

    ///
    ftxui::ComponentDecorator overlay();

private:
    std::shared_ptr<State> _state;
    ftxui::Component _comp;
};

} // namespace sdb

#endif // SVM_MODALVIEW_H_
