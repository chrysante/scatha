#include "Model/Executor.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

#include <magic_enum/magic_enum.hpp>
#include <range/v3/algorithm.hpp>
#include <svm/Util.h>
#include <utl/stack.hpp>
#include <utl/thread.hpp>

#include "Model/BreakpointPatcher.h"
#include "Model/Events.h"

using namespace sdb;
using scdis::InstructionPointerOffset;

using Impl = Executor::Impl;

namespace {

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

    /// Run submitted work items on the VM thread
    DoWork,

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

// State of a stepping operation. There is a stack of these in the executor. We
// push a step state when we begin a stepping operation and pop after finishing
// the operation.
struct StepState {
    enum Type : uint8_t { Line, LineOut, Out, In };

    Type type;

    // TODO: Unify and document the meaning of this variable.
    uint64_t stackPtr;

    // The binary positions at which we pushed breakpoints to perform the step.
    // These must be popped after the stepping operation.
    std::vector<InstructionPointerOffset> breakpoints;

    // Only used for stepping out
    std::optional<InstructionPointerOffset> dest;
};

} // namespace

struct Executor::Impl: Transceiver {
    explicit Impl(std::shared_ptr<Messenger> messenger,
                  scdis::IpoIndexMap const& ipoIndexMap,
                  SourceDebugInfo const& sourceDebugInfo);

    void threadMain();

    void pushCommand(Command command) {
        commandQueue.push(command);
        interruptsSentByExecutor.fetch_add(1);
        virtualMachine.interruptExecution();
    }

    std::optional<Command> tryPopCommand() { return commandQueue.tryPop(); }

    Command waitCommand() { return commandQueue.wait(); }

    Locked<svm::VirtualMachine&> getVM() {
        return { virtualMachine, std::unique_lock(vmMutex) };
    }

    Locked<svm::VirtualMachine&> initVMForExecution();

    bool haveUserBreakpoint(InstructionPointerOffset ipo) const;

    void willStepInstruction(svm::VirtualMachine& vm,
                             InstructionPointerOffset ipo);
    void didStepInstruction(svm::VirtualMachine& vm,
                            InstructionPointerOffset ipo);
    State stepInstruction(svm::VirtualMachine& vm, bool sendUIEncounter = true,
                          bool* didInterrupt = nullptr);

    void willStepSourceLine(svm::VirtualMachine& vm,
                            InstructionPointerOffset ipo);
    bool didStepSourceLine(svm::VirtualMachine& vm,
                           InstructionPointerOffset ipo, bool& isReturn);
    State stepSourceLine(svm::VirtualMachine& vm);

    bool willStepOut(svm::VirtualMachine& v, StepState targetState);
    bool didStepOut(svm::VirtualMachine& vm, InstructionPointerOffset ipo);
    State stepOut(svm::VirtualMachine& vm,
                  StepState targetState = { .type = StepState::Out });

    State handleRuntimeException(svm::VirtualMachine& vm,
                                 svm::RuntimeException const& e);
    State handleStepStateOnBreak(svm::VirtualMachine& vm,
                                 InstructionPointerOffset ipo);

    void killExecution(svm::VirtualMachine& vm);

    void endExecution(svm::VirtualMachine& vm);

    void doOnVMThread(std::function<void(svm::VirtualMachine&)> workItem);
    void performVMThreadWorkQueue(svm::VirtualMachine& vm);

    // State functions

#define X(Name) State do##Name();
    STATE(X)
#undef X

    static const EnumArray<State, State (*)(Impl&)> states;

    scdis::IpoIndexMap const& ipoIndexMap;
    SourceDebugInfo const& sourceDebugInfo;

    std::thread thread;
    std::atomic<State> state = State::Idle;
    CommandQueue commandQueue;
    std::mutex vmMutex;
    svm::VirtualMachine virtualMachine;
    BreakpointPatcher breakpointPatcher;
    std::vector<uint8_t> binary;
    std::vector<std::string> runArguments;
    utl::stack<StepState> stepStateStack;
    bool isContinue = false;
    utl::hashmap<InstructionPointerOffset, TinyBoolStack<>> userBreakpoints;
    std::atomic_int interruptsSentByExecutor = 0;

    std::mutex vmThreadWorkQueueMutex;
    std::queue<std::function<void(svm::VirtualMachine&)>> vmThreadWorkQueue;
};

#define X(Name) [](Impl& impl) { return impl.do##Name(); },
constexpr EnumArray<State, State (*)(Impl&)> Impl::states = { STATE(X) };
#undef X

