#include "IR/Print.h"

#include <iostream>
#include <sstream>

#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

#include "Basic/Basic.h"
#include "Basic/PrintUtil.h"
#include "IR/CFG.h"
#include "IR/Module.h"

using namespace scatha;
using namespace ir;

namespace {

struct PrintCtx {
    explicit PrintCtx(std::ostream& str): str(str), indent(2) {}

    void dispatch(Value const&);

    void print(Value const&) { SC_UNREACHABLE(); }
    void print(Function const&);
    void print(BasicBlock const&);
    void print(Instruction const&);
    void print(Alloca const&);
    void print(Load const&);
    void print(Store const&);
    void print(CompareInst const&);
    void print(UnaryArithmeticInst const&);
    void print(ArithmeticInst const&);
    void print(Goto const&);
    void print(Branch const&);
    void print(Return const&);
    void print(FunctionCall const&);
    void print(ExtFunctionCall const&);
    void print(Phi const&);
    void print(GetElementPointer const&);
    void print(ExtractValue const&);
    void print(InsertValue const&);

    void print(StructureType const& structure);

    std::ostream& str;
    Indenter indent;
};

} // namespace

void ir::print(Module const& program) {
    ir::print(program, std::cout);
}

void ir::print(Module const& program, std::ostream& str) {
    PrintCtx ctx(str);
    for (auto& structure: program.structures()) {
        ctx.print(*structure);
    }
    for (auto& function: program.functions()) {
        ctx.dispatch(function);
    }
}

void ir::print(Function const& function) {
    ir::print(function, std::cout);
}

void ir::print(Function const& function, std::ostream& str) {
    PrintCtx ctx(str);
    ctx.dispatch(function);
}

std::ostream& ir::operator<<(std::ostream& ostream, Instruction const& inst) {
    PrintCtx ctx(ostream);
    ctx.dispatch(inst);
    return ostream;
}

std::string ir::toString(Value const& value) {
    // clang-format off
    return visit(value, utl::overload{
        [&](Value const& value) { return utl::strcat("%", value.name()); },
        [&](IntegralConstant const& value) {
            return utl::strcat("$", value.value().toString());
        },
        [&](FloatingPointConstant const& value) {
            return utl::strcat("$", value.value().toString());
        },
    }); // clang-format on
}

void PrintCtx::dispatch(Value const& value) {
    visit(value, [this](auto const& value) { print(value); });
}

static auto keyword(std::string_view name) {
    return tfmt::format(tfmt::magenta | tfmt::bold, name);
}

static auto tertiary(auto... name) {
    return tfmt::format(tfmt::brightGrey, name...);
}

static auto instruction(auto... name) {
    return tfmt::format(tfmt::brightRed | tfmt::bold, name...);
}

static auto formatType(ir::Type const* type) {
    if (!type) {
        return tfmt::format(tfmt::brightBlue | tfmt::italic,
                            std::string("null-type"));
    }
    if (type->category() == ir::TypeCategory::StructureType) {
        return tfmt::format(tfmt::green, utl::strcat("%", type->name()));
    }
    return tfmt::format(tfmt::brightBlue, std::string(type->name()));
}

static auto formatName(Value const& value) {
    // clang-format off
    return visit(value, utl::overload{
        [](ir::Function const& function) {
            return tfmt::format(tfmt::italic,
                                "@",
                                std::string(function.name()));
        },
        [](ir::Parameter const& parameter) {
            return tfmt::format(tfmt::none, "%", std::string(parameter.name()));
        },
        [](ir::BasicBlock const& basicBlock) {
            return tfmt::format(tfmt::italic,
                                "%",
                                std::string(basicBlock.name()));
        },
        [](ir::Instruction const& inst) {
            return tfmt::format(tfmt::none, "%", std::string(inst.name()));
        },
        [](ir::IntegralConstant const& value) {
            return tfmt::format(tfmt::cyan, "$", value.value().toString());
        },
        [](ir::FloatingPointConstant const& value) {
            return tfmt::format(tfmt::cyan, "$", value.value().toString());
        },
        [](ir::UndefValue const& value) {
            /// Really annoying that I have to format it like this.
            return tfmt::format(tfmt::magenta, "u", std::string("ndef"));
        },
        [](ir::Value const&) {
            return tfmt::format(tfmt::bgMagenta, "?", std::string("??"));
        },
    }); // clang-format on
}

static auto equals() {
    return utl::streammanip([](std::ostream& str) -> std::ostream& {
        return str << " " << tfmt::format(tfmt::none, "=") << " ";
    });
}

static auto label() {
    return tertiary("label");
}

void PrintCtx::print(Function const& function) {
    str << keyword("function") << " " << formatType(function.returnType())
        << " " << formatName(function) << "(";
    for (bool first = true; auto& param: function.parameters()) {
        str << (first ? (void)(first = false), "" : ", ")
            << formatType(param.type()) << " " << formatName(param);
    }
    str << ") {\n";
    indent.increase();
    for (auto& bb: function) {
        dispatch(bb);
    }
    indent.decrease();
    str << "}\n";
}

void PrintCtx::print(BasicBlock const& bb) {
    str << indent << formatName(bb) << ":\n";
    indent.increase();
    for (auto& instruction: bb) {
        dispatch(instruction);
        str << "\n";
    }
    indent.decrease();
}

