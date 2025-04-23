#include "Model/BreakpointManager.h"

#include <atomic>

#include <scbinutil/OpCode.h>
#include <scbinutil/ProgramView.h>
#include <svm/VirtualMachine.h>

#include "Model/Events.h"

using namespace sdb;

void BreakpointPatcher::pushBreakpoint(scdis::InstructionPointerOffset ipo,
                                       bool value) {
    stackMap[ipo].push(value);
    if (value)
        addBreakpoint(ipo);
    else
        removeBreakpoint(ipo);
}

void BreakpointPatcher::popBreakpoint(scdis::InstructionPointerOffset ipo) {
    auto itr = stackMap.find(ipo);
    assert(itr != stackMap.end() && "Have no breakpoint pushed");
    auto& stack = itr->second;
    stack.pop();
    bool haveBreakpoint = !stack.empty() && stack.top();
    if (haveBreakpoint)
        addBreakpoint(ipo);
    else
        removeBreakpoint(ipo);
    if (stack.empty()) stackMap.erase(itr);
}

void BreakpointPatcher::removeAll() {
    insertQueue.clear();
    for (auto& [ipo, stack]: stackMap)
        if (stack.top()) removalQueue.insert(ipo);
    stackMap.clear();
}

void BreakpointPatcher::patchInstructionStream(uint8_t* bin) {
    auto store = [=](scdis::InstructionPointerOffset ipo, uint8_t value) {
        bin[ipo.value] = value;
    };
    for (auto ipo: insertQueue)
        store(ipo, (uint8_t)scbinutil::OpCode::trap);
    for (auto ipo: removalQueue)
        store(ipo, binary[ipo.value]);
    insertQueue.clear();
    removalQueue.clear();
}

void BreakpointPatcher::addBreakpoint(scdis::InstructionPointerOffset ipo) {
    insertQueue.insert(ipo);
    removalQueue.erase(ipo);
}

void BreakpointPatcher::removeBreakpoint(scdis::InstructionPointerOffset ipo) {
    insertQueue.erase(ipo);
    removalQueue.insert(ipo);
}

void BreakpointPatcher::setProgramData(std::span<uint8_t const> progData) {
    scbinutil::ProgramView program(progData.data());
    binary.assign(program.binary.begin(), program.binary.end());
}

