#include "Model/Model.h"

#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include <svm/Util.h>
#include <utl/strcat.hpp>

#include "Model/InstructionBreakpoint.h"

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

struct Model::ExecThread {
    std::array<std::function<ExecState()>, 4> states;
    std::thread thread;
    std::mutex commandQueueMutex;
    std::queue<ExecCommand> commands;
    std::condition_variable condVar;
    std::atomic<ExecState> atomicState;

    explicit ExecThread(std::function<ExecState()> starting,
                        std::function<ExecState()> running,
                        std::function<ExecState()> paused,
                        std::function<ExecState()> terminating):
        states({
            std::move(starting),
            std::move(running),
            std::move(paused),
            std::move(terminating),
        }) {}

    /// To be called from the execution thread
    ExecCommand popCommand() {
        std::lock_guard lock(commandQueueMutex);
        if (commands.empty()) {
            return ExecCommand::None;
        }
        auto command = commands.front();
        commands.pop();
        return command;
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
        using enum ExecCommand;
        std::lock_guard lock(commandQueueMutex);
        commands.push(command);
        switch (command) {
        case Start:
            if (thread.joinable()) {
                thread.join();
            }
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
    execThread(std::make_unique<ExecThread>([this] { return starting(); },
                                            [this] { return running(); },
                                            [this] { return paused(); },
                                            [this] { return stopping(); })),
    uiHandle(uiHandle),
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
    //    _currentFilepath = filepath;
    disasm = disassemble(binary);
    uiHandle->reload();
}

void Model::unloadProgram() {}

void Model::setArguments(std::span<std::string const> arguments) {}

void Model::start() {
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

void Model::toggleInstBreakpoint(size_t index) {
    size_t offset = disasm.indexToOffset(index);
    if (breakpoints.at(offset)) {
        breakpoints.erase(offset);
    }
    else {
        breakpoints.add(offset, std::make_unique<InstructionBreakpoint>());
    }
}

ExecState Model::starting() {
    using enum ExecState;
    execThread->popCommand();
    vm.beginExecution({});
    if (auto* BP = breakpoints.at(vm.instructionPointerOffset())) {
        BP->onHit();
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
    vm.endExecution();
    vm.ostream() << "Program returned with exit code: " << vm.getRegister(0)
                 << std::endl;
    return Stopped;
}

ExecState Model::doExecuteSteps() {
    using enum ExecState;
    int const NumSteps = 20;
    std::lock_guard lock(breakpointMutex);
    for (int i = 0; i < NumSteps; ++i) {
        if (!vm.running()) {
            return Stopping;
        }
        vm.stepExecution();
        if (auto* BP = breakpoints.at(vm.instructionPointerOffset())) {
            BP->onHit();
            return Paused;
        }
    }
    return Running;
}

ExecState Model::doStepInstruction() {
    assert(vm.running() && "Must be active to step");
    using enum ExecState;
    vm.stepExecution();
    if (vm.running()) {
        return Paused;
    }
    else {
        return Stopping;
    }
}

ExecState Model::doStepSourceLine() { assert(false); }
