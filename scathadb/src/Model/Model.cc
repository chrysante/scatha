#include "Model/Model.h"

#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include <svm/Util.h>
#include <svm/VirtualMemory.h>
#include <utl/strcat.hpp>

using namespace sdb;

namespace {

enum class ExecCommand {
    None,
    Start,
    Stop,
    Toggle,
    StepInstruction,
    StepSourceLine,
};

} // namespace

static decltype(auto) locked(auto& mutex, auto&& fn) {
    std::lock_guard lock(mutex);
    return fn();
}

struct Model::ExecThread {
    std::array<std::function<ExecState()>, 5> states;
    std::thread thread;
    std::mutex commandQueueMutex;
    std::queue<ExecCommand> commands;
    std::condition_variable condVar;
    std::atomic<ExecState> atomicState = ExecState::Stopped;

    explicit ExecThread(std::function<ExecState()> starting,
                        std::function<ExecState()> running,
                        std::function<ExecState()> paused,
                        std::function<ExecState()> stopping,
                        std::function<ExecState()> exiting):
        states({
            std::move(starting),
            std::move(running),
            std::move(paused),
            std::move(stopping),
            std::move(exiting),
        }) {}

    /// To be called from the execution thread
    ExecCommand popCommand() {
        return locked(commandQueueMutex, [&] {
            if (commands.empty()) {
                return ExecCommand::None;
            }
            auto command = commands.front();
            commands.pop();
            return command;
        });
    }

    /// To be called from the execution thread
    ExecCommand waitCommand() {
        std::unique_lock lock(commandQueueMutex);
        condVar.wait(lock, [&] { return !commands.empty(); });
        auto command = commands.front();
        commands.pop();
        return command;
    }

    /// To be called from the controlling thread
    void sendCommand(ExecCommand command) {
        locked(commandQueueMutex, [&] {
            commands.push(command);
            condVar.notify_one();
        });
        using enum ExecCommand;
        switch (command) {
        case Start:
            thread = std::thread([this] { threadFn(); });
            break;
        case Stop:
            if (thread.joinable()) {
                thread.join();
            }
            commands = {};
            break;
        default:
            break;
        }
    }

    /// Execution thread main function
    void threadFn() {
        using enum ExecState;
        auto state = Starting;
        atomicState = state;
        while (state != Stopped) {
            atomicState = state = states[static_cast<size_t>(state)]();
        }
    }
};

Model::Model(UIHandle* uiHandle):
    uiHandle(uiHandle),
    execThread(std::make_unique<ExecThread>([this] { return starting(); },
                                            [this] { return running(); },
                                            [this] { return paused(); },
                                            [this] { return stopping(); },
                                            [this] { return exiting(); })),
    breakpoints(&disasm) {
    vm.setIOStreams(nullptr, &_stdout);
}

Model::~Model() { stop(); }

void Model::loadProgram(std::filesystem::path filepath) {
    stop();
    clearBreakpoints();
    auto binary = svm::readBinaryFromFile(filepath.string());
    if (binary.empty()) {
        std::string progName = filepath.stem();
        auto msg =
            utl::strcat("Failed to load ", progName, ". Binary is empty.\n");
        throw std::runtime_error(msg);
    }
    vm.loadBinary(binary.data());
    _currentFilepath = filepath;
    disasm = disassemble(binary);
    auto dsympath = filepath;
    dsympath += ".scdsym";
    sourceDbg = SourceDebugInfo::Load(dsympath);
    if (uiHandle) {
        uiHandle->reload();
    }
}

void Model::unloadProgram() {
    stop();
    clearBreakpoints();
    _currentFilepath.clear();
}

void Model::setArguments(std::vector<std::string> arguments) {
    runArguments = std::move(arguments);
}

void Model::start() {
    stop();
    using enum ExecCommand;
    execThread->sendCommand(Start);
}

void Model::stop() {
    using enum ExecCommand;
    execThread->sendCommand(Stop);
}

void Model::toggle() {
    using enum ExecCommand;
    execThread->sendCommand(Toggle);
}

