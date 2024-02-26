#include "Views/Views.h"

#include <range/v3/view.hpp>

#include "Model/Model.h"
#include "Model/SourceDebugInfo.h"
#include "Model/UIHandle.h"
#include "UI/Common.h"

using namespace sdb;
using namespace ftxui;

namespace {

struct FileBrowser: ScrollBase {
    FileBrowser(Model const* model, UIHandle& uiHandle):
        model(model), uiHandle(&uiHandle) {
        uiHandle.addReloadCallback([this] { reload(); });
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
                uiHandle->openSourceFile(index);
            }));
        }
    }

    Model const* model = nullptr;
    UIHandle const* uiHandle = nullptr;
    SourceDebugInfo const* debug = nullptr;
};

} // namespace

Component sdb::SourceFileBrowser(Model* model, UIHandle& uiHandle) {
    return Make<FileBrowser>(model, uiHandle);
}