BreakpointManager::BreakpointManager(std::shared_ptr<Messenger> messenger,
                                     scdis::IpoIndexMap const& ipoMap,
                                     SourceDebugInfo const& debug):
    Transceiver(std::move(messenger)),
    ipoIndexMap(ipoMap),
    sourceDebugInfo(debug) {
    listen([this](WillBeginExecution event) {
        patcher.removeAll();
        for (auto instIndex: instBreakpointSet) {
            auto ipo = this->ipoIndexMap.indexToIpo(instIndex);
            patcher.pushBreakpoint(ipo, true);
        }
        for (auto sourceLine: sourceLineBreakpointSet) {
            auto ipos = sourceDebugInfo.sourceMap().toIpos(sourceLine);
            assert(!ipos.empty() &&
                   "Shouldn't have added this to the breakpoint set");
            patcher.pushBreakpoint(ipos.front(), true);
        }
        patcher.patchInstructionStream(event.vm.getBinaryPointer());
    });
    listen([this](WillStepInstruction event) {
        patcher.pushBreakpoint(event.ipo, false);
        patcher.patchInstructionStream(event.vm.getBinaryPointer());
    });
    listen([this](DidStepInstruction event) {
        patcher.popBreakpoint(event.ipo);
        patcher.patchInstructionStream(event.vm.getBinaryPointer());
    });
    listen([this](WillStepSourceLine event) {
        steppingBreakpoints.clear();
        auto* function = sourceDebugInfo.findFunction(event.ipo);
        if (!function) return;
        auto beginIndex = ipoIndexMap.ipoToIndex(function->begin);
        if (!beginIndex) return;
        auto startLoc = sourceDebugInfo.sourceMap().toSourceLoc(event.ipo);
        auto startLine = [&]() -> std::optional<SourceLine> {
            if (!startLoc) return std::nullopt;
            return startLoc->line;
        }();
        for (size_t index = *beginIndex; ipoIndexMap.isIndexValid(index);
             ++index)
        {
            auto ipo = ipoIndexMap.indexToIpo(index);
            if (ipo >= function->end) break;
            bool isReturn = patcher.opcodeAt(ipo) == scbinutil::OpCode::ret;
            auto sourceLoc = sourceDebugInfo.sourceMap().toSourceLoc(ipo);
            if (isReturn || (sourceLoc && sourceLoc->line != startLine)) {
                steppingBreakpoints.push_back(ipo);
                patcher.pushBreakpoint(ipo, true);
            }
        }
        patcher.patchInstructionStream(event.vm.getBinaryPointer());
    });
    listen([this](DidStepSourceLine event) {
        for (auto ipo: steppingBreakpoints)
            patcher.popBreakpoint(ipo);
        steppingBreakpoints.clear();
        patcher.patchInstructionStream(event.vm.getBinaryPointer());
        *event.isReturn = patcher.opcodeAt(event.ipo) == scbinutil::OpCode::ret;
    });
    listen([this](WillStepOut event) {
        auto frame = event.vm.getCurrentExecFrame();
        if (frame.regPtr - frame.bottomReg < 3) return;
        auto* retAddr = reinterpret_cast<uint8_t*>(frame.regPtr[-1]);
        stepOutStackPtr = frame.regPtr[-3];
        scdis::InstructionPointerOffset dest(
            utl::narrow_cast<uint64_t>(retAddr - event.vm.getBinaryPointer()));
        patcher.pushBreakpoint(dest, true);
        patcher.patchInstructionStream(event.vm.getBinaryPointer());
    });
    listen([this](DidStepOut event) {
        auto frame = event.vm.getCurrentExecFrame();
        uint64_t stackPtr = std::bit_cast<uint64_t>(frame.stackPtr);
        if (stepOutStackPtr >= stackPtr) {
            *event.isDone = false;
            return;
        }
        *event.isDone = true;
        patcher.popBreakpoint(event.ipo);
        patcher.patchInstructionStream(event.vm.getBinaryPointer());
    });
}

static bool isExecIdle(Transceiver const& emitter) {
    bool isIdle = true;
    emitter.send_now(IsExecIdle{ &isIdle });
    return isIdle;
}

void BreakpointManager::toggleInstBreakpoint(size_t instIndex) {
    auto [itr, success] = instBreakpointSet.insert(instIndex);
    if (!success) instBreakpointSet.erase(itr);
    if (isExecIdle(*this)) return;
    auto ipo = ipoIndexMap.indexToIpo(instIndex);
    if (success)
        patcher.pushBreakpoint(ipo, true);
    else
        patcher.popBreakpoint(ipo);
    install();
}

bool BreakpointManager::hasInstBreakpoint(size_t instIndex) const {
    return instBreakpointSet.contains(instIndex);
}

bool BreakpointManager::toggleSourceLineBreakpoint(SourceLine line) {
    auto ipos = sourceDebugInfo.sourceMap().toIpos(line);
    if (ipos.empty()) return false;
    auto [itr, success] = sourceLineBreakpointSet.insert(line);
    if (!success) sourceLineBreakpointSet.erase(itr);
    if (isExecIdle(*this)) return true;
    if (success)
        patcher.pushBreakpoint(ipos.front(), true);
    else
        patcher.popBreakpoint(ipos.front());
    install();
    return true;
}

bool BreakpointManager::hasSourceLineBreakpoint(SourceLine line) const {
    return sourceLineBreakpointSet.contains(line);
}

void BreakpointManager::clearAll() {
    instBreakpointSet.clear();
    sourceLineBreakpointSet.clear();
    if (!isExecIdle(*this)) {
        patcher.removeAll();
        install();
    }
}

void BreakpointManager::setProgramData(std::span<uint8_t const> progData) {
    clearAll();
    patcher.setProgramData(progData);
}

void BreakpointManager::install() {
    send_now(DoInterruptedOnVM{ [this](svm::VirtualMachine& vm) {
        patcher.patchInstructionStream(vm.getBinaryPointer());
    } });
}
