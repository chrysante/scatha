#include "IR/Print.h"

#include <iostream>
#include <optional>
#include <sstream>

#include <range/v3/algorithm.hpp>
#include <termfmt/termfmt.h>
#include <utl/scope_guard.hpp>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Common/Base.h"
#include "Common/EscapeSequence.h"
#include "Common/Logging.h"
#include "Common/PrintUtil.h"
#include "IR/Attributes.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/PassRegistry.h"
#include "IR/PointerInfo.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;
using namespace tfmt::modifiers;

/// To expose the `print(Function)` function to the pass manager
static bool printPass(ir::Context&, ir::Function& function) {
    ir::print(function);
    return false;
}

SC_REGISTER_PASS(printPass, "print", PassCategory::Other);

/// To expose the `print(Module)` function to the pass manager
static bool printPass(ir::Context&, ir::Module& mod, LocalPass) {
    logging::header(" IR Module ");
    ir::print(mod);
    return false;
}

SC_REGISTER_GLOBAL_PASS(printPass, "print", PassCategory::Other);

namespace {

struct PrintCtx {
    explicit PrintCtx(std::ostream& str): str(str), indent(2) {}

    void print(Value const&);

    void printImpl(Value const&) { SC_UNREACHABLE(); }
    void printImpl(GlobalVariable const&);
    void printImpl(Function const&);
    void printImpl(ForeignFunction const&);
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

    void printImport(std::string_view libName);
    void print(StructType const& structure);

    void metadata(Value const& value, PointerInfo const&);

    void funcDecl(ir::Callable const*);
    void instDecl(Instruction const*) const;
    void name(Value const*) const;
    void type(Type const*) const;
    void typedName(Value const*) const;
    void keyword(auto...) const;
    void space() const { str << " "; }
    void comma() const {
        str << tfmt::format(None, ",");
        space();
    }
    void to() const;
    void indexList(auto list) const;

    std::ostream& str;
    Indenter indent;
};

} // namespace

static auto formatKeyword(auto... name) {
    return tfmt::format(Magenta | Bold, name...);
}

static auto missingEntity() {
    return tfmt::format(BGRed | BrightWhite, "NULL");
}

static auto formatNumLiteral(auto... value) {
    return tfmt::format(Cyan, value...);
}

static auto tertiary(auto... name) { return tfmt::format(BrightGrey, name...); }

static auto comment(std::invocable<std::ostream&> auto f) {
    return utl::streammanip([=](std::ostream& str) {
        tfmt::FormatGuard guard(BrightGrey, str);
        str << "// ";
        f(str);
    });
}

static auto comment(auto... args) {
    return comment([=](std::ostream& str) { ((str << args), ...); });
}

static auto formatInstName(auto... name) { return formatKeyword(name...); }

static utl::vstreammanip<> formatType(ir::Type const* type) {
    return [=](std::ostream& str) {
        if (!type) {
            str << missingEntity();
            return;
        }
        // clang-format off
        SC_MATCH (*type) {
            [&](StructType const& type) {
                if (!type.isAnonymous()) {
                    str << tfmt::format(Green, "@", type.name());
                    return;
                }
                str << tfmt::format(None, "{ ");
                for (bool first = true; auto [type, offset]: type.members()) {
                    str << (first ? first = false, "" : ", ")
                        << formatType(type);
                }
                str << tfmt::format(None, " }");
            },
            [&](ArrayType const& type) {
                str << tfmt::format(None, "[") << formatType(type.elementType()) << ", "
                    << formatNumLiteral(type.count()) << tfmt::format(None, "]");
            },
            [&](Type const& type) {
                SC_ASSERT(!type.isAnonymous(), "Only structs can be anonymous");
                str << tfmt::format(BrightBlue, type.name());
            }
        }; // clang-format on
    };
}