void PrintCtx::print(Instruction const& inst) {
    str << indent << "<<" << inst.nodeType() << ">>\n";
}

void PrintCtx::print(Alloca const& alloc) {
    str << indent << formatName(alloc) << equals() << instruction("alloca")
        << " " << formatType(alloc.allocatedType());
}

void PrintCtx::print(Load const& load) {
    str << indent << formatName(load) << equals() << instruction("load") << " "
        << formatType(load.type()) << " " << formatName(*load.operand());
}

void PrintCtx::print(Store const& store) {
    str << indent << instruction("store") << " ";
    str << formatName(*store.lhs()) << ", ";
    str << formatName(*store.rhs());
}

void PrintCtx::print(CompareInst const& cmp) {
    str << indent << formatName(cmp) << equals()
        << instruction("cmp ", cmp.operation()) << " ";
    str << formatType(cmp.lhs()->type()) << " " << formatName(*cmp.lhs())
        << ", ";
    str << formatType(cmp.rhs()->type()) << " " << formatName(*cmp.rhs());
}

void PrintCtx::print(UnaryArithmeticInst const& inst) {
    str << indent << formatName(inst) << equals()
        << instruction(inst.operation()) << " "
        << formatType(inst.operand()->type()) << " ";
    str << formatName(*inst.operand());
}

void PrintCtx::print(ArithmeticInst const& inst) {
    str << indent << formatName(inst) << equals()
        << instruction(inst.operation()) << " "
        << formatType(inst.lhs()->type()) << " ";
    str << formatName(*inst.lhs()) << ", ";
    str << formatName(*inst.rhs());
}

void PrintCtx::print(Goto const& gt) {
    str << indent << instruction("goto") << " " << label() << " "
        << formatName(*gt.target());
}

void PrintCtx::print(Branch const& br) {
    str << indent << instruction("branch") << " "
        << formatType(br.condition()->type()) << " "
        << formatName(*br.condition()) << ", ";
    str << label() << " " << formatName(*br.thenTarget()) << ", ";
    str << label() << " " << formatName(*br.elseTarget());
}

void PrintCtx::print(Return const& ret) {
    str << indent << instruction("return") << " ";
    if (ret.value()) {
        str << formatType(ret.value()->type()) << " "
            << formatName(*ret.value());
    }
}

void PrintCtx::print(FunctionCall const& call) {
    str << indent;
    if (!call.name().empty()) {
        str << formatName(call) << equals();
    }
    str << instruction("call") << " " << formatType(call.type()) << " "
        << formatName(*call.function());
    for (auto& arg: call.arguments()) {
        str << ", " << formatType(arg->type()) << " " << formatName(*arg);
    }
}

void PrintCtx::print(ExtFunctionCall const& call) {
    str << indent;
    if (!call.name().empty()) {
        str << formatName(call) << equals();
    }
    str << instruction("call") << " " << formatType(call.type()) << " "
        << call.functionName();
    for (auto& arg: call.arguments()) {
        str << ", " << formatType(arg->type()) << " " << formatName(*arg);
    }
}

void PrintCtx::print(Phi const& phi) {
    str << indent << formatName(phi) << equals() << instruction("phi") << " "
        << formatType(phi.type()) << " ";
    for (bool first = true; auto const [pred, value]: phi.arguments()) {
        str << (first ? first = false, "" : ", ");
        str << "[" << label() << " " << formatName(*pred) << " : "
            << formatName(*value) << "]";
    }
}

void PrintCtx::print(GetElementPointer const& gep) {
    str << indent << formatName(gep) << equals() << instruction("gep") << " "
        << formatType(gep.type()) << ", "
        << formatType(gep.basePointer()->type()) << " "
        << formatName(*gep.basePointer()) << ", "
        << formatType(gep.arrayIndex()->type()) << " "
        << formatName(*gep.arrayIndex()) << ", "
        << formatType(gep.structMemberIndex()->type()) << " "
        << formatName(*gep.structMemberIndex());
}

void PrintCtx::print(ExtractValue const& extract) {
    str << indent << formatName(extract) << equals()
        << instruction("extract_value") << " " << formatType(extract.type())
        << ", " << formatType(extract.baseValue()->type()) << " "
        << formatName(*extract.baseValue()) << ", "
        << formatType(extract.index()->type()) << " "
        << formatName(*extract.index());
}

void PrintCtx::print(InsertValue const& insert) {
    str << indent << formatName(insert) << equals()
        << instruction("insert_value") << " " << formatType(insert.type())
        << ", " << formatType(insert.baseValue()->type()) << " "
        << formatName(*insert.baseValue()) << ", "
        << formatType(insert.insertedValue()->type()) << " "
        << formatName(*insert.insertedValue()) << " "
        << formatType(insert.index()->type()) << " "
        << formatName(*insert.index());
}

void PrintCtx::print(StructureType const& structure) {
    str << keyword("structure") << " " << formatType(&structure) << " {\n";
    indent.increase();
    for (bool first = true; auto const* type: structure.members()) {
        str << (first ? (first = false), "" : ",\n") << indent;
        str << formatType(type);
    }
    indent.decrease();
    str << "\n}\n";
}
