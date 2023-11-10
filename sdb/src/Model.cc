#include "Model.h"

#include <ostream>

#include "Common.h"

using namespace sdb;

Model::Model(svm::VirtualMachine vm,
             uint8_t const* program,
             std::array<uint64_t, 2> arguments):
    vm(std::move(vm)), disasm(disassemble(program)), arguments(arguments) {
    virtualMachine().setIOStreams(nullptr, &standardout());
}

Model::~Model() {
    {
        std::lock_guard lock(mutex);
        execThreadRunning = false;
        send(Signal::Terminate);
    }
    if (executionThread.joinable()) {
        executionThread.join();
    }
}

void Model::startExecutionThread() {
    signal = Signal::Run;
    execThreadRunning = true;
    executionThread = std::thread([this] {
        vm.beginExecution(arguments);
        updateInstIndex();
        handlePausedOrBreakpoint();
        while (execThreadRunning) {
            refreshScreen();
            std::unique_lock lock(mutex);
            condVar.wait(lock, [&] { return signal != Signal::Sleep; });
            auto sig = signal;
            lock.unlock();
            switch (sig) {
            case Signal::Sleep:
                break;
            case Signal::Step: {
                std::lock_guard lock(mutex);
                if (vm.running()) {
                    vm.stepExecution();
                }
                execThreadRunning = vm.running();
                signal = Signal::Sleep;
                updateInstIndex();
                setScroll(currentIndex);
                break;
            }
            case Signal::Run: {
                while (vm.running()) {
                    std::lock_guard lock(mutex);
                    vm.stepExecution();
                    updateInstIndex();
                    if (handlePausedOrBreakpoint()) {
                        break;
                    }
                    refreshScreen();
                }
                if (!vm.running()) {
                    execThreadRunning = false;
                }
                std::lock_guard lock(mutex);
                signal = Signal::Sleep;
                break;
            }
            case Signal::Terminate:
                execThreadRunning = false;
                break;
            }
        }
        if (!vm.running()) {
            vm.endExecution();
            vm.ostream() << "Program returned with exit code: "
                         << vm.getRegister(0) << std::endl;
        }
        refreshScreen(FORCE);
    });
}

bool Model::handlePausedOrBreakpoint() {
    if (signal != Signal::Run || breakpoints.contains(currentIndex)) {
        signal = Signal::Sleep;
        setScroll(currentIndex);
        return true;
    }
    return false;
}

void Model::toggleExecution() {
    std::lock_guard lock(mutex);
    if (signal == Signal::Sleep) {
        send(Signal::Run);
    }
    else {
        send(Signal::Sleep);
    }
}

void Model::skipLine() {
    std::lock_guard lock(mutex);
    send(Signal::Step);
}

void Model::enterFunction() { beep(); }

void Model::exitFunction() { beep(); }

void Model::restart() {
    _stdout.str({});
    {
        std::lock_guard lock(mutex);
        send(Signal::Terminate);
    }
    executionThread.join();
    startExecutionThread();
}

bool Model::isSleeping() const {
    std::lock_guard lock(mutex);
    return signal == Signal::Sleep;
}

std::vector<uint64_t> Model::readRegisters(size_t numRegisters) const {
    std::vector<uint64_t> result;
    result.reserve(numRegisters);
    std::lock_guard lock(mutex);
    auto frame = vm.getCurrentExecFrame();
    for (size_t i = 0; i < numRegisters; ++i) {
        result.push_back(frame.bottomReg[i]);
    }
    return result;
}

void Model::send(Signal signal) {
    this->signal = signal;
    condVar.notify_all();
}

void Model::updateInstIndex() {
    currentIndex =
        disasm.instIndexAt(vm.instructionPointerOffset()).value_or(0);
}

void Model::setScroll(size_t index) {
    if (!scrollCallback) {
        return;
    }
    scrollCallback(index);
    if (refreshCallback) {
        refreshCallback();
    }
}

void Model::refreshScreen(SoftOrForce mode) {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto dur = now - lastRefresh;
    if (duration_cast<milliseconds>(dur).count() < 60 && mode != FORCE) {
        return;
    }
    lastRefresh = now;
    if (refreshCallback) {
        refreshCallback();
    }
}
