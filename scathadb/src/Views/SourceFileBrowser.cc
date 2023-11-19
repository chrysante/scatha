#include "Views/Views.h"

#include "Model/UIHandle.h"
#include "UI/Common.h"

using namespace sdb;
using namespace ftxui;

namespace {

struct FileBrowser: ScrollBase {
    FileBrowser(Model* model, UIHandle& uiHandle) {
        Add(Button("Open file", [&uiHandle] {
            uiHandle.openSourceFile("examples/memory-errors.sc");
        }));
    }
};

} // namespace

Component sdb::SourceFileBrowser(Model* model, UIHandle& uiHandle) {
    return Make<FileBrowser>(model, uiHandle);
}
