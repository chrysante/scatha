#include "Views/Views.h"

#include <range/v3/view.hpp>

#include "App/Messenger.h"
#include "Model/Events.h"
#include "Model/Model.h"
#include "Model/SourceDebugInfo.h"
#include "UI/Common.h"

using namespace sdb;
using namespace ftxui;

namespace {

struct FileBrowser: ScrollBase, Transceiver {
    FileBrowser(Model const* model, std::shared_ptr<Messenger> messenger):
        Transceiver(std::move(messenger)), model(model) {
        listen([this](ReloadUIRequest) { reload(); });
        reload();
    }

    void reload() {
        DetachAllChildren();
        debug = &model->sourceDebug();
        if (debug->empty()) {
            return;
        }
        for (auto [index, file]: debug->files() | ranges::views::enumerate) {
            Add(Button(file.path().filename().string(), [this, index = index] {
                send_buffered(OpenSourceFileRequest{ index });
            }));
        }
    }

    Model const* model = nullptr;
    SourceDebugInfo const* debug = nullptr;
};

} // namespace

Component sdb::SourceFileBrowser(Model* model,
                                 std::shared_ptr<Messenger> messenger) {
    return Make<FileBrowser>(model, std::move(messenger));
}