static std::string replaceSubstrings(
    std::string text, std::span<std::string_view const> substrs,
    std::span<std::string_view const> replacements) {
    for (auto [substr, replacement]: ranges::views::zip(substrs, replacements))
    {
        size_t n = 0;
        while ((n = text.find(substr, n)) != std::string::npos) {
            text.replace(n, substr.size(), replacement);
            n += replacement.size();
        }
    }
    return text;
}

static std::string htmlNameImpl(std::ostream const& str, Value const& value) {
    if (!tfmt::isHTMLFormattable(str)) {
        return std::string(value.name());
    }
    using namespace std::literals;
    return replaceSubstrings(std::string(value.name()),
                             std::array{ "<"sv, ">"sv, "&"sv },
                             std::array{ "&lt;"sv, "&gt;"sv, "&amp;"sv });
};

static std::array<char const*, 2> recordBrackets(
    ir::RecordConstant const& value) {
    // clang-format off
    return SC_MATCH (value) {
        [](ir::StructConstant const&) { return std::array{ "{ ", " }" }; },
        [](ir::ArrayConstant const&) { return std::array{ "[", "]" }; },
    }; // clang-format on
}

static std::optional<std::string> asStringLiteral(RecordConstant const* value) {
    auto* array = dyncast<ArrayConstant const*>(value);
    if (!array) {
        return std::nullopt;
    }
    auto* intType = dyncast<IntegralType const*>(array->type()->elementType());
    if (!intType || intType->bitwidth() != 8) {
        return std::nullopt;
    }
    std::string result(array->elements().size(), '\0');
    array->writeValueTo(result.data());
    return result;
}

static constexpr utl::streammanip formatStringLit = [](std::ostream& str,
                                                       std::string_view text) {
    tfmt::formatScope(Red, [&] {
        str << '\"';
        printWithEscapeSeqs(str, text);
        str << '\"';
    });
};

static void formatValueImpl(std::ostream& str, Value const* value) {
    if (!value) {
        str << tfmt::format(BrightWhite | BGBrightRed | Bold, "<NULL>");
        return;
    }
    auto htmlName = [&](Value const& value) {
        return htmlNameImpl(str, value);
    };
    // clang-format off
    SC_MATCH (*value) {
        [&](ir::Global const& global) {
            str << tfmt::format(Green,
                                "@", htmlName(global));
        },
        [&](ir::Parameter const& parameter) {
            str << tfmt::format(None, "%", htmlName(parameter));
        },
        [&](ir::BasicBlock const& basicBlock) {
            str << tfmt::format(Italic,
                                "%", htmlName(basicBlock));
        },
        [&](ir::Instruction const& inst) {
            str << tfmt::format(None, "%", htmlName(inst));
        },
        [&](ir::IntegralConstant const& value) {
            if (value.type() && value.type()->bitwidth() > 32) {
                str << formatNumLiteral(value.value().signedToString());
            }
            else {
                str << formatNumLiteral(value.value().toString());
            }
        },
        [&](ir::FloatingPointConstant const& value) {
            str << formatNumLiteral(value.value().toString());
        },
        [&](ir::NullPointerConstant const&) {
            str << formatKeyword("nullptr");
        },
        [&](ir::RecordConstant const& value) {
            if (auto text = asStringLiteral(&value)) {
                str << formatStringLit(*text);
                return;
            }
            auto brackets = recordBrackets(value);
            str << brackets[0];
            bool first = true;
            for (auto* elem: value.elements()) {
                if (!first) {
                    str << ", ";
                }
                first = false;
                if (elem) {
                    str << formatType(elem->type()) << " ";
                }
                formatValueImpl(str, elem);
            }
            str << brackets[1];
        },
        [&](ir::UndefValue const&) {
            str << formatKeyword("undef");
        },
        [&](ir::Value const&) {
            str << tfmt::format(BGMagenta, "???");
        },
    }; // clang-format on
}

static constexpr utl::streammanip doFormatName([](std::ostream& str,
                                                  Value const* value) {
    formatValueImpl(str, value);
});

