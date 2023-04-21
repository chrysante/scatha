#include "IR/Print.h"

#include <iostream>
#include <sstream>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Common/Base.h"
#include "Common/PrintUtil.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Type.h"

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
    void print(ConversionInst const&);
    void print(CompareInst const&);
    void print(UnaryArithmeticInst const&);
    void print(ArithmeticInst const&);
    void print(Goto const&);
    void print(Branch const&);
    void print(Return const&);
    void print(Call const&);
    void print(Phi const&);
    void print(GetElementPointer const&);
    void print(ExtractValue const&);
    void print(InsertValue const&);
    void print(Select const&);

    void print(StructureType const& structure);

    void instDecl(Instruction const*) const;
    void name(Value const*) const;
    void type(Type const*) const;
    void typedName(Value const*) const;
    void keyword(auto...) const;
    void space() const { str << " "; }
    void comma() const { str << ", "; }
    void to() const;
    void indexList(auto list) const;

    std::ostream& str;
    Indenter indent;
};

} // namespace

void ir::print(Module const& program) { ir::print(program, std::cout); }

void ir::print(Module const& program, std::ostream& str) {
    PrintCtx ctx(str);
    for (auto& structure: program.structures()) {
        ctx.print(*structure);
    }
    for (auto& function: program) {
        ctx.dispatch(function);
    }
}

void ir::print(Function const& function) { ir::print(function, std::cout); }

void ir::print(Function const& function, std::ostream& str) {
    PrintCtx ctx(str);
    ctx.dispatch(function);
}

std::ostream& ir::operator<<(std::ostream& ostream, Instruction const& inst) {
    PrintCtx ctx(ostream);
    ctx.dispatch(inst);
    return ostream;
}

std::string ir::toString(Value const& value) { return toString(&value); }

std::string ir::toString(Value const* value) {
    if (!value) {
        return "<null-value>";
    }
    // clang-format off
    return visit(*value, utl::overload{
        [&](Function const& func) { return utl::strcat("@", func.name()); },
        [&](Value const& value) { return utl::strcat("%", value.name()); },
        [&](IntegralConstant const& value) {
            return value.value().toString();
        },
        [&](FloatingPointConstant const& value) {
            return value.value().toString();
        },
        [&](UndefValue const&) {
            return std::string("undef");
        },
    }); // clang-format on
}

void PrintCtx::dispatch(Value const& value) {
    if (auto* inst = dyncast<Instruction const*>(&value)) {
        instDecl(inst);
    }
    visit(value, [this](auto const& value) { print(value); });
}

static auto formatKeyword(auto... name) {
    return tfmt::format(tfmt::magenta | tfmt::bold, name...);
}

static auto tertiary(auto... name) {
    return tfmt::format(tfmt::brightGrey, name...);
}

static auto formatInstName(auto... name) { return formatKeyword(name...); }

static tfmt::VObjectWrapper formatType(ir::Type const* type) {
    if (!type) {
        return tfmt::format(tfmt::brightBlue | tfmt::italic, "null-type");
    }
    if (type->category() == ir::TypeCategory::StructureType) {
        return tfmt::format(tfmt::green, "@", type->name());
    }
    return tfmt::format(tfmt::brightBlue, type->name());
}

static auto formatNumLiteral(auto... value) {
    return tfmt::format(tfmt::cyan, value...);
}

