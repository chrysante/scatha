#include "Views/Views.h"

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
        for (auto& file: debug->files()) {
            Add(Button(file.path().filename().string(),
                       [=] { uiHandle->openSourceFile(&file); }));
        }
    }

    Model const* model = nullptr;
    UIHandle const* uiHandle = nullptr;
    SourceDebugInfo const* debug;
};

} // namespace

Component sdb::SourceFileBrowser(Model* model, UIHandle& uiHandle) {
    return Make<FileBrowser>(model, uiHandle);
}
