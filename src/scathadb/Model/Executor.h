#ifndef SDB_MODEL_EXECUTOR_H_
#define SDB_MODEL_EXECUTOR_H_

#include <concepts>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <svm/VirtualMachine.h>

#include "Model/SourceDebugInfo.h"
#include "Model/UIHandle.h"

namespace sdb {

template <typename T, typename Lock = std::unique_lock<std::mutex>>
struct Locked {
    Locked(T obj, Lock&& lock): obj(obj), lock(std::move(lock)) {}

    template <std::convertible_to<T> U>
    Locked(Locked<U, Lock>&& rhs):
        Locked(std::move(rhs.obj), std::move(rhs.lock)) {}

    T const& get() const { return obj; }

    void unlock() { lock.unlock(); }

private:
    template <typename, typename>
    friend struct Locked;

    T obj;
    Lock lock;
};

///
class Executor {
public:
    struct Impl;

    explicit Executor(UIHandle const* uiHandle);
    Executor(Executor&&) noexcept;
    Executor& operator=(Executor&&) noexcept;
    ~Executor();

    ///
    void startExecution();

    ///
    void stopExecution();

    ///
    void toggleExecution();

    ///
    void stepInstruction();

    ///
    bool isIdle() const;

    ///
    bool isPaused() const;

    ///
    Locked<svm::VirtualMachine const&> readVM();

    ///
    Locked<svm::VirtualMachine&> writeVM();

    ///
    void setBinary(std::vector<uint8_t> binary);

    ///
    void setArguments(std::vector<std::string> arguments);

private:
    std::unique_ptr<Impl> impl;
};

} // namespace sdb

#endif // SDB_MODEL_EXECUTOR_H_
