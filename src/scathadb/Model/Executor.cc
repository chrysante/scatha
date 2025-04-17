#include "Model/Executor.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

#include <magic_enum/magic_enum.hpp>
#include <svm/Util.h>

using namespace sdb;
using scdis::InstructionPointerOffset;

using Impl = Executor::Impl;

namespace {

template <typename E>
    requires std::is_enum_v<E>
inline constexpr std::size_t EnumCount = magic_enum::enum_count<E>();

template <typename E, typename T>
struct EnumArray: std::array<T, EnumCount<E>> {
    static_assert(sizeof(EnumCount<E>));

    using std::array<T, EnumCount<E>>::operator[];

    T& operator[](E e) { return (*this)[(size_t)e]; }

    T const& operator[](E e) const { return (*this)[(size_t)e]; }
};

#define STATE(X)                                                               \
    /*  Not executing,  */                                                     \
    X(Idle)                                                                    \
                                                                               \
    /* Default indefinite running state */                                     \
    X(RunningIndef)                                                            \
                                                                               \
    /*  */                                                                     \
    X(Paused)                                                                  \
                                                                               \
    /* Shut down, waiting for cleanup */                                       \
    X(Stopped)

enum class State {
#define X(Name) Name,
    STATE(X)
#undef X
};

enum class Command {
    ///
    StartExecution,

    ///
    StopExecution,

    ///
    ToggleExecution,

    ///
    StepInst,

    ///
    Shutdown
};

class CommandQueue {
public:
    void push(Command command) {
        std::lock_guard lock(mutex);
        queue.push(command);
        condVar.notify_one();
    }

    std::optional<Command> tryPop() {
        std::lock_guard lock(mutex);
        if (queue.empty()) return std::nullopt;
        return doPop();
    }

    Command wait() {
        std::unique_lock lock(mutex);
        if (!queue.empty()) return doPop();
        condVar.wait(lock, [this] { return !queue.empty(); });
        return doPop();
    }

private:
    Command doPop() {
        auto command = queue.front();
        queue.pop();
        return command;
    }

    std::mutex mutex;
    std::queue<Command> queue;
    std::condition_variable condVar;
};

} // namespace

struct Executor::Impl {
    explicit Impl(UIHandle const* uiHandle);

    void threadMain();

    Locked<svm::VirtualMachine&> getVM() {
        return { virtualMachine, std::unique_lock(vmMutex) };
    }

    Locked<svm::VirtualMachine&> initVMForExecution();

    bool handleBreakpoint(InstructionPointerOffset ipo);

    State executeSteps(svm::VirtualMachine& vm);
    State stepInstruction(svm::VirtualMachine& vm);

    // State functions

#define X(Name) State do##Name();
    STATE(X)
#undef X

    static const EnumArray<State, State (*)(Impl&)> states;

    UIHandle const* uiHandle;
    std::thread thread;
    std::atomic<State> state = State::Idle;
    CommandQueue commandQueue;
    std::mutex vmMutex;
    svm::VirtualMachine virtualMachine;
    std::vector<uint8_t> binary;
    std::vector<std::string> runArguments;
};

#define X(Name) [](Impl& impl) { return impl.do##Name(); },
constexpr EnumArray<State, State (*)(Impl&)> Impl::states = { STATE(X) };
#undef X

Executor::Executor(UIHandle const* uiHandle):
    impl(std::make_unique<Impl>(uiHandle)) {}

Executor::Executor(Executor&&) noexcept = default;

Executor& Executor::operator=(Executor&&) noexcept = default;

Executor::~Executor() {
    impl->commandQueue.push(Command::Shutdown);
    impl->thread.join();
}

void Executor::startExecution() {
    impl->commandQueue.push(Command::StartExecution);
}

void Executor::stopExecution() {
    impl->commandQueue.push(Command::StopExecution);
}

void Executor::toggleExecution() {
    impl->commandQueue.push(Command::ToggleExecution);
}

void Executor::stepInstruction() { impl->commandQueue.push(Command::StepInst); }

bool Executor::isIdle() const { return impl->state.load() == State::Idle; }

bool Executor::isPaused() const { return impl->state.load() == State::Paused; }

Locked<svm::VirtualMachine const&> Executor::readVM() { return impl->getVM(); }

Locked<svm::VirtualMachine&> Executor::writeVM() { return impl->getVM(); }

void Executor::setBinary(std::vector<uint8_t> binary) {
    impl->binary = std::move(binary);
}

void Executor::setArguments(std::vector<std::string> arguments) {
    impl->runArguments = std::move(arguments);
}

Impl::Impl(UIHandle const* uiHandle):
    uiHandle(uiHandle), thread(&Impl::threadMain, this) {}