static tfmt::VObjectWrapper formatName(Value const* value) {
    if (!value) {
        return tfmt::format(tfmt::brightWhite | tfmt::bgBrightRed | tfmt::bold,
                            "<NULL>");
    }
    // clang-format off
    return visit(*value, utl::overload{
        [](ir::Callable const& function) -> tfmt::VObjectWrapper {
            return tfmt::format(tfmt::italic | tfmt::green,
                                "@", function.name());
        },
        [](ir::Parameter const& parameter) -> tfmt::VObjectWrapper {
            return tfmt::format(tfmt::none, "%", parameter.name());
        },
        [](ir::BasicBlock const& basicBlock) -> tfmt::VObjectWrapper {
            return tfmt::format(tfmt::italic,
                                "%", basicBlock.name());
        },
        [](ir::Instruction const& inst) -> tfmt::VObjectWrapper {
            return tfmt::format(tfmt::none, "%", inst.name());
        },
        [](ir::IntegralConstant const& value) -> tfmt::VObjectWrapper {
            return formatNumLiteral(value.value().toString());
        },
        [](ir::FloatingPointConstant const& value) -> tfmt::VObjectWrapper {
            return formatNumLiteral(value.value().toString());
        },
        [](ir::UndefValue const& value) -> tfmt::VObjectWrapper {
            return formatKeyword("undef");
        },
        [](ir::Value const&) -> tfmt::VObjectWrapper {
            return tfmt::format(tfmt::bgMagenta, "???");
        },
    }); // clang-format on
}

static auto equals() {
    return utl::streammanip([](std::ostream& str) -> std::ostream& {
        return str << " " << tfmt::format(tfmt::none, "=") << " ";
    });
}

static auto label() { return tertiary("label"); }

void PrintCtx::print(Function const& function) {
    str << formatKeyword("func") << " " << formatType(function.returnType())
        << " " << formatName(&function) << "(";
    for (bool first = true; auto& param: function.parameters()) {
        str << (first ? (void)(first = false), "" : ", ")
            << formatType(param.type()) << " " << formatName(&param);
    }
    str << ") {\n";
    indent.increase();
    for (auto& bb: function) {
        dispatch(bb);
    }
    indent.decrease();
    str << "}\n\n";
}

static ssize_t length(auto const& fmt) {
    std::stringstream sstr;
    sstr << fmt;
    return utl::narrow_cast<ssize_t>(std::move(sstr).str().size());
}

void PrintCtx::print(BasicBlock const& bb) {
    str << indent << formatName(&bb) << ":";
    if (!bb.isEntry()) {
        ssize_t commentIndent = 30;
        ssize_t const currentColumn =
            indent.totalIndent() + length(formatName(&bb)) + 1;
        if (currentColumn >= commentIndent) {
            str << "\n";
        }
        else {
            commentIndent -= currentColumn;
        }
        for (ssize_t i = 0; i < commentIndent; ++i) {
            str << ' ';
        }
        tfmt::pushModifier(tfmt::brightGrey, str);
        str << "# preds: ";
        for (bool first = true; auto* pred: bb.predecessors()) {
            str << (first ? first = false, "" : ", ") << pred->name();
        }
        tfmt::popModifier(str);
    }
    str << "\n";
    indent.increase();
    for (auto& inst: bb) {
        str << indent;
        dispatch(inst);
        str << "\n";
    }
    indent.decrease();
    if (bb.next() != bb.parent()->end().to_address()) {
        str << "\n";
    }
}

void PrintCtx::print(Instruction const& inst) {
    str << "<" << inst.nodeType() << ">\n";
}

void PrintCtx::print(Alloca const& alloc) {
    type(alloc.allocatedType());
    comma();
    typedName(alloc.count());
}

void PrintCtx::print(Load const& load) {
    type(load.type());
    comma();
    typedName(load.address());
}

void PrintCtx::print(Store const& store) {
    typedName(store.address());
    comma();
    typedName(store.value());
}

void PrintCtx::print(ConversionInst const& inst) {
    typedName(inst.operand());
    to();
    type(inst.type());
}

void PrintCtx::print(CompareInst const& cmp) {
    keyword(cmp.operation());
    space();
    typedName(cmp.lhs());
    comma();
    typedName(cmp.rhs());
}

void PrintCtx::print(UnaryArithmeticInst const& inst) {
    typedName(inst.operand());
}

void PrintCtx::print(ArithmeticInst const& inst) {
    typedName(inst.lhs());
    comma();
    typedName(inst.rhs());
}

void PrintCtx::print(Goto const& gt) { typedName(gt.target()); }

