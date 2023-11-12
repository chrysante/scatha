#ifndef SVM_UI_MODALVIEW_H_
#define SVM_UI_MODALVIEW_H_

#include <string>

#include <ftxui/component/component.hpp>

namespace sdb {

using OpenModalCommand = std::function<void()>;

/// Represents a modal view, i.e. an overlay popup window
class ModalView {
public:
    /// State structure of a modal view
    struct State {
        bool open;
    };

    /// Manually create a shared state structure. This is useful if the modal
    /// view wants to be able to close itself
    static std::shared_ptr<State> makeState() {
        return std::make_shared<State>();
    }

    /// Construct a modal view
    explicit ModalView(std::string title,
                       ftxui::Component body,
                       std::shared_ptr<State> = nullptr);

    /// \Returns The component of this modal
    ftxui::Component component() { return _comp; }

    /// Opens and focuses this modal
    void open();

    /// \Returns a callback that opens this modal
    OpenModalCommand openCommand();

    /// \Returns a component decorator to register this modal as an overlay
    ftxui::ComponentDecorator overlay();

private:
    std::shared_ptr<State> _state;
    ftxui::Component _comp;
};

} // namespace sdb

#endif // SVM_UI_MODALVIEW_H_
