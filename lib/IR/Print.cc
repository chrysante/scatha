#include "IR/Print.h"

#include <iostream>
#include <sstream>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Common/Base.h"
#include "Common/EscapeSequence.h"
#include "Common/PrintUtil.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

namespace {

struct PrintCtx {
    explicit PrintCtx(std::ostream& str): str(str), indent(2) {}

    void print(Value const&);

    void printImpl(Value const&) { SC_UNREACHABLE(); }
    void printImpl(Function const&);
    void printImpl(ExtFunction const&);
    void printImpl(BasicBlock const&);
    void printImpl(Instruction const&);
    void printImpl(Alloca const&);
    void printImpl(Load const&);
    void printImpl(Store const&);
    void printImpl(ConversionInst const&);
    void printImpl(CompareInst const&);
    void printImpl(UnaryArithmeticInst const&);
    void printImpl(ArithmeticInst const&);
    void printImpl(Goto const&);
    void printImpl(Branch const&);
    void printImpl(Return const&);
    void printImpl(Call const&);
    void printImpl(Phi const&);
    void printImpl(GetElementPointer const&);
    void printImpl(ExtractValue const&);
    void printImpl(InsertValue const&);
    void printImpl(Select const&);

    void print(StructType const& structure);

    void print(ConstantData const& constData);

    void printDataAs(Type const* type, std::span<u8 const> data);

