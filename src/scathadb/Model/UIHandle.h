#ifndef SDB_MODEL_UIHANDLE_H_
#define SDB_MODEL_UIHANDLE_H_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <vector>

#include "Model/SourceDebugInfo.h"
#include <svm/Errors.h>

namespace sdb {

class SourceFile;

enum class BreakState {
    None,
    Step,
    Breakpoint,
    Error,
};

/// Provides callback methods for the model to signal the UI
class UIHandle {
public:
    /// Called when a background event occurs that requires the UI to update
    void refresh() const {
        for (auto& cb: refreshCallbacks) {
            cb();
        }
    }

    void addRefreshCallback(std::function<void()> cb) {
        refreshCallbacks.push_back(std::move(cb));
    }

    /// Called when a major background event occurs that requires the UI to
    /// reconstruct
    void reload() const {
        for (auto& cb: reloadCallbacks) {
            cb();
        }
        refresh();
    }

    void addReloadCallback(std::function<void()> cb) {
        reloadCallbacks.push_back(std::move(cb));
    }

    /// Called when an instruction has been hit at index \p index
    void hitInstruction(size_t index, BreakState state) const {
        for (auto& cb: instCallbacks) {
            cb(index, state);
        }
        refresh();
    }

    void addInstCallback(std::function<void(size_t, BreakState)> cb) {
        instCallbacks.push_back(std::move(cb));
    }

    /// Called when a source line has been hit at index \p index
    void hitSourceLocation(SourceLocation SL, BreakState state) const {
        for (auto& cb: sourceCallbacks) {
            cb(SL, state);
        }
        refresh();
    }

    void addSourceCallback(std::function<void(SourceLocation, BreakState)> cb) {
        sourceCallbacks.push_back(std::move(cb));
    }

    /// Called when execution resumes after being paused
    void resume() {
        for (auto& cb: resumeCallbacks) {
            cb();
        }
        refresh();
    }

    void addResumeCallback(std::function<void()> cb) {
        resumeCallbacks.push_back(std::move(cb));
    }

    /// Called when an error is reported by VM execution
    void onError(svm::ErrorVariant error) const {
        for (auto& cb: errorCallbacks) {
            cb(error);
        }
        refresh();
    }

    void addErrorCallback(std::function<void(svm::ErrorVariant)> cb) {
        errorCallbacks.push_back(std::move(cb));
    }

    /// Called to open a source file
    void openSourceFile(size_t index) const {
        for (auto& cb: openSourceFileCallbacks) {
            cb(index);
        }
        refresh();
    }

    void addOpenSourceFileCallback(std::function<void(size_t index)> cb) {
        openSourceFileCallbacks.push_back(std::move(cb));
    }

private:
    std::vector<std::function<void()>> refreshCallbacks;
    std::vector<std::function<void()>> reloadCallbacks;
    std::vector<std::function<void(size_t, BreakState)>> instCallbacks;
    std::vector<std::function<void(SourceLocation, BreakState)>>
        sourceCallbacks;
    std::vector<std::function<void()>> resumeCallbacks;
    std::vector<std::function<void(svm::ErrorVariant)>> errorCallbacks;
    std::vector<std::function<void(size_t index)>> openSourceFileCallbacks;
};

} // namespace sdb

#endif // SDB_MODEL_UIHANDLE_H_
