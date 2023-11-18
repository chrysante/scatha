#ifndef SDB_MODEL_INSTRUCTIONBREAKPOINT_H_
#define SDB_MODEL_INSTRUCTIONBREAKPOINT_H_

#include "Model/Breakpoint.h"

namespace sdb {

class InstructionBreakpoint: public Breakpoint {
    void onHit() override {}
};

} // namespace sdb

#endif // SDB_MODEL_INSTRUCTIONBREAKPOINT_H_
