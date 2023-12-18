#include "MIR/Clone.h"

#include "MIR/Instructions.h"

using namespace scatha;
using namespace mir;

static StoreInst* doClone(StoreInst& inst) {
    return new StoreInst(inst.address(),
                         inst.source(),
                         inst.bytewidth(),
                         inst.metadata());
}

static LoadInst* doClone(LoadInst& inst) {
    return new LoadInst(inst.dest(),
                        inst.address(),
                        inst.bytewidth(),
                        inst.metadata());
}

static CopyInst* doClone(CopyInst& inst) {
    return new CopyInst(inst.dest(),
                        inst.source(),
                        inst.bytewidth(),
                        inst.metadata());
}

static CallInst* doClone(CallInst& inst) {
    return new CallInst(inst.dest(),
                        inst.numDests(),
                        inst.callee(),
                        inst.arguments() | ToSmallVector<>,
                        inst.metadata());
}

static CondCopyInst* doClone(CondCopyInst& inst) {
    return new CondCopyInst(inst.dest(),
                            inst.source(),
                            inst.bytewidth(),
                            inst.condition(),
                            inst.metadata());
}

static LISPInst* doClone(LISPInst& inst) {
    return new LISPInst(inst.dest(), inst.allocSize(), inst.metadata());
}

static LEAInst* doClone(LEAInst& inst) {
    return new LEAInst(inst.dest(), inst.address(), inst.metadata());
}

static CompareInst* doClone(CompareInst& inst) {
    return new CompareInst(inst.LHS(),
                           inst.RHS(),
                           inst.bytewidth(),
                           inst.mode(),
                           inst.metadata());
}

static TestInst* doClone(TestInst& inst) {
    return new TestInst(inst.operand(),
                        inst.bytewidth(),
                        inst.mode(),
                        inst.metadata());
}

static SetInst* doClone(SetInst& inst) {
    return new SetInst(inst.dest(), inst.operation(), inst.metadata());
}

static UnaryArithmeticInst* doClone(UnaryArithmeticInst& inst) {
    return new UnaryArithmeticInst(inst.dest(),
                                   inst.operand(),
                                   inst.bytewidth(),
                                   inst.operation(),
                                   inst.metadata());
}

static ValueArithmeticInst* doClone(ValueArithmeticInst& inst) {
    return new ValueArithmeticInst(inst.dest(),
                                   inst.LHS(),
                                   inst.RHS(),
                                   inst.bytewidth(),
                                   inst.operation(),
                                   inst.metadata());
}

static LoadArithmeticInst* doClone(LoadArithmeticInst& inst) {
    return new LoadArithmeticInst(inst.dest(),
                                  inst.LHS(),
                                  inst.RHS(),
                                  inst.bytewidth(),
                                  inst.operation(),
                                  inst.metadata());
}

static ConversionInst* doClone(ConversionInst& inst) {
    return new ConversionInst(inst.dest(),
                              inst.operand(),
                              inst.conversion(),
                              inst.fromBits(),
                              inst.toBits(),
                              inst.metadata());
}

static JumpInst* doClone(JumpInst& inst) {
    return new JumpInst(inst.target(), inst.metadata());
}

static CondJumpInst* doClone(CondJumpInst& inst) {
    return new CondJumpInst(inst.target(), inst.condition(), inst.metadata());
}

static ReturnInst* doClone(ReturnInst& inst) {
    return new ReturnInst(inst.operands() | ToSmallVector<>, inst.metadata());
}

static PhiInst* doClone(PhiInst& inst) {
    return new PhiInst(inst.dest(),
                       inst.operands() | ToSmallVector<>,
                       inst.bytewidth(),
                       inst.metadata());
}

static SelectInst* doClone(SelectInst& inst) {
    return new SelectInst(inst.dest(),
                          inst.thenValue(),
                          inst.elseValue(),
                          inst.condition(),
                          inst.bytewidth(),
                          inst.metadata());
}

UniquePtr<Instruction> mir::cloneImpl(Instruction& inst) {
    return visit(inst, []<typename Inst>(Inst& inst) {
        return UniquePtr<Instruction>(doClone(inst));
    });
}
