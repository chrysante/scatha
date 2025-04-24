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
#include <utl/thread.hpp>

#include "Model/Events.h"

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
    StepSourceLine,

    ///
    StepOut,

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

struct Executor::Impl: Transceiver {
    explicit Impl(std::shared_ptr<Messenger> messenger);

    void threadMain();

    void pushCommand(Command command) {
        commandQueue.push(command);
        virtualMachine.interruptExecution();
    }

    std::optional<Command> tryPopCommand() { return commandQueue.tryPop(); }

    Command waitCommand() { return commandQueue.wait(); }

    Locked<svm::VirtualMachine&> getVM() {
        return { virtualMachine, std::unique_lock(vmMutex) };
    }

    Locked<svm::VirtualMachine&> initVMForExecution();

    State stepInstruction(svm::VirtualMachine& vm, bool sendUIEncounter = true);

    State stepSourceLine(svm::VirtualMachine& vm);

    State stepOut(svm::VirtualMachine& vm);

    State handleRuntimeException(svm::VirtualMachine& vm,
                                 svm::RuntimeException const& e);
    bool runInterruptCallback(svm::VirtualMachine& vm);

    void killExecution(svm::VirtualMachine& vm);

    void endExecution(svm::VirtualMachine& vm);

    // State functions

#define X(Name) State do##Name();
    STATE(X)
#undef X

    static const EnumArray<State, State (*)(Impl&)> states;

    std::thread thread;
    std::atomic<State> state = State::Idle;
    CommandQueue commandQueue;
    std::mutex vmMutex;
    svm::VirtualMachine virtualMachine;
    std::vector<uint8_t> binary;
    std::vector<std::string> runArguments;
    std::function<void(svm::VirtualMachine&)> interruptCallback;
    std::mutex interruptCallbackMutex;
    bool isContinue = false;

    enum class StepState : unsigned { None, Line, Out, In };
    StepState stepState = StepState::None;
};

#define X(Name) [](Impl& impl) { return impl.do##Name(); },
constexpr EnumArray<State, State (*)(Impl&)> Impl::states = { STATE(X) };
#undef X

Executor::Executor(std::shared_ptr<Messenger> messenger):
    impl(std::make_unique<Impl>(std::move(messenger))) {}

Executor::Executor(Executor&&) noexcept = default;

Executor& Executor::operator=(Executor&&) noexcept = default;

Executor::~Executor() { shutdown(); }

void Executor::startExecution() { impl->pushCommand(Command::StartExecution); }

void Executor::stopExecution() {
    impl->pushCommand(Command::StopExecution);
    while (impl->state.load() != State::Idle)
        std::this_thread::yield();
}

void Executor::toggleExecution() {
    if (isRunning())
        impl->virtualMachine.interruptExecution();
    else
        impl->pushCommand(Command::ToggleExecution);
}

void Executor::stepInstruction() { impl->pushCommand(Command::StepInst); }

void Executor::stepSourceLine() { impl->pushCommand(Command::StepSourceLine); }

void Executor::stepOut() { impl->pushCommand(Command::StepOut); }

void Executor::shutdown() {
    if (impl->thread.joinable()) {
        impl->pushCommand(Command::Shutdown);
        impl->thread.join();
    }
}

bool Executor::isRunning() const {
    return impl->state.load() == State::RunningIndef;
}

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

Impl::Impl(std::shared_ptr<Messenger> messenger):
    Transceiver(std::move(messenger)) {
    listen([this](DoInterruptedOnVM const& event) {
        if (state.load() != State::RunningIndef) {
            event.callback(getVM().get());
        }
        else {
            std::unique_lock lock(interruptCallbackMutex);
            interruptCallback = event.callback;
            lock.unlock();
            virtualMachine.interruptExecution();
        }
    });
    listen([this](IsExecIdle event) {
        *event.value = state.load() == State::Idle;
    });
    thread = std::thread(&Impl::threadMain, this);
}

