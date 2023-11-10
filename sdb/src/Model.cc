#include "Model.h"

#include <ostream>

#include "Common.h"

using namespace sdb;

Model::Model(svm::VirtualMachine vm,
             uint8_t const* program,
             std::array<uint64_t, 2> arguments):
    vm(std::move(vm)), disasm(disassemble(program)), arguments(arguments) {}

Model::~Model() {
    {
        std::lock_guard lock(mutex);
        send(Signal::Terminate);
    }
    if (executionThread.joinable()) {
        executionThread.join();
    }
}

void Model::startExecutionThread() {
    signal = Signal::Sleep;
    execThreadRunning = true;
    executionThread = std::thread([this] {
        vm.beginExecution(arguments);
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
                if (vm.running()) {
                    vm.stepExecution();
                }
                execThreadRunning = vm.running();
                std::lock_guard lock(mutex);
                signal = Signal::Sleep;
                currentIndex = disasm.instIndexAt(vm.instructionPointerOffset())
                                   .value_or(0);
                break;
            }
            case Signal::Run: {
                while (vm.running()) {
                    vm.stepExecution();
                    std::lock_guard lock(mutex);
                    if (signal != Signal::Run) {
                        break;
                    }
                    refreshScreen();
                    auto instIndex =
                        disasm.instIndexAt(vm.instructionPointerOffset());
                    if (instIndex && breakpoints.contains(*instIndex)) {
                        signal = Signal::Sleep;
                        currentIndex = *instIndex;
                        break;
                    }
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
    });
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

bool Model::isSleeping() {
    std::lock_guard lock(mutex);
    return signal == Signal::Sleep;
}

void Model::send(Signal signal) {
    this->signal = signal;
    condVar.notify_all();
}

void Model::refreshScreen() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto dur = now - lastRefresh;
    if (duration_cast<milliseconds>(dur).count() < 60) {
        return;
    }
    lastRefresh = now;
    if (refreshScreenFn) {
        refreshScreenFn();
    }
}
