#include "IR/Print.h"

#include <iostream>

#include "Basic/Basic.h"
#include "IR/Module.h"
#include "IR/BasicBlock.h"
#include "IR/Instruction.h"
#include "IR/Parameter.h"

using namespace scatha;
using namespace ir;

namespace {

struct PrintCtx {
    explicit PrintCtx(std::ostream& str): str(str) {}
    
    void dispatch(Value const&);
    
    void print(Value const&) { SC_DEBUGFAIL(); }
    void print(Function const&);
    void print(BasicBlock const&);
    void print(Instruction const&);
    void print(Alloca const&);
    void print(Load const&);
    void print(Store const&);
    void print(CompareInst const&);
    void print(ArithmeticInst const&);
    void print(Goto const&);
    void print(Branch const&);
    void print(Return const&);
    void print(FunctionCall const&);
    void print(Phi const&);
    
    std::string toString(Value const&);
    
    std::ostream& str;
};

} // namespace

void ir::print(Module const& program) {
    ir::print(program, std::cout);
}

void ir::print(Module const& program, std::ostream& str) {
    PrintCtx ctx(str);
    
    for (auto& function: program.functions()) {
        ctx.dispatch(function);
    }
}

void PrintCtx::dispatch(Value const& value) {
    visit(value, [this](auto const& value) { print(value); });
}

void PrintCtx::print(Function const& function) {
    str << "define " << function.returnType()->name() << " @" << function.name() << "(";
    for (bool first = true; auto& param: function.parameters()) {
        str << (first ? (void)(first = false), "" : ", ") << param.type()->name() << " %" << param.name();
    }
    str << ") {\n";
    for (auto& bb: function.basicBlocks()) {
        dispatch(bb);
    }
    str << "}\n";
}

void PrintCtx::print(BasicBlock const& bb) {
    str << "  %" << bb.name() << ":\n";
    for (auto& instruction: bb.instructions) {
        dispatch(instruction);
    }
}

void PrintCtx::print(Instruction const& inst) {
    str << "    <<" << inst.nodeType() << ">>\n";
}

void PrintCtx::print(Alloca const& alloc) {
    str << "    %" << alloc.name() << " = alloca " << alloc.allocatedType()->name() << "\n";
}

void PrintCtx::print(Load const& load) {
    str << "    %" << load.name() << " = load " << load.type()->name() << ", ptr %" << load.operand()->name() << "\n";
}

void PrintCtx::print(Store const& store) {
    str << "    " << "store ";
    str << store.lhs()->type()->name() << " " << toString(*store.lhs()) << ", ";
    str << store.rhs()->type()->name() << " " << toString(*store.rhs()) << "\n";
}

void PrintCtx::print(CompareInst const& cmp) {
    str << "    %" << cmp.name() << " = cmp " << cmp.operation() << " ";
    str << cmp.lhs()->type()->name() << " " << toString(*cmp.lhs()) << ", ";
    str << cmp.rhs()->type()->name() << " " << toString(*cmp.rhs()) << "\n";
}

void PrintCtx::print(ArithmeticInst const& arith) {
    str << "    %" << arith.name() << " = " << arith.operation() << " " << arith.lhs()->type()->name() << " ";
    str << toString(*arith.lhs()) << ", ";
    str << toString(*arith.rhs()) << "\n";
}

void PrintCtx::print(Goto const& gt) {
    str << "    " << "goto " << "label %" << gt.target()->name() << "\n";
}

void PrintCtx::print(Branch const& br) {
    str << "    " << "branch " << br.condition()->type()->name() << " " << toString(*br.condition()) << ", ";
    str << "label %" << br.ifTarget()->name() << ", ";
    str << "label %" << br.elseTarget()->name() << "\n";
}

void PrintCtx::print(Return const& ret) {
    str << "    " << "return " << ret.value()->type()->name() << " " << toString(*ret.value()) << "\n";
}

void PrintCtx::print(FunctionCall const& call) {
    str << "    ";
    if (!call.name().empty()) {
        str << "%" << call.name() << " = ";
    }
    str << "call " << call.type()->name();
    for (auto& arg: call.arguments()) {
        str << ", " << arg->type()->name() << " " << toString(*arg);
    }
    str << "\n";
}

void PrintCtx::print(Phi const& phi) {
    str << "    %" << phi.name() << " = phi " << phi.type()->name();
    for (auto& [pred, value]: phi.arguments) {
        str << ", [label %" << pred->name() << ", " << toString(*value) << "]";
    }
    str << "\n";
}

std::string PrintCtx::toString(Value const& value) {
    return visit(value, utl::overload{
        [&](Value const& value) -> std::string { return utl::strcat("%", value.name()); },
        [&](IntegralConstant const& value) -> std::string { return value.value().toString(); },
    });
}