void Impl::threadMain() {
    utl::set_current_thread_name("Executor");
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

State Impl::doIdle() {
    switch (waitCommand()) {
    case Command::StartExecution:
        try {
            stepState = StepState::None;
            auto vm = initVMForExecution();
            auto arg = svm::setupArguments(vm.get(), runArguments);
            send_now(WillBeginExecution{ vm.get() });
            vm.get().beginExecution(arg);
            return State::RunningIndef;
        }
        catch (svm::RuntimeException const& e) {
            send_buffered(PatientStartFailureEvent{ e.get() });
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
    case Command::StepSourceLine:
    case Command::StepOut:
        return State::Idle;
    }
}

void Impl::killExecution(svm::VirtualMachine& vm) {
    vm.ostream() << "Process killed\n";
    send_now(ProcessKilled{});
}

void Impl::endExecution(svm::VirtualMachine& vm) {
    vm.endExecution();
    vm.ostream() << "Process returned with exit code: " << vm.getRegister(0)
                 << "\n";
    send_now(ProcessTerminated{});
}

State Impl::handleRuntimeException(svm::VirtualMachine& vm,
                                   svm::RuntimeException const& e) {
    InstructionPointerOffset ipo(vm.instructionPointerOffset());
    if (!e.get().is<svm::InterruptException>()) {
        send_buffered(BreakEvent{ ipo, BreakState::Error, e.get() });
        // Set the instruction pointer to where it was before executing the
        // error
        vm.setInstructionPointerOffset(ipo.value);
        return State::Paused;
    }
    if (runInterruptCallback(vm)) return State::RunningIndef;
    if (!vm.running()) return State::Idle;
    switch (std::exchange(stepState, StepState::None)) {
    case StepState::Line: {
        bool isReturn = false;
        send_now(DidStepSourceLine{ vm, ipo, &isReturn });
        if (isReturn) return stepInstruction(vm);
        break;
    }
    case StepState::Out: {
        bool done = false;
        send_now(DidStepOut{ vm, ipo, &done });
        if (!done) {
            stepState = StepState::Out;
            return State::RunningIndef;
        }
        if (!vm.running()) {
            endExecution(vm);
            return State::Idle;
        }
        break;
    }
    case StepState::In:
        break;
    case StepState::None:
        break;
    }
    send_buffered(BreakEvent{ ipo, BreakState::Paused });
    return State::Paused;
}

bool Impl::runInterruptCallback(svm::VirtualMachine& vm) {
    std::lock_guard lock(interruptCallbackMutex);
    if (!interruptCallback) return false;
    interruptCallback(vm);
    interruptCallback = nullptr;
    return true;
}

State Impl::doRunningIndef() {
    auto command = tryPopCommand();
    auto vm = getVM();
    if (!command) {
        try {
            if (std::exchange(isContinue, false)) {
                stepInstruction(vm.get(), /* sendUIEncounter: */ false);
            }
            if (vm.get().running()) vm.get().executeInterruptible();
        }
        catch (svm::RuntimeException const& e) {
            return handleRuntimeException(vm.get(), e);
        }
        endExecution(vm.get());
        return State::Idle;
    }
    switch (*command) {
    case Command::StartExecution:
        return State::RunningIndef;

    case Command::StopExecution:
        killExecution(vm.get());
        return State::Idle;

    case Command::ToggleExecution: {
        InstructionPointerOffset ipo(vm.get().instructionPointerOffset());
        send_buffered(BreakEvent{ ipo, BreakState::Paused });
        return State::Paused;
    }
    case Command::Shutdown:
        killExecution(vm.get());
        return State::Stopped;

    case Command::StepInst:
    case Command::StepSourceLine:
    case Command::StepOut:
        return State::RunningIndef;
    }
}

State Impl::stepInstruction(svm::VirtualMachine& vm, bool sendUIEncounter) {
    InstructionPointerOffset const ipo(vm.instructionPointerOffset());
    send_now(WillStepInstruction{ vm, ipo });
    try {
        vm.stepExecution();
    }
    catch (svm::RuntimeException const& e) {
        send_buffered(BreakEvent{ ipo, BreakState::Error, e.get() });
        vm.setInstructionPointerOffset(ipo.value);
        send_now(DidStepInstruction{ vm, ipo });
        return State::Paused;
    }
    send_now(DidStepInstruction{ vm, ipo });
    if (vm.running()) {
        if (sendUIEncounter) {
            InstructionPointerOffset ipoAfter(vm.instructionPointerOffset());
            send_buffered(BreakEvent{ ipoAfter, BreakState::Step });
        }
        return State::Paused;
    }
    else {
        endExecution(vm);
        return State::Idle;
    }
}

State Impl::stepSourceLine(svm::VirtualMachine& vm) {
    InstructionPointerOffset const ipo(vm.instructionPointerOffset());
    send_now(WillStepSourceLine{ vm, ipo });
    stepState = StepState::Line;
    isContinue = true;
    return State::RunningIndef;
}

State Impl::stepOut(svm::VirtualMachine& vm) {
    InstructionPointerOffset const ipo(vm.instructionPointerOffset());
    bool possible = true;
    send_now(WillStepOut{ vm, ipo, &possible });
    if (possible) {
        stepState = StepState::Out;
        isContinue = true;
        return State::RunningIndef;
    }
    else {
        // If stepping out is not possible, because we are in the root function,
        // we just continue normally
        stepState = StepState::None;
        isContinue = true;
        return State::RunningIndef;
    }
}

State Impl::doPaused() {
    switch (waitCommand()) {
    case Command::StartExecution:
        return State::Paused;

    case Command::StopExecution:
        killExecution(getVM().get());
        return State::Idle;

    case Command::ToggleExecution:
        isContinue = true;
        return State::RunningIndef;

    case Command::StepInst:
        return stepInstruction(getVM().get());

    case Command::StepSourceLine:
        return stepSourceLine(getVM().get());

    case Command::StepOut:
        return stepOut(getVM().get());

    case Command::Shutdown:
        return State::Stopped;
    }
}

State Impl::doStopped() { assert(false); }