    void funcDecl(ir::Callable const*);
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

void ir::print(Module const& mod) { ir::print(mod, std::cout); }

void ir::print(Module const& mod, std::ostream& str) {
    PrintCtx ctx(str);
    for (auto& structure: mod.structures()) {
        ctx.print(*structure);
    }
    for (auto* constData: mod.constantData()) {
        ctx.print(*constData);
    }
    for (auto* global: mod.globals()) {
        ctx.print(*global);
    }
    for (auto& function: mod) {
        ctx.print(function);
    }
}

void ir::print(Callable const& callable) { ir::print(callable, std::cout); }

void ir::print(Callable const& callable, std::ostream& str) {
    PrintCtx ctx(str);
    visit(callable, [&](auto& function) { ctx.print(function); });
}

void ir::print(Instruction const& inst) { ir::print(inst, std::cout); }

void ir::print(Instruction const& inst, std::ostream& str) {
    str << inst << std::endl;
}

std::ostream& ir::operator<<(std::ostream& str, Instruction const& inst) {
    PrintCtx ctx(str);
    ctx.print(inst);
    return str;
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

void PrintCtx::print(Value const& value) {
    if (auto* inst = dyncast<Instruction const*>(&value)) {
        instDecl(inst);
    }
    visit(value, [this](auto const& value) { printImpl(value); });
}

static auto formatKeyword(auto... name) {
    return tfmt::format(tfmt::Magenta | tfmt::Bold, name...);
}

static auto formatNumLiteral(auto... value) {
    return tfmt::format(tfmt::Cyan, value...);
}

static auto tertiary(auto... name) {
    return tfmt::format(tfmt::BrightGrey, name...);
}

static auto formatInstName(auto... name) { return formatKeyword(name...); }

static utl::vstreammanip<> formatType(ir::Type const* type) {
    return [=](std::ostream& str) {
        if (!type) {
            str << tfmt::format(tfmt::BrightBlue | tfmt::Italic, "null-type");
            return;
        }
        // clang-format off
        visit(*type, utl::overload{
            [&](StructType const& type) {
                if (!type.name().empty()) {
                    str << tfmt::format(tfmt::Green, "@", type.name());
                    return;
                }
                str << "{ ";
                for (bool first = true; auto* member: type.members()) {
                    str << (first ? first = false, "" : ", ")
                        << formatType(member);
                }
                str << " }";
            },
            [&](ArrayType const& type) {
                str << "[" << formatType(type.elementType()) << ", "
                    << formatNumLiteral(type.count()) << "]";
            },
            [&](Type const& type) {
                str << tfmt::format(tfmt::BrightBlue, type.name());
            }
        }); // clang-format on
    };
}

static constexpr utl::streammanip formatName([](std::ostream& str,
                                                Value const* value) {
    if (!value) {
        str << tfmt::format(tfmt::BrightWhite | tfmt::BGBrightRed | tfmt::Bold,
                            "<NULL>");
        return;
    }
    // clang-format off
    visit(*value, utl::overload{
        [&](ir::Constant const& constant) {
            str << tfmt::format(tfmt::Italic | tfmt::Green,
                                "@", constant.name());
        },
        [&](ir::Parameter const& parameter) {
            str << tfmt::format(tfmt::None, "%", parameter.name());
        },
        [&](ir::BasicBlock const& basicBlock) {
            str << tfmt::format(tfmt::Italic,
                                "%", basicBlock.name());
        },
        [&](ir::Instruction const& inst) {
            str << tfmt::format(tfmt::None, "%", inst.name());
        },
        [&](ir::IntegralConstant const& value) {
            str << formatNumLiteral(value.value().toString());
        },
        [&](ir::FloatingPointConstant const& value) {
            str << formatNumLiteral(value.value().toString());
        },
        [&](ir::UndefValue const& value) {
            str << formatKeyword("undef");
        },
        [&](ir::Value const&) {
            str << tfmt::format(tfmt::BGMagenta, "???");
        },
    }); // clang-format on
});

static auto equals() {
    return utl::streammanip([](std::ostream& str) -> std::ostream& {
        return str << " " << tfmt::format(tfmt::None, "=") << " ";
    });
}

static auto label() { return tertiary("label"); }

void PrintCtx::printImpl(Function const& function) {
    funcDecl(&function);
    str << " {\n";
    indent.increase();
    for (auto& bb: function) {
        print(bb);
    }
    indent.decrease();
    str << "}\n\n";
}

void PrintCtx::printImpl(ExtFunction const& function) {
    str << formatKeyword("ext") << " ";
    funcDecl(&function);
    str << "\n\n";
}

static ssize_t length(auto const& fmt) {
    std::stringstream sstr;
    sstr << fmt;
    return utl::narrow_cast<ssize_t>(std::move(sstr).str().size());
}

void PrintCtx::printImpl(BasicBlock const& bb) {
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
        tfmt::pushModifier(tfmt::BrightGrey, str);
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
        print(inst);
        str << "\n";
    }
    indent.decrease();
    if (bb.next() != bb.parent()->end().to_address()) {
        str << "\n";
    }
}

void PrintCtx::printImpl(Instruction const& inst) {
    str << "<" << inst.nodeType() << ">\n";
}

void PrintCtx::printImpl(Alloca const& alloc) {
    type(alloc.allocatedType());
    comma();
    typedName(alloc.count());
}

void PrintCtx::printImpl(Load const& load) {
    type(load.type());
    comma();
    typedName(load.address());
}

void PrintCtx::printImpl(Store const& store) {
    typedName(store.address());
    comma();
    typedName(store.value());
}

void PrintCtx::printImpl(ConversionInst const& inst) {
    typedName(inst.operand());
    to();
    type(inst.type());
}

void PrintCtx::printImpl(CompareInst const& cmp) {
    keyword(cmp.operation());
    space();
    typedName(cmp.lhs());
    comma();
    typedName(cmp.rhs());
}

void PrintCtx::printImpl(UnaryArithmeticInst const& inst) {
    typedName(inst.operand());
}

void PrintCtx::printImpl(ArithmeticInst const& inst) {
    typedName(inst.lhs());
    comma();
    typedName(inst.rhs());
}

void PrintCtx::printImpl(Goto const& gt) { typedName(gt.target()); }

void PrintCtx::printImpl(Branch const& br) {
    typedName(br.condition());
    comma();
    typedName(br.thenTarget());
    comma();
    typedName(br.elseTarget());
}

void PrintCtx::printImpl(Return const& ret) {
    if (isa<VoidType>(ret.value()->type())) {
        return;
    }
    typedName(ret.value());
}

void PrintCtx::printImpl(Call const& call) {
    type(call.type());
    space();
    name(call.function());
    for (auto* arg: call.arguments()) {
        comma();
        typedName(arg);
    }
}

void PrintCtx::printImpl(Phi const& phi) {
    type(phi.type());
    space();
    for (bool first = true; auto const [pred, value]: phi.arguments()) {
        str << (first ? first = false, "" : ", ");
        str << "[" << label() << " " << formatName(pred) << " : "
            << formatName(value) << "]";
    }
}

void PrintCtx::printImpl(GetElementPointer const& gep) {
    keyword("inbounds");
    space();
    type(gep.inboundsType());
    comma();
    typedName(gep.basePointer());
    comma();
    typedName(gep.arrayIndex());
    indexList(gep.memberIndices());
}

void PrintCtx::printImpl(ExtractValue const& extract) {
    typedName(extract.baseValue());
    indexList(extract.memberIndices());
}

void PrintCtx::printImpl(InsertValue const& insert) {
    typedName(insert.baseValue());
    comma();
    typedName(insert.insertedValue());
    indexList(insert.memberIndices());
}

void PrintCtx::printImpl(Select const& select) {
    typedName(select.condition());
    comma();
    typedName(select.thenValue());
    comma();
    typedName(select.elseValue());
}

void PrintCtx::print(StructType const& structure) {
    str << formatKeyword("struct") << " " << formatType(&structure) << " {\n";
    indent.increase();
    for (bool first = true; auto const* type: structure.members()) {
        str << (first ? (first = false), "" : ",\n") << indent;
        str << formatType(type);
    }
    indent.decrease();
    str << "\n}\n\n";
}

void PrintCtx::print(ConstantData const& constData) {
    str << formatName(&constData) << equals();
    printDataAs(constData.dataType(), constData.data());
    str << "\n\n";
}

void PrintCtx::printDataAs(Type const* type, std::span<u8 const> data) {
    // clang-format off
    visit(*type, utl::overload{
        [](Type const& type) { SC_UNREACHABLE(); },
        [&](IntegralType const& type) {
            utl::small_vector<APInt::Limb> limbs(
                type.bitwidth() / sizeof(APInt::Limb));
            std::memcpy(limbs.data(), data.data(), type.bitwidth() / CHAR_BIT);
            str << formatType(&type) << " "
                << formatNumLiteral(APInt(limbs, type.bitwidth()).toString());
        },
        [&](FloatType const& type) {
            SC_ASSERT(type.bitwidth() == 32 || type.bitwidth() == 64, "");
            str << formatType(&type) << " ";
            if (type.bitwidth() == 32) {
                float f;
                std::memcpy(&f, data.data(), 4);
                str << formatNumLiteral(f);
            }
            else {
                double d;
                std::memcpy(&d, data.data(), 8);
                str << formatNumLiteral(d);
            }
        },
        [&](ArrayType const& type) {
            if (auto* intType = dyncast<IntegralType const*>(type.elementType());
                intType && intType->bitwidth() == 8)
            {
                std::string_view text(reinterpret_cast<char const*>(data.data()), data.size());
                tfmt::format(tfmt::Red, [&]{
                    str << '"';
                    printWithEscapeSeqs(str, text);
                    str << '"';
                });
                return;
            }
            str << formatType(&type) << " [";
            auto* ptr = data.data();
            auto* elemType = type.elementType();
            for (size_t i = 0, end = type.count();
                 i < end;
                 ++i, ptr += elemType->size())
            {
                if (i != 0) {
                    str << ", ";
                }
                printDataAs(elemType, { ptr, elemType->size() });
            }
            str << "]";
        },
    }); // clang-format on
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

void PrintCtx::funcDecl(ir::Callable const* func) {
    str << formatKeyword("func") << " " << formatType(func->returnType()) << " "
        << formatName(func) << "(";
    for (bool first = true; auto& param: func->parameters()) {
        str << (first ? (void)(first = false), "" : ", ")
            << formatType(param.type()) << " " << formatName(&param);
    }
    str << ")";
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