Executor::Executor(std::shared_ptr<Messenger> messenger,
                   scdis::IpoIndexMap const& ipoIndexMap,
                   SourceDebugInfo const& sourceDebugInfo,
                   std::istream* stdinStream, std::ostream* stdoutStream):
    impl(std::make_unique<Impl>(std::move(messenger), ipoIndexMap,
                                sourceDebugInfo)) {
    impl->virtualMachine.setIOStreams(stdinStream, stdoutStream);
}

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

void Executor::pushBreakpoint(InstructionPointerOffset ipo, bool value) {
    impl->breakpointPatcher.pushBreakpoint(ipo, value);
    impl->userBreakpoints[ipo].push(value);
}

void Executor::popBreakpoint(InstructionPointerOffset ipo) {
    impl->breakpointPatcher.popBreakpoint(ipo);
    auto itr = impl->userBreakpoints.find(ipo);
    assert(itr != impl->userBreakpoints.end());
    itr->second.pop();
    if (itr->second.empty()) impl->userBreakpoints.erase(itr);
}

void Executor::applyBreakpoints(svm::VirtualMachine& vm) {
    if (auto* binary = vm.getBinaryPointer())
        impl->breakpointPatcher.patchInstructionStream(binary);
}

bool Executor::isRunning() const {
    return impl->state.load() == State::RunningIndef;
}

bool Executor::isIdle() const { return impl->state.load() == State::Idle; }

bool Executor::isPaused() const { return impl->state.load() == State::Paused; }

void Executor::loadProgram(std::vector<uint8_t> binary,
                           std::filesystem::path runtimeLibDir) {
    impl->breakpointPatcher.setProgramData(binary);
    impl->binary = std::move(binary);
    impl->getVM().get().setLibdir(std::move(runtimeLibDir));
}

void Executor::unloadProgram() { impl->getVM().get().reset(); }

void Executor::setArguments(std::vector<std::string> arguments) {
    impl->runArguments = std::move(arguments);
}

void Executor::doOnVMThread(
    std::function<void(svm::VirtualMachine&)> workItem) {
    impl->doOnVMThread(std::move(workItem));
}

