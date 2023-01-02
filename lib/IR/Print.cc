#include "IR/Print.h"

#include <iostream>
#include <sstream>

#include "Basic/Basic.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "Basic/PrintUtil.h"

using namespace scatha;
using namespace ir;

namespace {

struct PrintCtx {
    explicit PrintCtx(std::ostream& str): str(str), indent(2) {}

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
    Indenter indent;
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

std::ostream& ir::operator<<(std::ostream& ostream, Instruction const& inst) {
    PrintCtx ctx(ostream);
    ctx.dispatch(inst);
    return ostream;
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
    indent.increase();
    for (auto& bb: function.basicBlocks()) {
        dispatch(bb);
    }
    indent.decrease();
    str << "}\n";
}

void PrintCtx::print(BasicBlock const& bb) {
    str << indent << "%" << bb.name() << ":\n";
    indent.increase();
    for (auto& instruction: bb.instructions) {
        dispatch(instruction);
        str << "\n";
    }
    indent.decrease();
}

void PrintCtx::print(Instruction const& inst) {
    str << indent << "<<" << inst.nodeType() << ">>\n";
}

void PrintCtx::print(Alloca const& alloc) {
    str << indent << "%" << alloc.name() << " = alloca " << alloc.allocatedType()->name();
}

void PrintCtx::print(Load const& load) {
    str << indent << "%" << load.name() << " = load " << load.type()->name() << ", ptr %" << load.operand()->name();
}

void PrintCtx::print(Store const& store) {
    str << indent << "store ";
    str << store.lhs()->type()->name() << " " << toString(*store.lhs()) << ", ";
    str << store.rhs()->type()->name() << " " << toString(*store.rhs());
}

void PrintCtx::print(CompareInst const& cmp) {
    str << indent << "%" << cmp.name() << " = cmp " << cmp.operation() << " ";
    str << cmp.lhs()->type()->name() << " " << toString(*cmp.lhs()) << ", ";
    str << cmp.rhs()->type()->name() << " " << toString(*cmp.rhs());
}

void PrintCtx::print(ArithmeticInst const& arith) {
    str << indent << "%" << arith.name() << " = " << arith.operation() << " " << arith.lhs()->type()->name() << " ";
    str << toString(*arith.lhs()) << ", ";
    str << toString(*arith.rhs());
}

void PrintCtx::print(Goto const& gt) {
    str << indent << "goto " << "label %" << gt.target()->name();
}

void PrintCtx::print(Branch const& br) {
    str << indent
        << "branch " << br.condition()->type()->name() << " " << toString(*br.condition()) << ", ";
    str << "label %" << br.thenTarget()->name() << ", ";
    str << "label %" << br.elseTarget()->name();
}

void PrintCtx::print(Return const& ret) {
    str << indent << "return ";
    if (ret.value()) {
        str << ret.value()->type()->name() << " " << toString(*ret.value());
    }
}

void PrintCtx::print(FunctionCall const& call) {
    str << indent;
    if (!call.name().empty()) {
        str << "%" << call.name() << " = ";
    }
    str << "call " << call.type()->name() << ", @" << call.function()->name();
    for (auto& arg: call.arguments()) {
        str << ", " << arg->type()->name() << " " << toString(*arg);
    }
}

void PrintCtx::print(Phi const& phi) {
    str << indent << "%" << phi.name() << " = phi " << phi.type()->name();
    for (auto& [pred, value]: phi.arguments) {
        str << ", [label %" << pred->name() << ", " << toString(*value) << "]";
    }
}

std::string PrintCtx::toString(Value const& value) {
    // clang-format off
    return visit(value, utl::overload{
        [&](Value const& value) -> std::string { return utl::strcat("%", value.name()); },
        [&](IntegralConstant const& value) -> std::string { return value.value().toString(); },
    });
    // clang-format on
}
