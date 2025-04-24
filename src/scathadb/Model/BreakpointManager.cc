#include "Model/BreakpointManager.h"

#include <atomic>

#include <scbinutil/OpCode.h>
#include <scbinutil/ProgramView.h>
#include <svm/VirtualMachine.h>

#include "Model/Events.h"
#include "Model/Executor.h"

using namespace sdb;

BreakpointManager::BreakpointManager(std::shared_ptr<Messenger> messenger,
                                     Executor& exec,
                                     scdis::IpoIndexMap const& ipoMap,
                                     SourceDebugInfo const& debug):
    Transceiver(std::move(messenger)),
    ipoIndexMap(ipoMap),
    sourceDebugInfo(debug),
    executor(exec) {
    listen([this](WillBeginExecution event) {
        for (auto instIndex: instBreakpointSet) {
            auto ipo = this->ipoIndexMap.indexToIpo(instIndex);
            executor.pushBreakpoint(ipo, true);
        }
        for (auto sourceLine: sourceLineBreakpointSet) {
            auto ipos = sourceDebugInfo.sourceMap().toIpos(sourceLine);
            assert(!ipos.empty() &&
                   "Shouldn't have added this to the breakpoint set");
            executor.pushBreakpoint(ipos.front(), true);
        }
        executor.applyBreakpoints(event.vm);
    });
}

void BreakpointManager::toggleInstBreakpoint(size_t instIndex) {
    auto [itr, success] = instBreakpointSet.insert(instIndex);
    if (!success) instBreakpointSet.erase(itr);
    auto ipo = ipoIndexMap.indexToIpo(instIndex);
    if (success)
        executor.pushBreakpoint(ipo, true);
    else
        executor.popBreakpoint(ipo);
    executor.applyBreakpoints();
}

bool BreakpointManager::hasInstBreakpoint(size_t instIndex) const {
    return instBreakpointSet.contains(instIndex);
}

bool BreakpointManager::toggleSourceLineBreakpoint(SourceLine line) {
    auto ipos = sourceDebugInfo.sourceMap().toIpos(line);
    if (ipos.empty()) return false;
    auto [itr, success] = sourceLineBreakpointSet.insert(line);
    if (!success) sourceLineBreakpointSet.erase(itr);
    if (success)
        executor.pushBreakpoint(ipos.front(), true);
    else
        executor.popBreakpoint(ipos.front());
    executor.applyBreakpoints();
    return true;
}

bool BreakpointManager::hasSourceLineBreakpoint(SourceLine line) const {
    return sourceLineBreakpointSet.contains(line);
}

void BreakpointManager::clearAll() {
    for (auto instIndex: instBreakpointSet) {
        auto ipo = ipoIndexMap.indexToIpo(instIndex);
        executor.popBreakpoint(ipo);
    }
    instBreakpointSet.clear();
    for (auto sourceLine: sourceLineBreakpointSet) {
        auto ipo = sourceDebugInfo.sourceMap().toIpos(sourceLine).front();
        executor.popBreakpoint(ipo);
    }
    sourceLineBreakpointSet.clear();
    executor.applyBreakpoints();
}