Impl::Impl(std::shared_ptr<Messenger> messenger,
           scdis::IpoIndexMap const& ipoIndexMap,
           SourceDebugInfo const& sourceDebugInfo):
    Transceiver(std::move(messenger)),
    ipoIndexMap(ipoIndexMap),
    sourceDebugInfo(sourceDebugInfo) {
    listen([this](DoOnVMThread const& event) { doOnVMThread(event.callback); });
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
            assert(stepStateStack.empty());
            auto vm = initVMForExecution();
            auto arg = svm::setupArguments(vm.get(), runArguments);
            breakpointPatcher.removeAll();
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

    case Command::StepInst:
    case Command::StepSourceLine:
    case Command::StepOut:
        return State::Idle;

    case Command::DoWork:
        performVMThreadWorkQueue(getVM().get());
        return State::Idle;

    case Command::Shutdown:
        return State::Stopped;
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

void Impl::doOnVMThread(std::function<void(svm::VirtualMachine&)> workItem) {
    std::unique_lock lock(vmThreadWorkQueueMutex);
    vmThreadWorkQueue.push(std::move(workItem));
    lock.unlock();
    pushCommand(Command::DoWork);
}

void Impl::performVMThreadWorkQueue(svm::VirtualMachine& vm) {
    std::lock_guard lock(vmThreadWorkQueueMutex);
    while (!vmThreadWorkQueue.empty()) {
        vmThreadWorkQueue.front()(vm);
        vmThreadWorkQueue.pop();
    }
}

static bool atomicDetchDecSaturate(std::atomic_int& value) {
    int current = value.load(std::memory_order_relaxed);
    while (true) {
        if (current == 0) return false; // Already at zero, did not decrement
        // Try to subtract 1
        if (value.compare_exchange_weak(current, current - 1,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed))
            return true; // Successfully decremented
        // If it failed, current is updated with the latest value, retry
    }
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
    if (atomicDetchDecSaturate(interruptsSentByExecutor))
        return State::RunningIndef;
    if (!vm.running()) return State::Idle;
    if (!stepStateStack.empty()) return handleStepStateOnBreak(vm, ipo);
    send_buffered(BreakEvent{ ipo, BreakState::Paused });
    return State::Paused;
}

State Impl::handleStepStateOnBreak(svm::VirtualMachine& vm,
                                   InstructionPointerOffset ipo) {
    if (haveUserBreakpoint(ipo)) {
        while (!stepStateStack.empty()) {
            auto stepState = stepStateStack.pop();
            for (auto& ipo: stepState.breakpoints)
                breakpointPatcher.popBreakpoint(ipo);
            if (stepState.dest)
                breakpointPatcher.popBreakpoint(*stepState.dest);
            breakpointPatcher.patchInstructionStream(vm.getBinaryPointer());
        }
        send_buffered(BreakEvent{ ipo, BreakState::Paused });
        return State::Paused;
    }

    assert(!stepStateStack.empty());
    auto& stepState = stepStateStack.top();
    switch (stepState.type) {
    case StepState::Line: {
        bool isReturn = false;
        if (!didStepSourceLine(vm, ipo, isReturn)) {
            assert(ranges::contains(stepState.breakpoints, ipo));
            // We failed to step the source line because we recursively hit our
            // line stepping breakpoint. Pop all the line stepping breakpoints
            // so we can safely step out and continue line stepping.
            StepState stepOutState = { .type = StepState::LineOut };
            for (auto ipo: stepState.breakpoints) {
                if (haveUserBreakpoint(ipo)) continue;
                stepOutState.breakpoints.push_back(ipo);
                breakpointPatcher.pushBreakpoint(ipo, false);
            }
            breakpointPatcher.patchInstructionStream(vm.getBinaryPointer());
            return stepOut(vm, std::move(stepOutState));
        }
        if (isReturn) return stepInstruction(vm);
        break;
    }
    case StepState::Out:
    case StepState::LineOut:
        if (!didStepOut(vm, ipo)) {
            isContinue = true;
            return State::RunningIndef;
        }
        if (!vm.running()) {
            endExecution(vm);
            return State::Idle;
        }
        if (stepState.type == StepState::LineOut) {
            if (bool isReturn;
                ranges::contains(stepStateStack.top().breakpoints, ipo) &&
                didStepSourceLine(vm, ipo, isReturn))
                break;
            isContinue = true;
            return State::RunningIndef;
        }
        break;
    case StepState::In:
        stepStateStack.pop();
        break;
    }
    send_buffered(BreakEvent{ ipo, BreakState::Paused });
    return State::Paused;
}

State Impl::doRunningIndef() {
    auto command = tryPopCommand();
    auto vm = getVM();
    if (!command) {
        try {
            if (std::exchange(isContinue, false)) {
                bool didInterrupt = false;
                auto result = stepInstruction(
                    vm.get(), /* sendUIEncounter (on happy path): */ false,
                    &didInterrupt);
                if (didInterrupt) return State::Paused;
                if (result == State::Idle) return State::Idle;
                vm.get().executeInterruptible();
            }
            else if (vm.get().running())
                vm.get().executeInterruptible();
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

    case Command::StepInst:
    case Command::StepSourceLine:
    case Command::StepOut:
        return State::RunningIndef;

    case Command::DoWork:
        performVMThreadWorkQueue(vm.get());
        return State::RunningIndef;

    case Command::Shutdown:
        killExecution(vm.get());
        return State::Stopped;
    }
}

bool Impl::haveUserBreakpoint(InstructionPointerOffset ipo) const {
    auto itr = userBreakpoints.find(ipo);
    if (itr == userBreakpoints.end()) return false;
    return itr->second.top();
}

void Impl::willStepInstruction(svm::VirtualMachine& vm,
                               InstructionPointerOffset ipo) {
    breakpointPatcher.pushBreakpoint(ipo, false);
    breakpointPatcher.patchInstructionStream(vm.getBinaryPointer());
}

void Impl::didStepInstruction(svm::VirtualMachine& vm,
                              InstructionPointerOffset ipo) {
    breakpointPatcher.popBreakpoint(ipo);
    breakpointPatcher.patchInstructionStream(vm.getBinaryPointer());
}

State Impl::stepInstruction(svm::VirtualMachine& vm, bool sendUIEncounter,
                            bool* didInterrupt) {
    InstructionPointerOffset const ipo(vm.instructionPointerOffset());
    willStepInstruction(vm, ipo);
    try {
        vm.stepExecution();
    }
    catch (svm::RuntimeException const& e) {
        send_buffered(BreakEvent{ ipo, BreakState::Error, e.get() });
        vm.setInstructionPointerOffset(ipo.value);
        didStepInstruction(vm, ipo);
        if (didInterrupt) *didInterrupt = true;
        return State::Paused;
    }
    didStepInstruction(vm, ipo);
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

void Impl::willStepSourceLine(svm::VirtualMachine& vm,
                              InstructionPointerOffset ipo) {
    auto* function = sourceDebugInfo.findFunction(ipo);
    if (!function) return;
    auto beginIndex = ipoIndexMap.ipoToIndex(function->begin);
    if (!beginIndex) return;
    auto startLoc = sourceDebugInfo.sourceMap().toSourceLoc(ipo);
    auto startLine = [&]() -> std::optional<SourceLine> {
        if (!startLoc) return std::nullopt;
        return startLoc->line;
    }();
    StepState stepState{ .type = StepState::Line };
    for (size_t index = *beginIndex; ipoIndexMap.isIndexValid(index); ++index) {
        auto ipo = ipoIndexMap.indexToIpo(index);
        if (ipo >= function->end) break;
        bool isReturn = breakpointPatcher.opcodeAt(ipo) ==
                        scbinutil::OpCode::ret;
        auto sourceLoc = sourceDebugInfo.sourceMap().toSourceLoc(ipo);
        if (isReturn || (sourceLoc && sourceLoc->line != startLine)) {
            stepState.breakpoints.push_back(ipo);
            breakpointPatcher.pushBreakpoint(ipo, true);
        }
    }
    breakpointPatcher.patchInstructionStream(vm.getBinaryPointer());
    stepState.stackPtr =
        std::bit_cast<uint64_t>(vm.getCurrentExecFrame().stackPtr);
    stepStateStack.push(std::move(stepState));
}

bool Impl::didStepSourceLine(svm::VirtualMachine& vm,
                             InstructionPointerOffset ipo, bool& isReturn) {
    auto& stepState = stepStateStack.top();
    auto stackPtr = std::bit_cast<uint64_t>(vm.getCurrentExecFrame().stackPtr);
    if (stepState.stackPtr < stackPtr) return false;
    for (auto ipo: stepState.breakpoints)
        breakpointPatcher.popBreakpoint(ipo);
    breakpointPatcher.patchInstructionStream(vm.getBinaryPointer());
    isReturn = breakpointPatcher.opcodeAt(ipo) == scbinutil::OpCode::ret;
    stepStateStack.pop();
    return true;
}

State Impl::stepSourceLine(svm::VirtualMachine& vm) {
    InstructionPointerOffset const ipo(vm.instructionPointerOffset());
    isContinue = true;
    willStepSourceLine(vm, ipo);
    return State::RunningIndef;
}

bool Impl::willStepOut(svm::VirtualMachine& vm, StepState targetState) {
    auto frame = vm.getCurrentExecFrame();
    if (frame.regPtr - frame.bottomReg < 3) return false;
    auto* retAddr = reinterpret_cast<uint8_t*>(frame.regPtr[-1]);
    targetState.stackPtr = frame.regPtr[-3];
    InstructionPointerOffset dest(
        utl::narrow_cast<uint64_t>(retAddr - vm.getBinaryPointer()));
    breakpointPatcher.pushBreakpoint(dest, true);
    breakpointPatcher.patchInstructionStream(vm.getBinaryPointer());
    targetState.dest = dest;
    stepStateStack.push(std::move(targetState));
    return true;
}

bool Impl::didStepOut(svm::VirtualMachine& vm, InstructionPointerOffset ipo) {
    auto& stepState = stepStateStack.top();
    if (stepState.dest != ipo) return false;
    auto frame = vm.getCurrentExecFrame();
    uint64_t stackPtr = std::bit_cast<uint64_t>(frame.stackPtr);
    if (stepState.stackPtr < stackPtr) return false;
    breakpointPatcher.popBreakpoint(ipo);
    // Reapply all line stepping breakpoints if we are stepping out of a
    // recursively hit line stepping breakpoint.
    if (stepState.type == StepState::LineOut)
        for (auto ipo: stepState.breakpoints)
            breakpointPatcher.popBreakpoint(ipo);
    breakpointPatcher.patchInstructionStream(vm.getBinaryPointer());
    stepStateStack.pop();
    return true;
}

State Impl::stepOut(svm::VirtualMachine& vm, StepState targetState) {
    willStepOut(vm, std::move(targetState));
    isContinue = true;
    return State::RunningIndef;
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

    case Command::DoWork:
        performVMThreadWorkQueue(getVM().get());
        return State::Paused;

    case Command::Shutdown:
        return State::Stopped;
    }
}

State Impl::doStopped() { assert(false); }