ExecState Model::state() const { return execThread->atomicState; }

void Model::stepInstruction() {
    using enum ExecCommand;
    execThread->sendCommand(StepInstruction);
}

void Model::stepSourceLine() {
    using enum ExecCommand;
    execThread->sendCommand(StepSourceLine);
}

std::vector<uint64_t> Model::readRegisters(size_t count) {
    auto regs = vm.registerData();
    return std::vector<uint64_t>(regs.data(), regs.data() + count);
}

ExecState Model::starting() {
    using enum ExecState;
    execThread->popCommand();
    _stdout.str({});
    std::lock_guard lock(vmMutex);
    vm.reset();
    auto execArg = setupArguments(vm, runArguments);
    vm.beginExecution(execArg);
    size_t offset = vm.instructionPointerOffset();
    if (breakpoints.atOffset(offset)) {
        handleInstEncounter(offset, BreakState::Breakpoint);
        return Paused;
    }
    return Running;
}

ExecState Model::running() {
    using enum ExecCommand;
    using enum ExecState;
    switch (execThread->popCommand()) {
    case None:
        return doExecuteSteps();
    case Stop:
        return Stopping;
    case Toggle:
        return Paused;
    default:
        return Running;
    }
}

ExecState Model::paused() {
    using enum ExecCommand;
    using enum ExecState;
    switch (execThread->waitCommand()) {
    case Stop:
        return Stopping;
    case Toggle:
        return Running;
    case StepInstruction:
        return doStepInstruction();
    case StepSourceLine:
        return doStepSourceLine();
    default:
        return Paused;
    }
}

ExecState Model::stopping() {
    using enum ExecState;
    execThread->popCommand();
    vm.ostream() << "Program interrupted" << std::endl;
    uiHandle->refresh();
    return Stopped;
}

ExecState Model::exiting() {
    using enum ExecState;
    execThread->popCommand();
    std::lock_guard lock(vmMutex);
    vm.endExecution();
    vm.ostream() << "Program returned with exit code: " << vm.getRegister(0)
                 << std::endl;
    uiHandle->refresh();
    return Stopped;
}

ExecState Model::doExecuteSteps() {
    using enum ExecState;
    int const NumSteps = 20;
    std::lock_guard bpLock(breakpointMutex);
    size_t offset = 0;
    std::lock_guard vmLock(vmMutex);
    try {
        for (int i = 0; i < NumSteps; ++i) {
            if (!vm.running()) {
                return Exiting;
            }
            offset = vm.instructionPointerOffset();
            vm.stepExecution();
            offset = vm.instructionPointerOffset();
            if (breakpoints.atOffset(offset)) {
                handleInstEncounter(offset, BreakState::Breakpoint);
                return Paused;
            }
        }
        return Running;
    }
    catch (...) {
        handleException();
        handleInstEncounter(offset, BreakState::Error);
        vm.setInstructionPointerOffset(offset);
        return Paused;
    }
}

ExecState Model::doStepInstruction() {
    assert(vm.running() && "Must be active to step");
    using enum ExecState;
    std::lock_guard lock(vmMutex);
    size_t offset = vm.instructionPointerOffset();
    try {
        vm.stepExecution();
    }
    catch (...) {
        handleException();
        handleInstEncounter(offset, BreakState::Error);
        vm.setInstructionPointerOffset(offset);
        return Paused;
    }
    if (vm.running()) {
        handleInstEncounter(vm.instructionPointerOffset(), BreakState::Step);
        return Paused;
    }
    else {
        return Exiting;
    }
}

ExecState Model::doStepSourceLine() { assert(false); }

void Model::handleInstEncounter(size_t offset, BreakState state) {
    auto index = disasm.offsetToIndex(offset);
    uiHandle->hitInstruction(index.value_or(0), state);
}

void Model::handleException() {
    try {
        throw;
    }
    catch (svm::RuntimeException& e) {
        uiHandle->onError(std::move(e).error());
    }
    catch (...) {
        assert(false);
    }
}
