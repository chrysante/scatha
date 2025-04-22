#ifndef SDB_MODEL_EXECUTOR_H_
#define SDB_MODEL_EXECUTOR_H_

#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <svm/VirtualMachine.h>

#include "App/Messenger.h"
#include "Model/SourceDebugInfo.h"

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

    explicit Executor(std::shared_ptr<Messenger> messenger);
    Executor(Executor&&) noexcept;
    Executor& operator=(Executor&&) noexcept;
    ~Executor();

    /// Starts execution of the loaded program
    void startExecution();

    /// Stops execution of the currently running program
    void stopExecution();

    /// Pauses or continues execution of the currently running program
    void toggleExecution();

    /// Steps one instruction of the currently running paused program
    void stepInstruction();

    /// Steps one instruction of the currently running paused program
    void stepSourceLine();

    /// \Returns `true` if a program is currently running
    bool isRunning() const;

    /// \Returns `true` if no program is currently running
    bool isIdle() const;

    /// \Returns `true` if the currently running program is paused
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
