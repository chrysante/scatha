#include "MIR/Clone.h"

#include "MIR/Instructions.h"

using namespace scatha;
using namespace mir;

static StoreInst* doClone(StoreInst& inst) {
    return new StoreInst(inst.address(), inst.source(), inst.bytewidth(),
                         inst.cloneMetadata());
}

static LoadInst* doClone(LoadInst& inst) {
    return new LoadInst(inst.dest(), inst.address(), inst.bytewidth(),
                        inst.cloneMetadata());
}

static CopyInst* doClone(CopyInst& inst) {
    return new CopyInst(inst.dest(), inst.source(), inst.bytewidth(),
                        inst.cloneMetadata());
}

static CallInst* doClone(CallValueInst& inst) {
    return new CallValueInst(inst.dest(), inst.numDests(), inst.callee(),
                             inst.arguments() | ToSmallVector<>,
                             inst.cloneMetadata());
}

static CallInst* doClone(CallMemoryInst& inst) {
    return new CallMemoryInst(inst.dest(), inst.numDests(), inst.callee(),
                              inst.arguments() | ToSmallVector<>,
                              inst.cloneMetadata());
}

static CondCopyInst* doClone(CondCopyInst& inst) {
    return new CondCopyInst(inst.dest(), inst.source(), inst.bytewidth(),
                            inst.condition(), inst.cloneMetadata());
}

static LISPInst* doClone(LISPInst& inst) {
    return new LISPInst(inst.dest(), inst.allocSize(), inst.cloneMetadata());
}

static LEAInst* doClone(LEAInst& inst) {
    return new LEAInst(inst.dest(), inst.address(), inst.cloneMetadata());
}

static CompareInst* doClone(CompareInst& inst) {
    return new CompareInst(inst.LHS(), inst.RHS(), inst.bytewidth(),
                           inst.mode(), inst.cloneMetadata());
}

static TestInst* doClone(TestInst& inst) {
    return new TestInst(inst.operand(), inst.bytewidth(), inst.mode(),
                        inst.cloneMetadata());
}

static SetInst* doClone(SetInst& inst) {
    return new SetInst(inst.dest(), inst.operation(), inst.cloneMetadata());
}

static UnaryArithmeticInst* doClone(UnaryArithmeticInst& inst) {
    return new UnaryArithmeticInst(inst.dest(), inst.operand(),
                                   inst.bytewidth(), inst.operation(),
                                   inst.cloneMetadata());
}

static ValueArithmeticInst* doClone(ValueArithmeticInst& inst) {
    return new ValueArithmeticInst(inst.dest(), inst.LHS(), inst.RHS(),
                                   inst.bytewidth(), inst.operation(),
                                   inst.cloneMetadata());
}

static LoadArithmeticInst* doClone(LoadArithmeticInst& inst) {
    return new LoadArithmeticInst(inst.dest(), inst.LHS(), inst.RHS(),
                                  inst.bytewidth(), inst.operation(),
                                  inst.cloneMetadata());
}

static ConversionInst* doClone(ConversionInst& inst) {
    return new ConversionInst(inst.dest(), inst.operand(), inst.conversion(),
                              inst.fromBits(), inst.toBits(),
                              inst.cloneMetadata());
}

static JumpInst* doClone(JumpInst& inst) {
    return new JumpInst(inst.target(), inst.cloneMetadata());
}

static CondJumpInst* doClone(CondJumpInst& inst) {
    return new CondJumpInst(inst.target(), inst.condition(),
                            inst.cloneMetadata());
}

static ReturnInst* doClone(ReturnInst& inst) {
    return new ReturnInst(inst.operands() | ToSmallVector<>,
                          inst.cloneMetadata());
}

static PhiInst* doClone(PhiInst& inst) {
    return new PhiInst(inst.dest(), inst.operands() | ToSmallVector<>,
                       inst.bytewidth(), inst.cloneMetadata());
}

static SelectInst* doClone(SelectInst& inst) {
    return new SelectInst(inst.dest(), inst.thenValue(), inst.elseValue(),
                          inst.condition(), inst.bytewidth(),
                          inst.cloneMetadata());
}

UniquePtr<Instruction> mir::cloneImpl(Instruction& inst) {
    return visit(inst, []<typename Inst>(Inst& inst) {
        return UniquePtr<Instruction>(doClone(inst));
    });
}