static constexpr utl::streammanip formatSizeAlignAttrib([](std::ostream& str,
                                                           auto const& attrib) {
    str << "(" << formatType(attrib.type()) << ")";
});

static constexpr utl::streammanip formatAttrib([](std::ostream& str,
                                                  Attribute const& attrib) {
    // clang-format off
    SC_MATCH (attrib) {
        [&](ByValAttribute const& attrib) {
            str << formatKeyword("byval") << formatSizeAlignAttrib(attrib);
        },
        [&](ValRetAttribute const& attrib) {
            str << formatKeyword("valret") << formatSizeAlignAttrib(attrib);
        }
    }; // clang-format off
});

static auto equals() {
    return utl::streammanip([](std::ostream& str) -> std::ostream& {
        return str << " " << tfmt::format(None, "=") << " ";
    });
}

static auto label() { return tertiary("label"); }

void ir::print(Module const& mod) { ir::print(mod, std::cout); }

void ir::print(Module const& mod, std::ostream& str) {
    PrintCtx ctx(str);
    for (auto* structure: mod.structTypes()) {
        ctx.print(*structure);
    }
    auto globals = mod.globals() | TakeAddress | ToSmallVector<>;
    ranges::partition(globals, isa<GlobalVariable>);
    for (auto* global: globals) {
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

void ir::printDecl(Value const& value) { ir::printDecl(value, std::cout); }

void ir::printDecl(Value const& value, std::ostream& ostream) {
    PrintCtx ctx(ostream);
    // clang-format off
    SC_MATCH (value) {
        [&](Callable const& function) {
            ctx.funcDecl(&function);
        },
        [&](Instruction const& inst) {
            ctx.print(inst);
        },
        [&](Value const& value) {
            ctx.typedName(&value);
        }
    }; // clang-format on
}

utl::vstreammanip<> ir::format(Value const& value) {
    return [&](std::ostream& str) { printDecl(value, str); };
}

utl::vstreammanip<> ir::formatName(Value const& value) {
    return [&](std::ostream& str) { str << doFormatName(&value); };
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

void ir::print(Type const& type) { print(type, std::cout); }

void ir::print(Type const& type, std::ostream& ostream) {
    if (auto* structType = dyncast<StructType const*>(&type)) {
        PrintCtx{ ostream }.print(*structType);
    }
    else {
        ostream << type << std::endl;
    }
}

std::ostream& ir::operator<<(std::ostream& ostream, Type const& type) {
    return ostream << formatType(&type);
}

std::string ir::toString(Value const* value) {
    if (!value) {
        std::stringstream sstr;
        sstr << missingEntity();
        return sstr.str();
    }
    // clang-format off
    return SC_MATCH (*value) {
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
    }; // clang-format on
}

void PrintCtx::print(Value const& value) {
    if (auto* inst = dyncast<Instruction const*>(&value)) {
        instDecl(inst);
    }
    visit(value, [this](auto const& value) { printImpl(value); });
    if (auto* info = value.pointerInfo(); info && !isa<Callable>(value)) {
        str << "\n" << indent + 1;
        metadata(value, *info);
    }
    if (isa<Global>(value)) {
        str << "\n\n";
    }
}

void PrintCtx::printImpl(GlobalVariable const& var) {
    str << doFormatName(&var) << equals();
    keyword(var.isMutable() ? "global" : "constant");
    str << " ";
    typedName(var.initializer());
}

void PrintCtx::printImpl(Function const& function) {
    funcDecl(&function);
    str << " {\n";
    indent.increase();
    for (auto& bb: function) {
        print(bb);
    }
    indent.decrease();
    str << "}";
}

void PrintCtx::printImpl(ForeignFunction const& function) {
    str << formatKeyword("ext") << " ";
    funcDecl(&function);
}

static ssize_t length(auto const& fmt) {
    std::stringstream sstr;
    sstr << fmt;
    return utl::narrow_cast<ssize_t>(std::move(sstr).str().size());
}

void PrintCtx::printImpl(BasicBlock const& bb) {
    str << indent << doFormatName(&bb) << tfmt::format(None, ":");
    if (!bb.isEntry()) {
        ssize_t commentIndent = 30;
        ssize_t const currentColumn =
            indent.totalIndent() + length(doFormatName(&bb)) + 1;
        if (currentColumn >= commentIndent) {
            str << "\n";
        }
        tfmt::FormatGuard grey(BrightGrey, str);
        str << " " << comment([&](std::ostream& str) {
            str << "preds: ";
            for (bool first = true; auto* pred: bb.predecessors()) {
                str << (first ? first = false, "" : ", ") << pred->name();
            }
        });
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
    str << "<" << inst.nodeType() << ">";
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
    if (ret.value() && isa<VoidType>(ret.value()->type())) {
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
        str << tfmt::format(None, "[") << label() << " " << doFormatName(pred)
            << tfmt::format(None, " : ") << doFormatName(value)
            << tfmt::format(None, "]");
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

void PrintCtx::printImport(std::string_view libName) {
    str << formatKeyword("import") << " " << formatStringLit(libName) << "\n";
}

void PrintCtx::print(StructType const& structure) {
    str << formatKeyword("struct") << " " << formatType(&structure) << " {";
    utl::scope_guard closeBrace = [&] { str << "}\n\n"; };
    if (structure.members().empty()) {
        return;
    }
    str << "\n";
    indent.increase();
    for (bool first = true; auto [type, offset]: structure.members()) {
        str << (first ? (first = false), "" : ",\n") << indent;
        str << formatType(type);
    }
    indent.decrease();
    str << "\n";
}

void PrintCtx::metadata(Value const& value, PointerInfo const& info) {
    str << tfmt::format(Red, "#ptr") << "(";
    bool first = true;
    utl::streammanip field = [&](std::ostream& str) {
        if (!first) {
            str << ", ";
        }
        first = false;
    };
    if (info.align() != 1) {
        str << field << "align: " << info.align();
    }
    if (auto size = info.validSize()) {
        str << field << "validsize: " << *size;
    }
    if (auto* prov = info.provenance().value(); prov != &value) {
        str << field << "provenance: ";
        typedName(info.provenance().value());
        if (auto offset = info.staticProvenanceOffset(); offset && *offset != 0)
        {
            str << field << "offset: " << *offset;
        }
    }
    if (info.provenance().isStatic()) {
        str << field << "static";
    }
    if (info.isNonEscaping()) {
        str << field << "noescape";
    }
    if (info.guaranteedNotNull()) {
        str << field << "nonnull";
    }
    str << ")";
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
        << doFormatName(func) << "(";
    for (bool first = true; auto& param: func->parameters()) {
        str << (first ? (void)(first = false), "" : ", ")
            << formatType(param.type()) << " ";
        for (auto* attrib: param.attributes()) {
            str << formatAttrib(*attrib) << " ";
        }
        str << doFormatName(&param);
        if (auto* info = param.pointerInfo()) {
            str << " ";
            metadata(param, *info);
        }
    }
    str << ")";
}

void PrintCtx::instDecl(Instruction const* inst) const {
    /// Optional comment
    if (!inst->comment().empty()) {
        str << comment(inst->comment()) << "\n" << indent;
    }
    /// Name of the value
    if (!isa<VoidType>(inst->type())) {
        str << doFormatName(inst) << equals();
    }
    /// Name of the instruction
    str << formatInstName(toStrName(inst)) << " ";
}

void PrintCtx::name(Value const* value) const { str << doFormatName(value); }

void PrintCtx::type(Type const* type) const { str << formatType(type); }

void PrintCtx::typedName(Value const* value) const {
    if (!value) {
        str << missingEntity();
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