void Impl::threadMain() {
    while (true) {
        auto before = state.load(std::memory_order::relaxed);
        if (before == State::Stopped) return;
        auto after = states[before](*this);
        state.store(after, std::memory_order::relaxed);
    }
}

Locked<svm::VirtualMachine&> Impl::initVMForExecution() {
    auto vm = getVM();
    vm.get().loadBinary(binary.data());
    return vm;
}

static void handleException(UIHandle const* uiHandle) {
    try {
        throw;
    }
    catch (svm::RuntimeException& e) {
        uiHandle->onError(std::move(e).error());
    }
    catch (...) {
        // TODO: Display error message
        std::exit(42);
    }
}

State Impl::doIdle() {
    switch (commandQueue.wait()) {
    case Command::StartExecution:
        try {
            auto vm = initVMForExecution();
            auto arg = svm::setupArguments(vm.get(), runArguments);
            vm.get().beginExecution(arg);
            return State::RunningIndef;
        }
        catch (...) {
            handleException(uiHandle);
            return State::Idle;
        }

    case Command::StopExecution:
        // Do nothing
        return State::Idle;

    case Command::ToggleExecution:
        // Do nothing
        return State::Idle;

    case Command::Shutdown:
        return State::Stopped;

    case Command::StepInst:
        return State::Idle;
    }
}

static void interruptExecution(svm::VirtualMachine& vm,
                               UIHandle const* uiHandle) {
    vm.ostream() << "Program interrupted" << std::endl;
    uiHandle->refresh();
}

static void endExecution(svm::VirtualMachine& vm, UIHandle const* uiHandle) {
    vm.endExecution();
    vm.ostream() << "Program returned with exit code: " << vm.getRegister(0)
                 << std::endl;
    uiHandle->refresh();
}

bool Impl::handleBreakpoint(InstructionPointerOffset ipo) {
    if (/* find breakpoint */ false) {
        uiHandle->encounter(ipo, BreakState::Breakpoint);
        return true;
    }
    return false;
}

constexpr int NumExecStepsPerFSMStep = 20;

State Impl::executeSteps(svm::VirtualMachine& vm) {
    InstructionPointerOffset ipo;
    try {
        for (int i = 0; i < NumExecStepsPerFSMStep; ++i) {
            if (!vm.running()) {
                endExecution(vm, uiHandle);
                return State::Idle;
            }
            ipo.value = vm.instructionPointerOffset();
            vm.stepExecution();
            ipo.value = vm.instructionPointerOffset();
            if (handleBreakpoint(ipo)) return State::Paused;
        }
        return State::RunningIndef;
    }
    catch (...) {
        handleException(uiHandle);
        uiHandle->encounter(ipo, BreakState::Error);
        // Set the instruction pointer to where it was before executing the
        // error
        vm.setInstructionPointerOffset(ipo.value);
        return State::Paused;
    }
}

State Impl::doRunningIndef() {
    auto command = commandQueue.tryPop();
    auto vm = getVM();
    if (!command) return executeSteps(vm.get());
    switch (*command) {
    case Command::StartExecution:
        return State::RunningIndef;

    case Command::StopExecution:
        interruptExecution(vm.get(), uiHandle);
        return State::Idle;

    case Command::ToggleExecution: {
        InstructionPointerOffset ipo(vm.get().instructionPointerOffset());
        uiHandle->encounter(ipo, BreakState::None);
        return State::Paused;
    }
    case Command::Shutdown:
        interruptExecution(vm.get(), uiHandle);
        return State::Stopped;

    case Command::StepInst:
        return State::RunningIndef;
    }
}

State Impl::stepInstruction(svm::VirtualMachine& vm) {
    InstructionPointerOffset ipo(vm.instructionPointerOffset());
    try {
        vm.stepExecution();
    }
    catch (...) {
        handleException(uiHandle);
        uiHandle->encounter(ipo, BreakState::Error);
        vm.setInstructionPointerOffset(ipo.value);
        return State::Paused;
    }
    if (vm.running()) {
        ipo.value = vm.instructionPointerOffset();
        uiHandle->encounter(ipo, BreakState::Step);
        return State::Paused;
    }
    else {
        endExecution(vm, uiHandle);
        return State::Stopped;
    }
}

State Impl::doPaused() {
    switch (commandQueue.wait()) {
    case Command::StartExecution:
        return State::Paused;

    case Command::StopExecution:
        interruptExecution(getVM().get(), uiHandle);
        return State::Idle;

    case Command::ToggleExecution:
        return State::RunningIndef;

    case Command::StepInst:
        return stepInstruction(getVM().get());

    case Command::Shutdown:
        return State::Stopped;
    }
}

State Impl::doStopped() { assert(false); }