void PrintCtx::print(Branch const& br) {
    typedName(br.condition());
    comma();
    typedName(br.thenTarget());
    comma();
    typedName(br.elseTarget());
}

void PrintCtx::print(Return const& ret) {
    if (isa<VoidType>(ret.value()->type())) {
        return;
    }
    typedName(ret.value());
}

void PrintCtx::print(Call const& call) {
    type(call.type());
    space();
    name(call.function());
    for (auto* arg: call.arguments()) {
        comma();
        typedName(arg);
    }
}

void PrintCtx::print(Phi const& phi) {
    type(phi.type());
    space();
    for (bool first = true; auto const [pred, value]: phi.arguments()) {
        str << (first ? first = false, "" : ", ");
        str << "[" << label() << " " << formatName(pred) << " : "
            << formatName(value) << "]";
    }
}

void PrintCtx::print(GetElementPointer const& gep) {
    keyword("inbounds");
    space();
    type(gep.inboundsType());
    comma();
    typedName(gep.basePointer());
    comma();
    typedName(gep.arrayIndex());
    indexList(gep.memberIndices());
}

void PrintCtx::print(ExtractValue const& extract) {
    typedName(extract.baseValue());
    indexList(extract.memberIndices());
}

void PrintCtx::print(InsertValue const& insert) {
    typedName(insert.baseValue());
    comma();
    typedName(insert.insertedValue());
    indexList(insert.memberIndices());
}

void PrintCtx::print(Select const& select) {
    typedName(select.condition());
    comma();
    typedName(select.thenValue());
    comma();
    typedName(select.elseValue());
}

void PrintCtx::print(StructureType const& structure) {
    str << formatKeyword("struct") << " " << formatType(&structure) << " {\n";
    indent.increase();
    for (bool first = true; auto const* type: structure.members()) {
        str << (first ? (first = false), "" : ",\n") << indent;
        str << formatType(type);
    }
    indent.decrease();
    str << "\n}\n\n";
}

static std::string_view toStrName(ir::Instruction const* inst) {
    switch (inst->nodeType()) {
    case NodeType::Alloca:
        return "alloca";
    case NodeType::Load:
        return "load";
    case NodeType::Store:
        return "store";
    case NodeType::ConversionInst:
        return toString(cast<ConversionInst const*>(inst)->conversion());
    case NodeType::CompareInst:
        return toString(cast<CompareInst const*>(inst)->mode());
    case NodeType::UnaryArithmeticInst:
        return toString(cast<UnaryArithmeticInst const*>(inst)->operation());
    case NodeType::ArithmeticInst:
        return toString(cast<ArithmeticInst const*>(inst)->operation());
    case NodeType::Goto:
        return "goto";
    case NodeType::Branch:
        return "branch";
    case NodeType::Return:
        return "return";
    case NodeType::Call:
        return "call";
    case NodeType::Phi:
        return "phi";
    case NodeType::GetElementPointer:
        return "getelementptr";
    case NodeType::ExtractValue:
        return "extract_value";
    case NodeType::InsertValue:
        return "insert_value";
    case NodeType::Select:
        return "select";
    default:
        SC_UNREACHABLE();
    }
}

void PrintCtx::instDecl(Instruction const* inst) const {
    if (!inst->name().empty()) {
        str << formatName(inst) << equals();
    }
    str << formatInstName(toStrName(inst)) << " ";
}

void PrintCtx::name(Value const* value) const { str << formatName(value); }

void PrintCtx::type(Type const* type) const { str << formatType(type); }

void PrintCtx::typedName(Value const* value) const {
    if (!value) {
        str << "<null>";
        return;
    }
    if (isa<BasicBlock>(value)) {
        str << label();
    }
    else {
        type(value->type());
    }
    space();
    name(value);
}

void PrintCtx::keyword(auto... name) const { str << formatKeyword(name...); }

void PrintCtx::to() const { str << " " << formatKeyword("to") << " "; }

void PrintCtx::indexList(auto list) const {
    for (auto index: list) {
        comma();
        str << formatNumLiteral(index);
    }
}
