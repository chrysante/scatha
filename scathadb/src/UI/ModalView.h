#ifndef SDB_UI_MODALVIEW_H_
#define SDB_UI_MODALVIEW_H_

#include <string>

#include <ftxui/component/component.hpp>

namespace sdb {

using OpenModalCommand = std::function<void()>;

/// State structure of a modal view
struct ModalState {
    /// Manually create a shared state structure. This is useful if the modal
    /// view wants to be able to close itself
    static std::shared_ptr<ModalState> make() {
        return std::make_shared<ModalState>();
    }

    /// Flag to indicate whether the modal is visible
    bool open;
};

/// Constructor options for modal view
struct ModalOptions {
    std::shared_ptr<ModalState> state = nullptr;
    bool closeButton = true;
};

/// Represents a modal view, i.e. an overlay popup window
class ModalView {
public:
    /// Construct a modal view
    explicit ModalView(std::string title, ftxui::Component body,
                       ModalOptions options = {});

    /// \Returns The component of this modal
    ftxui::Component component() { return _comp; }

    /// Opens and focuses this modal
    void open();

    /// \Returns a callback that opens this modal
    OpenModalCommand openCommand();

    /// \Returns a component decorator to register this modal as an overlay
    ftxui::ComponentDecorator overlay();

private:
    std::shared_ptr<ModalState> _state;
    ftxui::Component _comp;
};

} // namespace sdb

#endif // SDB_UI_MODALVIEW_H_
