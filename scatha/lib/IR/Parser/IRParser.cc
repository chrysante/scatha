#include "IR/IRParser.h"

#include <array>
#include <functional>

#include <range/v3/algorithm.hpp>
#include <svm/Builtin.h>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/APInt.h"
#include "Common/EscapeSequence.h"
#include "Common/Ranges.h"
#include "IR/Attributes.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/InvariantSetup.h"
#include "IR/Module.h"
#include "IR/Parser/IRLexer.h"
#include "IR/Parser/IRToken.h"
#include "IR/PointerInfo.h"
#include "IR/Type.h"
#include "IR/Validate.h"

using namespace scatha;
using namespace ir;

namespace {

/// A parsed but not necessarily "semantically analyzed" value. Constants and
/// many other values can be parsed directly and their `OptValue` will always
/// have valid `value()` pointer. But values that have not been defined yet
/// (because they are defined in a block that has not yet been parsed may have a
/// null `value()` pointer and are only represented by their first token, which
/// in all relevant cases is the single token. Their `value()` pointer will be
/// resolved later when the definition has been parsed.
class OptValue {
public:
    OptValue(Token token, Value* value = nullptr): val(value), tok(token) {}

    /// \Returns the value if it has already been resolved
    Value* value() const { return val; }

    /// \Returns the first token of the value
    Token token() const { return tok; }

private:
    Value* val = nullptr;
    Token tok;
};

struct ParserException {};

struct IRParser {
    using ValueMap = utl::hashmap<std::string, Value*>;
    struct PendingUpdate: std::function<void(Value*)> {
        using Base = std::function<void(Value*)>;
        PendingUpdate(Token token, Base base):
            Base(std::move(base)), token(token) {}

        Token token;
    };
    using PUMap = utl::hashmap<std::string, utl::small_vector<PendingUpdate>>;

    Context& ctx;
    Module& mod;
    ParseOptions const& options;
    Lexer lexer;
    std::optional<Token> nextToken[2];
    ValueMap globals;
    ValueMap locals;
    PUMap globalPendingUpdates;
    PUMap localPendingUpdates;
    std::vector<ParseIssue> issues;

    explicit IRParser(Context& ctx, Module& mod, std::string_view text,
                      ParseOptions const& options):
        ctx(ctx),
        mod(mod),
        options(options),
        lexer(text),
        nextToken{ lexer.next().value(), lexer.next().value() } {}

    void parse();

    UniquePtr<Global> parseGlobal();
    UniquePtr<GlobalVariable> parseGlobalVar();
    UniquePtr<Callable> parseCallable();
    UniquePtr<Parameter> parseParamDecl(size_t index);
    UniquePtr<Attribute> parseAttribute();
    UniquePtr<ForeignFunction> makeForeignFunction(Type const* returnType,
                                                   List<Parameter> params,
                                                   Token name);
    UniquePtr<BasicBlock> parseBasicBlock();
    UniquePtr<Instruction> parseInstruction();
    template <typename T>
    UniquePtr<Instruction> parseArithmeticConversion(std::string name);
    utl::small_vector<size_t> parseConstantIndices();
    UniquePtr<StructType> parseStructure();
    void parseMetadataFor(Value& value);
    bool validateFFIType(Token const& token, Type const* type);

    /// Try to parse a type, return `nullptr` if no type could be parsed
    Type const* tryParseType();

    /// Parse a type, report issue if no type could be parsed
    Type const* parseType();

    ///
    OptValue parseValue(Type const* type);

    template <typename V>
    V* getValue(Type const* type, Token const& token);

    size_t getIntLiteral(Token const& token);

    void registerValue(Token const& token, Value* value);

    void _addPendingUpdate(Token const& name, std::function<void(Value*)> f) {
        switch (name.kind()) {
        case TokenKind::GlobalIdentifier:
            globalPendingUpdates[name.id()].push_back({ name, std::move(f) });
            break;
        case TokenKind::LocalIdentifier:
            localPendingUpdates[name.id()].push_back({ name, std::move(f) });
            break;
        default:
            SC_UNREACHABLE();
        }
    }

    void checkSelfRef(auto* user, OptValue optVal) {
        if (optVal.value()) {
            return;
        }
        auto token = optVal.token();
        if (token.kind() != TokenKind::LocalIdentifier &&
            token.kind() != TokenKind::GlobalIdentifier)
        {
            return;
        }
        if (user->name() != token.id()) {
            return;
        }
        /// We report self references as use of undeclared identifier
        /// because the identifier is not defined before the next
        /// declaration.
        reportSemaIssueNoFail(token, SemanticIssue::UseOfUndeclaredIdentifier);
    }

    template <typename V = Value, std::derived_from<Value> V2>
    void addValueLink(V2* user, Type const* type, OptValue optVal, auto fn,
                      bool allowSelfRef = false) {
        if (!allowSelfRef) {
            checkSelfRef(user, optVal);
        }
        auto* value = [&] {
            if (optVal.value()) {
                return dyncast<V*>(optVal.value());
            }
            return getValue<V>(type, optVal.token());
        }();
        if (value) {
            if (type && value->type() && value->type() != type) {
                reportSemaIssueNoFail(optVal.token(),
                                      SemanticIssue::TypeMismatch);
            }
            std::invoke(fn, user, value);
            return;
        }
        _addPendingUpdate(optVal.token(), [=, this](Value* v) {
            SC_EXPECT(v);
            auto* value = dyncast<V*>(v);
            if (!value) {
                reportSemaIssueNoFail(optVal.token(),
                                      SemanticIssue::InvalidEntity);
            }
            else {
                std::invoke(fn, user, value);
            }
        });
    }

    void executePendingUpdates(Token const& name, Value* value);

    void postProcess();

    Token eatToken() {
        Token result = peekToken();
        if (result.kind() != TokenKind::EndOfFile) {
            auto next = lexer.next();
            if (!next) {
                reportLexicalIssue(next.error());
            }
            nextToken[0] = nextToken[1];
            nextToken[1] = next.value();
        }
        return result;
    }

    Token eatToken(size_t count) {
        SC_ASSERT(count > 0, "`count` must be positive");
        for (size_t i = 0; i < count - 1; ++i) {
            eatToken();
        }
        return eatToken();
    }

    Token peekToken(size_t i = 0) {
        SC_EXPECT(i < 2 && "We only have a look-ahead of 2");
        SC_EXPECT(nextToken[i].has_value());
        return *nextToken[i];
    }

    [[noreturn]] void reportLexicalIssue(LexicalIssue issue) {
        issues.push_back(issue);
        throw ParserException{};
    }

    [[noreturn]] void reportSyntaxIssue(Token const& token) {
        issues.push_back(SyntaxIssue(token));
        throw ParserException{};
    }

    void reportSemaIssueNoFail(Token const& token,
                               SemanticIssue::Reason reason) {
        issues.push_back(SemanticIssue(token, reason));
    }

    void reportSemaIssue(Token const& token, SemanticIssue::Reason reason) {
        reportSemaIssueNoFail(token, reason);
        throw ParserException{};
    }

    void expectAny(Token const& token, std::same_as<TokenKind> auto... kind) {
        if (((token.kind() != kind) && ...)) {
            reportSyntaxIssue(token);
        }
    }

    void expect(Token const& token, TokenKind kind) { expectAny(token, kind); }

    void expectNext(std::same_as<TokenKind> auto... next) {
        (expect(eatToken(), next), ...);
    }

    Conversion toConversion(Token token) {
        switch (token.kind()) {
#define SC_CONVERSION_DEF(Op, Keyword)                                         \
    case TokenKind::Op:                                                        \
        return Conversion::Op;
#include "IR/Lists.def"
        default:
            reportSyntaxIssue(token);
        }
    }

    APInt parseInt(Token token, size_t bitwidth) {
        expect(token, TokenKind::IntLiteral);
        auto value = APInt::parse(token.id(), 10, bitwidth);
        if (!value) {
            reportSyntaxIssue(token);
        }
        return *value;
    }

    APInt parseInt(Token token, Type const* type) {
        return parseInt(token, cast<IntegralType const*>(type)->bitwidth());
    }

    APFloat parseFloat(Token token, Type const* type) {
        SC_EXPECT(token.kind() == TokenKind::FloatLiteral);
        size_t bitwidth = cast<FloatType const*>(type)->bitwidth();
        SC_EXPECT(bitwidth == 32 || bitwidth == 64);
        auto value = APFloat::parse(token.id(), bitwidth == 32 ?
                                                    APFloatPrec::Single() :
                                                    APFloatPrec::Double());
        if (!value) {
            reportSyntaxIssue(token);
        }
        return *value;
    }

    CompareMode toCompareMode(Token token) {
        switch (token.kind()) {
        case TokenKind::SCmp:
            return CompareMode::Signed;
        case TokenKind::UCmp:
            return CompareMode::Unsigned;
        case TokenKind::FCmp:
            return CompareMode::Float;
        default:
            reportSyntaxIssue(token);
        }
    }

    CompareOperation toCompareOp(Token token) {
        switch (token.kind()) {
        case TokenKind::Equal:
            return CompareOperation::Equal;
        case TokenKind::NotEqual:
            return CompareOperation::NotEqual;
        case TokenKind::Less:
            return CompareOperation::Less;
        case TokenKind::LessEq:
            return CompareOperation::LessEq;
        case TokenKind::Greater:
            return CompareOperation::Greater;
        case TokenKind::GreaterEq:
            return CompareOperation::GreaterEq;
        default:
            reportSyntaxIssue(token);
        }
    }

    UnaryArithmeticOperation toUnaryArithmeticOp(Token token) {
        switch (token.kind()) {
        case TokenKind::Bnt:
            return UnaryArithmeticOperation::BitwiseNot;
        case TokenKind::Lnt:
            return UnaryArithmeticOperation::LogicalNot;
        case TokenKind::Neg:
            return UnaryArithmeticOperation::Negate;
        default:
            reportSyntaxIssue(token);
        }
    }

    ArithmeticOperation toArithmeticOp(Token token) {
        switch (token.kind()) {
        case TokenKind::Add:
            return ArithmeticOperation::Add;
        case TokenKind::Sub:
            return ArithmeticOperation::Sub;
        case TokenKind::Mul:
            return ArithmeticOperation::Mul;
        case TokenKind::SDiv:
            return ArithmeticOperation::SDiv;
        case TokenKind::UDiv:
            return ArithmeticOperation::UDiv;
        case TokenKind::SRem:
            return ArithmeticOperation::SRem;
        case TokenKind::URem:
            return ArithmeticOperation::URem;
        case TokenKind::FAdd:
            return ArithmeticOperation::FAdd;
        case TokenKind::FSub:
            return ArithmeticOperation::FSub;
        case TokenKind::FMul:
            return ArithmeticOperation::FMul;
        case TokenKind::FDiv:
            return ArithmeticOperation::FDiv;
        case TokenKind::LShL:
            return ArithmeticOperation::LShL;
        case TokenKind::LShR:
            return ArithmeticOperation::LShR;
        case TokenKind::AShL:
            return ArithmeticOperation::AShL;
        case TokenKind::AShR:
            return ArithmeticOperation::AShR;
        case TokenKind::And:
            return ArithmeticOperation::And;
        case TokenKind::Or:
            return ArithmeticOperation::Or;
        case TokenKind::XOr:
            return ArithmeticOperation::XOr;
        default:
            reportSyntaxIssue(token);
        }
    }

    void checkEmpty(PUMap const& updates) {
        /// We expect `globalPendingUpdates` to be empty when parsed
        /// successfully.
        for (auto&& [name, list]: updates) {
            for (auto&& update: list) {
                reportSemaIssueNoFail(update.token,
                                      SemanticIssue::UseOfUndeclaredIdentifier);
            }
        }
    }
};

} // namespace

Expected<std::pair<Context, Module>, ParseIssue> ir::parse(
    std::string_view text) {
    Context ctx;
    Module mod;
    auto result = parseTo(text, ctx, mod);
    if (result.empty()) {
        return { std::move(ctx), std::move(mod) };
    }
    return result.front();
}

std::vector<ParseIssue> ir::parseTo(std::string_view text, Context& ctx,
                                    Module& mod, ParseOptions const& options) {
    IRParser parser(ctx, mod, text, options);
    parser.parse();
    if (!parser.issues.empty()) {
        return std::move(parser.issues);
    }
    parser.postProcess();
    assertInvariants(ctx, mod);
    return {};
}

void IRParser::parse() {
    while (peekToken().kind() != TokenKind::EndOfFile) {
        try {
            if (auto type = parseStructure()) {
                if (options.typeParseCallback) {
                    options.typeParseCallback(*type);
                }
                mod.addStructure(std::move(type));
                continue;
            }
            if (auto global = parseGlobal()) {
                if (options.objectParseCallback) {
                    options.objectParseCallback(*global);
                }
                mod.addGlobal(std::move(global));
                continue;
            }
            reportSyntaxIssue(peekToken());
        }
        catch (ParserException const&) {
            return;
        }
    }
    checkEmpty(globalPendingUpdates);
}

UniquePtr<Global> IRParser::parseGlobal() {
    if (auto var = parseGlobalVar()) {
        return var;
    }
    if (auto fn = parseCallable()) {
        return fn;
    }
    return nullptr;
}

UniquePtr<GlobalVariable> IRParser::parseGlobalVar() {
    Token const name = peekToken();
    if (name.kind() != TokenKind::GlobalIdentifier) {
        return nullptr;
    }
    eatToken();
    expectNext(TokenKind::Assign);
    auto mut = [&] {
        using enum GlobalVariable::Mutability;
        auto kind = eatToken();
        switch (kind.kind()) {
        case TokenKind::Global:
            return Mutable;
        case TokenKind::Constant:
            return Const;
        default:
            reportSyntaxIssue(kind);
        }
    }();
    auto* type = parseType();
    auto value = parseValue(type);
    auto global =
        allocate<GlobalVariable>(ctx, mut, nullptr, std::string(name.id()));
    addValueLink<Constant>(global.get(), type, value,
                           &GlobalVariable::setInitializer);
    bool success =
        globals.insert({ std::string(name.id()), global.get() }).second;
    if (!success) {
        reportSemaIssueNoFail(name, SemanticIssue::Redeclaration);
    }
    if (peekToken().kind() == TokenKind::MetadataIdentifier) {
        parseMetadataFor(*global);
    }
    return global;
}

UniquePtr<Callable> IRParser::parseCallable() {
    locals.clear();
    bool const isForeign = peekToken().kind() == TokenKind::Ext;
    if (isForeign) {
        eatToken();
    }
    bool isValidFFISig = true;
    Token const declarator = peekToken();
    if (isForeign) {
        expect(declarator, TokenKind::Function);
    }
    if (declarator.kind() != TokenKind::Function) {
        return nullptr;
    }
    eatToken();
    auto retTypeTok = peekToken();
    auto* const returnType = parseType();
    if (isForeign) {
        isValidFFISig &= validateFFIType(retTypeTok, returnType);
    }
    Token const name = eatToken();
    expect(eatToken(), TokenKind::OpenParan);
    List<Parameter> parameters;
    size_t index = 0;
    if (peekToken().kind() != TokenKind::CloseParan) {
        parameters.push_back(parseParamDecl(index++).release());
    }
    while (true) {
        if (peekToken().kind() != TokenKind::Comma) {
            break;
        }
        eatToken(); // Comma
        auto argTypeTok = peekToken();
        parameters.push_back(parseParamDecl(index++).release());
        if (isForeign) {
            isValidFFISig &=
                validateFFIType(argTypeTok, parameters.back().type());
        }
    }
    expect(eatToken(), TokenKind::CloseParan);
    if (isForeign) {
        if (!isValidFFISig) {
            return nullptr;
        }
        auto function =
            makeForeignFunction(returnType, std::move(parameters), name);
        registerValue(name, function.get());
        return function;
    }
    auto function =
        allocate<Function>(ctx, returnType, std::move(parameters),
                           std::string(name.id()), FunctionAttribute::None,
                           Visibility::External); // FIXME: Parse
                                                  // function visibility
    registerValue(name, function.get());
    expect(eatToken(), TokenKind::OpenBrace);
    /// Parse the body of the function.
    while (true) {
        auto basicBlock = parseBasicBlock();
        if (!basicBlock) {
            break;
        }
        function->pushBack(std::move(basicBlock));
    }
    expect(eatToken(), TokenKind::CloseBrace);
    checkEmpty(localPendingUpdates);
    setupInvariants(ctx, *function);
    return function;
}

UniquePtr<Parameter> IRParser::parseParamDecl(size_t index) {
    auto* type = parseType();
    utl::small_vector<UniquePtr<Attribute>> attribs;
    while (true) {
        if (auto attrib = parseAttribute()) {
            attribs.push_back(std::move(attrib));
        }
        else {
            break;
        }
    }
    UniquePtr<Parameter> result;
    if (peekToken().kind() == TokenKind::LocalIdentifier) {
        result = allocate<Parameter>(type, index, std::string(eatToken().id()),
                                     nullptr);
    }
    else {
        result = allocate<Parameter>(type, index, nullptr);
    }
    for (auto& attrib: attribs) {
        result->addAttribute(std::move(attrib));
    }
    locals[std::string(result->name())] = result.get();
    return result;
}

static AttributeType toAttribType(TokenKind kind) {
    switch (kind) {
    case TokenKind::ByVal:
        return AttributeType::ByValAttribute;
    case TokenKind::ValRet:
        return AttributeType::ValRetAttribute;
    default:
        SC_UNREACHABLE();
    }
}

UniquePtr<Attribute> IRParser::parseAttribute() {
    switch (peekToken().kind()) {
    case TokenKind::ByVal:
    case TokenKind::ValRet: {
        auto type = toAttribType(peekToken().kind());
        eatToken();
        expect(eatToken(), TokenKind::OpenParan);
        size_t size = 0, align = 0;
        bool first = true;
        while (true) {
            auto tok = eatToken();
            if (tok.kind() == TokenKind::CloseParan) {
                break;
            }
            if (!first) {
                expect(tok, TokenKind::Comma);
                tok = eatToken();
            }
            first = false;
            expect(tok, TokenKind::OtherID);
            if (tok.id() == "size") {
                expect(eatToken(), TokenKind::Colon);
                size = getIntLiteral(eatToken());
            }
            else if (tok.id() == "align") {
                expect(eatToken(), TokenKind::Colon);
                align = getIntLiteral(eatToken());
            }
        }
        switch (type) {
        case AttributeType::ByValAttribute:
            return allocate<ByValAttribute>(size, align);
        case AttributeType::ValRetAttribute:
            return allocate<ValRetAttribute>(size, align);
        default:
            SC_UNREACHABLE();
        }
    }
    default:
        return nullptr;
    }
}

UniquePtr<ForeignFunction> IRParser::makeForeignFunction(Type const* returnType,
                                                         List<Parameter> params,
                                                         Token name) {
    return allocate<ForeignFunction>(ctx, returnType, std::move(params),
                                     std::string(name.id()),
                                     FunctionAttribute::None);
}

UniquePtr<BasicBlock> IRParser::parseBasicBlock() {
    if (peekToken().kind() != TokenKind::LocalIdentifier) {
        return nullptr;
    }
    Token const name = eatToken();
    expect(eatToken(), TokenKind::Colon);
    auto result = allocate<BasicBlock>(ctx, std::string(name.id()));
    registerValue(name, result.get());
    while (true) {
        auto optInstName = peekToken();
        auto instruction = parseInstruction();
        if (!instruction) {
            break;
        }
        /// Phi instructions register themselves because they may be self
        /// referential
        if (!isa<Phi>(instruction.get())) {
            registerValue(optInstName, instruction.get());
        }
        if (peekToken().kind() == TokenKind::MetadataIdentifier) {
            parseMetadataFor(*instruction);
        }
        result->pushBack(std::move(instruction));
    }
    return result;
}

UniquePtr<Instruction> IRParser::parseInstruction() {
    auto _nameTok = peekToken(0);
    std::optional _name = [&]() -> std::optional<std::string> {
        if (_nameTok.kind() != TokenKind::LocalIdentifier) {
            return std::nullopt;
        }
        auto assignToken = peekToken(1);
        if (assignToken.kind() != TokenKind::Assign) {
            return std::nullopt;
        }
        eatToken(2);
        return std::string(_nameTok.id());
    }();
    auto name = [&] {
        if (!_name) {
            reportSyntaxIssue(_nameTok);
        }
        return *_name;
    };
    auto nameOrEmpty = [&] { return _name.value_or(std::string{}); };
    switch (peekToken().kind()) {
    case TokenKind::Alloca: {
        eatToken();
        auto* type = parseType();
        auto result = allocate<Alloca>(ctx, type, name());
        if (peekToken().kind() != TokenKind::Comma) {
            return result;
        }
        eatToken();
        auto* countType = parseType();
        auto count = parseValue(countType);
        addValueLink(result.get(), countType, count, &Alloca::setCount);
        return result;
    }
    case TokenKind::Load: {
        eatToken();
        auto* const type = parseType();
        expect(eatToken(), TokenKind::Comma);
        auto* ptrType = parseType();
        auto const ptr = parseValue(ptrType);
        auto result = allocate<Load>(nullptr, type, name());
        addValueLink(result.get(), ctx.ptrType(), ptr, &Load::setAddress);
        return result;
    }
    case TokenKind::Store: {
        eatToken();
        auto* ptrType = parseType();
        auto const addr = parseValue(ptrType);
        expect(eatToken(), TokenKind::Comma);
        auto* valueType = parseType();
        auto const value = parseValue(valueType);
        auto result = allocate<Store>(ctx, nullptr, nullptr);
        addValueLink(result.get(), ctx.ptrType(), addr, &Store::setAddress);
        addValueLink(result.get(), valueType, value, &Store::setValue);
        return result;
    }
    case TokenKind::Zext:
        [[fallthrough]];
    case TokenKind::Sext:
        [[fallthrough]];
    case TokenKind::Trunc:
        [[fallthrough]];
    case TokenKind::Fext:
        [[fallthrough]];
    case TokenKind::Ftrunc:
        [[fallthrough]];
    case TokenKind::UtoF:
        [[fallthrough]];
    case TokenKind::StoF:
        [[fallthrough]];
    case TokenKind::FtoU:
        [[fallthrough]];
    case TokenKind::FtoS:
        [[fallthrough]];
    case TokenKind::Bitcast: {
        auto conv = toConversion(eatToken());
        auto* valueType = parseType();
        auto value = parseValue(valueType);
        expect(eatToken(), TokenKind::To);
        auto* targetType = parseType();
        auto result =
            allocate<ConversionInst>(nullptr, targetType, conv, name());
        addValueLink(result.get(), valueType, value,
                     &ConversionInst::setOperand);
        return result;
    }
    case TokenKind::Goto: {
        eatToken();
        expect(eatToken(), TokenKind::Label);
        auto targetName = eatToken();
        expect(targetName, TokenKind::LocalIdentifier);
        auto result = allocate<Goto>(ctx, nullptr);
        addValueLink<BasicBlock>(result.get(), nullptr, targetName,
                                 &Goto::setTarget);
        return result;
    }
    case TokenKind::Branch: {
        eatToken();
        auto condTypeName = peekToken();
        auto* condType = parseType();
        if (condType != ctx.intType(1)) {
            reportSemaIssue(condTypeName, SemanticIssue::InvalidType);
        }
        auto cond = parseValue(condType);
        expectNext(TokenKind::Comma, TokenKind::Label);
        auto thenName = eatToken();
        expectNext(TokenKind::Comma, TokenKind::Label);
        auto elseName = eatToken();
        auto result = allocate<Branch>(ctx, nullptr, nullptr, nullptr);
        addValueLink(result.get(), ctx.intType(1), cond, &Branch::setCondition);
        addValueLink<BasicBlock>(result.get(), ctx.voidType(), thenName,
                                 &Branch::setThenTarget);
        addValueLink<BasicBlock>(result.get(), ctx.voidType(), elseName,
                                 &Branch::setElseTarget);
        return result;
    }
    case TokenKind::Return: {
        eatToken();
        auto result = allocate<Return>(ctx, nullptr);
        auto* valueType = tryParseType();
        if (!valueType) {
            result->setValue(ctx.voidValue());
            return result;
        }
        auto value = parseValue(valueType);
        addValueLink(result.get(), valueType, value, &Return::setValue);
        return result;
    }
    case TokenKind::Call: {
        eatToken();
        auto retTypeName = peekToken();
        auto* retType = parseType();
        auto funcName = eatToken();
        using CallArg = std::pair<Type const*, OptValue>;
        utl::small_vector<CallArg> args;
        while (true) {
            if (peekToken().kind() != TokenKind::Comma) {
                break;
            }
            eatToken();
            auto* argType = parseType();
            auto arg = parseValue(argType);
            args.push_back({ argType, arg });
        }
        utl::small_vector<Value*> nullArgs(args.size());
        auto result = allocate<Call>(retType, nullptr, nullArgs, nameOrEmpty());
        addValueLink(result.get(), nullptr, funcName,
                     [=, this](Call* call, Value* value) {
            auto* func = dyncast<Callable const*>(value);
            if (func && func->returnType() != retType) {
                reportSemaIssue(retTypeName, SemanticIssue::TypeMismatch);
            }
            call->setFunction(value);
        });
        /// We set the type manually. If the called function is not parsed yet
        /// but we try to access the type, i.e. because an ExtractValue
        /// instruction uses the return value, we would crash. `addValueLink`
        /// asserted or will assert that the return type is correct.
        result->setType(retType);
        for (auto [index, arg]: args | ranges::views::enumerate) {
            auto [type, name] = arg;
            addValueLink(result.get(), type, name,
                         [index = index](Call* call, Value* arg) {
                call->setArgument(index, arg);
            });
        }
        return result;
    }
    case TokenKind::Phi: {
        eatToken();
        auto* type = parseType();
        using PhiArg = std::pair<Token, OptValue>;
        utl::small_vector<PhiArg> args;
        while (true) {
            expectNext(TokenKind::OpenBracket, TokenKind::Label);
            auto predName = eatToken();
            expect(eatToken(), TokenKind::Colon);
            auto value = parseValue(type);
            expect(eatToken(), TokenKind::CloseBracket);
            args.push_back({ predName, value });
            if (peekToken().kind() != TokenKind::Comma) {
                break;
            }
            eatToken();
        }
        auto result = allocate<Phi>(type, args.size(), name());
        registerValue(_nameTok, result.get());
        for (auto [index, arg]: args | ranges::views::enumerate) {
            auto [predName, value] = arg;
            addValueLink<BasicBlock>(result.get(), ctx.voidType(), predName,
                                     [index = index](Phi* phi,
                                                     BasicBlock* pred) {
                phi->setPredecessor(index, pred);
            });
            addValueLink(
                result.get(), type, value,
                [index = index](Phi* phi, Value* value) {
                phi->setArgument(index, value);
            },
                /* allowSelfRef = */ true);
        }
        return result;
    }
    case TokenKind::SCmp:
        [[fallthrough]];
    case TokenKind::UCmp:
        [[fallthrough]];
    case TokenKind::FCmp: {
        auto mode = toCompareMode(eatToken());
        auto op = toCompareOp(eatToken());
        auto* lhsType = parseType();
        auto lhs = parseValue(lhsType);
        expect(eatToken(), TokenKind::Comma);
        auto* rhsType = parseType();
        auto rhs = parseValue(rhsType);
        auto result =
            allocate<CompareInst>(ctx, nullptr, nullptr, mode, op, name());
        addValueLink(result.get(), lhsType, lhs, &CompareInst::setLHS);
        addValueLink(result.get(), rhsType, rhs, &CompareInst::setRHS);
        return result;
    }
    case TokenKind::Bnt:
        [[fallthrough]];
    case TokenKind::Lnt:
        [[fallthrough]];
    case TokenKind::Neg: {
        auto op = toUnaryArithmeticOp(eatToken());
        auto* valueType = parseType();
        auto value = parseValue(valueType);
        auto result = allocate<UnaryArithmeticInst>(ctx, nullptr, op, name());
        addValueLink(result.get(), valueType, value,
                     [&](auto* inst, auto* op) { inst->setOperand(ctx, op); });
        return result;
    }

    case TokenKind::Add:
        [[fallthrough]];
    case TokenKind::Sub:
        [[fallthrough]];
    case TokenKind::Mul:
        [[fallthrough]];
    case TokenKind::SDiv:
        [[fallthrough]];
    case TokenKind::UDiv:
        [[fallthrough]];
    case TokenKind::SRem:
        [[fallthrough]];
    case TokenKind::URem:
        [[fallthrough]];
    case TokenKind::FAdd:
        [[fallthrough]];
    case TokenKind::FSub:
        [[fallthrough]];
    case TokenKind::FMul:
        [[fallthrough]];
    case TokenKind::FDiv:
        [[fallthrough]];
    case TokenKind::LShL:
        [[fallthrough]];
    case TokenKind::LShR:
        [[fallthrough]];
    case TokenKind::AShL:
        [[fallthrough]];
    case TokenKind::AShR:
        [[fallthrough]];
    case TokenKind::And:
        [[fallthrough]];
    case TokenKind::Or:
        [[fallthrough]];
    case TokenKind::XOr: {
        auto op = toArithmeticOp(eatToken());
        auto* lhsType = parseType();
        auto lhs = parseValue(lhsType);
        expect(eatToken(), TokenKind::Comma);
        auto* rhsType = parseType();
        auto rhs = parseValue(rhsType);
        auto result = allocate<ArithmeticInst>(nullptr, nullptr, op, name());
        addValueLink(result.get(), lhsType, lhs, &ArithmeticInst::setLHS);
        addValueLink(result.get(), rhsType, rhs, &ArithmeticInst::setRHS);
        return result;
    }
    case TokenKind::GetElementPointer: {
        eatToken();
        expectNext(TokenKind::Inbounds);
        auto* accessedType = parseType();
        expectNext(TokenKind::Comma, TokenKind::Ptr);
        auto basePtr = parseValue(accessedType);
        expectNext(TokenKind::Comma);
        auto* indexType = parseType();
        auto index = parseValue(indexType);
        auto indices = parseConstantIndices();
        auto result = allocate<GetElementPointer>(ctx, accessedType, nullptr,
                                                  nullptr, indices, name());
        addValueLink(result.get(), ctx.ptrType(), basePtr,
                     &GetElementPointer::setBasePtr);
        addValueLink(result.get(), indexType, index,
                     &GetElementPointer::setArrayIndex);
        return result;
    }
    case TokenKind::InsertValue: {
        eatToken();
        auto* baseType = parseType();
        auto base = parseValue(baseType);
        expectNext(TokenKind::Comma);
        auto* insType = parseType();
        auto ins = parseValue(insType);
        auto indices = parseConstantIndices();
        if (indices.empty()) {
            reportSyntaxIssue(peekToken());
        }
        auto result = allocate<InsertValue>(nullptr, nullptr, indices, name());
        addValueLink(result.get(), baseType, base, &InsertValue::setBaseValue);
        addValueLink(result.get(), insType, ins,
                     &InsertValue::setInsertedValue);
        return result;
    }
    case TokenKind::ExtractValue: {
        eatToken();
        auto* baseType = parseType();
        auto base = parseValue(baseType);
        auto indices = parseConstantIndices();
        if (indices.empty()) {
            reportSyntaxIssue(peekToken());
        }
        auto result = allocate<ExtractValue>(nullptr, indices, name());
        addValueLink(result.get(), baseType, base, &ExtractValue::setBaseValue);
        return result;
    }
    case TokenKind::Select: {
        eatToken();
        auto condTypeName = peekToken();
        auto* condType = parseType();
        if (condType != ctx.intType(1)) {
            reportSemaIssue(condTypeName, SemanticIssue::InvalidType);
        }
        auto cond = parseValue(condType);
        expectNext(TokenKind::Comma);
        auto* thenType = parseType();
        auto thenVal = parseValue(thenType);
        expectNext(TokenKind::Comma);
        auto* elseType = parseType();
        auto elseVal = parseValue(elseType);
        auto result = allocate<Select>(nullptr, nullptr, nullptr, name());
        addValueLink(result.get(), condType, cond, &Select::setCondition);
        addValueLink(result.get(), thenType, thenVal, &Select::setThenValue);
        addValueLink(result.get(), elseType, elseVal, &Select::setElseValue);
        return result;
    }
    default:
        return nullptr;
    }
}

utl::small_vector<size_t> IRParser::parseConstantIndices() {
    utl::small_vector<size_t> result;
    while (true) {
        if (peekToken().kind() != TokenKind::Comma) {
            return result;
        }
        eatToken();
        size_t const index = getIntLiteral(eatToken());
        result.push_back(index);
    }
}

UniquePtr<StructType> IRParser::parseStructure() {
    if (peekToken().kind() != TokenKind::Structure) {
        return nullptr;
    }
    eatToken();
    Token const nameID = eatToken();
    expect(nameID, TokenKind::GlobalIdentifier);
    expect(eatToken(), TokenKind::OpenBrace);
    utl::small_vector<Type const*> members;
    while (true) {
        if (peekToken().kind() == TokenKind::CloseBrace) {
            break;
        }
        auto* type = parseType();
        members.push_back(type);
        if (peekToken().kind() != TokenKind::Comma) {
            break;
        }
        eatToken();
    }
    expect(eatToken(), TokenKind::CloseBrace);
    auto structure = allocate<StructType>(std::string(nameID.id()), members);
    return structure;
}

Type const* IRParser::tryParseType() {
    Token const token = peekToken();
    switch (token.kind()) {
    case TokenKind::Void:
        eatToken();
        return ctx.voidType();
    case TokenKind::Ptr:
        eatToken();
        return ctx.ptrType();
    case TokenKind::GlobalIdentifier: {
        eatToken();
        auto structures = mod.structures();
        auto itr = ranges::find_if(structures, [&](auto&& type) {
            // TODO: Handle '@' and '%' prefixes
            return type->name() == token.id();
        });
        if (itr == ranges::end(structures)) {
            reportSemaIssue(token, SemanticIssue::UseOfUndeclaredIdentifier);
        }
        return itr->get();
    }
    case TokenKind::IntType:
        eatToken();
        return ctx.intType(token.width());
    case TokenKind::FloatType:
        eatToken();
        return ctx.floatType(token.width());
    case TokenKind::OpenBrace: {
        eatToken();
        utl::small_vector<Type const*> members;
        while (true) {
            Token const typeTok = peekToken();
            auto* type = parseType();
            if (!type) {
                reportSemaIssue(typeTok, SemanticIssue::UnexpectedID);
            }
            members.push_back(type);
            if (peekToken().kind() == TokenKind::CloseBrace) {
                eatToken();
                return ctx.anonymousStruct(members);
            }
            expect(eatToken(), TokenKind::Comma);
        }
    }
    case TokenKind::OpenBracket: {
        eatToken();
        Token const typeTok = peekToken();
        auto* type = parseType();
        if (!type) {
            reportSemaIssue(typeTok, SemanticIssue::UnexpectedID);
        }
        expect(eatToken(), TokenKind::Comma);
        size_t const count = getIntLiteral(eatToken());
        expect(eatToken(), TokenKind::CloseBracket);
        return ctx.arrayType(type, count);
    }
    default:
        return nullptr;
    }
}

Type const* IRParser::parseType() {
    auto* type = tryParseType();
    if (type) {
        return type;
    }
    reportSemaIssue(peekToken(), SemanticIssue::ExpectedType);
    return nullptr;
}

OptValue IRParser::parseValue(Type const* type) {
    auto token = eatToken();
    switch (token.kind()) {
    case TokenKind::LocalIdentifier:
        return token;
    case TokenKind::GlobalIdentifier:
        return token;
    case TokenKind::NullptrLiteral:
        return { token, ctx.nullpointer() };
    case TokenKind::UndefLiteral:
        return { token, ctx.undef(type) };
    case TokenKind::IntLiteral: {
        auto* intType = dyncast<IntegralType const*>(type);
        if (!intType) {
            reportSemaIssue(token, SemanticIssue::InvalidType);
        }
        auto value = parseInt(token, intType);
        return { token, ctx.intConstant(value) };
    }
    case TokenKind::FloatLiteral: {
        auto* floatType = dyncast<FloatType const*>(type);
        if (!floatType) {
            reportSemaIssue(token, SemanticIssue::InvalidType);
        }
        auto value = parseFloat(token, floatType);
        return { token, ctx.floatConstant(value) };
    }
    case TokenKind::OpenBrace:
        [[fallthrough]];
    case TokenKind::OpenBracket: {
        auto* recordType = dyncast<RecordType const*>(type);
        if (!recordType) {
            reportSemaIssue(token, SemanticIssue::UnexpectedID);
        }
        auto closeTok = token.kind() == TokenKind::OpenBrace ?
                            TokenKind::CloseBrace :
                            TokenKind::CloseBracket;

        utl::small_vector<OptValue> elems;
        while (true) {
            if (peekToken().kind() == closeTok) {
                eatToken();
                break;
            }
            if (!elems.empty()) {
                expect(eatToken(), TokenKind::Comma);
            }
            auto* type = parseType();
            auto val = parseValue(type);
            elems.push_back(val);
        }
        auto* aggrValue =
            ctx.recordConstant(utl::small_vector<Constant*>(elems.size()),
                               recordType);
        for (auto [index, elem]: elems | ranges::views::enumerate) {
            addValueLink<Constant>(aggrValue, recordType->elementAt(index),
                                   elem, [index = index](User* u, Constant* c) {
                u->setOperand(index, c);
            });
        }
        return { token, aggrValue };
    }
    case TokenKind::StringLiteral: {
        auto text = toEscapedValue(token.id());
        auto elems = text | ranges::views::transform([&](char c) -> Constant* {
            return ctx.intConstant(static_cast<unsigned>(c), 8);
        }) | ToSmallVector<>;
        auto* arrayType = dyncast<ArrayType const*>(type);
        if (!arrayType || arrayType->elementType() != ctx.intType(8) ||
            arrayType->count() != elems.size())
        {
            reportSemaIssue(token, SemanticIssue::TypeMismatch);
        }
        return { token, ctx.arrayConstant(elems, arrayType) };
    }
    default:
        reportSyntaxIssue(token);
    }
}

void IRParser::parseMetadataFor(Value& value) {
    auto tok = eatToken();
    SC_EXPECT(tok.kind() == TokenKind::MetadataIdentifier);
    if (tok.id() == "ptr") {
        if (peekToken().kind() != TokenKind::OpenParan) {
            value.allocatePointerInfo(PointerInfo());
            return;
        }
        eatToken(); // Open paran
        std::optional<size_t> align;
        std::optional<size_t> validSize;
        Type const* provType = nullptr;
        std::optional<OptValue> prov;
        std::optional<size_t> provOffset;
        bool nonnull = false;
        for (int i = 0;; ++i) {
            auto tok = eatToken();
            if (tok.kind() == TokenKind::CloseParan) {
                break;
            }
            if (i > 0) {
                expect(tok, TokenKind::Comma);
                tok = eatToken();
            }
            if (!isBuiltinID(tok.kind())) {
                reportSyntaxIssue(tok);
            }
            if (tok.id() == "align") {
                expect(eatToken(), TokenKind::Colon);
                align = parseInt(eatToken(), /*width=*/9).to<size_t>();
            }
            else if (tok.id() == "validsize") {
                expect(eatToken(), TokenKind::Colon);
                validSize = parseInt(eatToken(), /*width=*/54).to<size_t>();
            }
            else if (tok.id() == "provenance") {
                expect(eatToken(), TokenKind::Colon);
                provType = parseType();
                prov = parseValue(provType);
            }
            else if (tok.id() == "offset") {
                expect(eatToken(), TokenKind::Colon);
                provOffset = parseInt(eatToken(), /*width=*/54).to<size_t>();
            }
            else if (tok.id() == "nonnull") {
                nonnull = true;
            }
            else {
                reportSyntaxIssue(tok);
            }
        }
        auto assign = [=](Value* value, Value* prov) {
            value->allocatePointerInfo({
                .align = align.value_or(1),
                .validSize = validSize,
                .provenance = prov,
                .staticProvenanceOffset = provOffset,
                .guaranteedNotNull = nonnull,
            });
        };
        if (prov) {
            addValueLink(&value, provType, *prov, assign,
                         /* allowSelfRef = */ true);
        }
        else {
            assign(&value, nullptr);
        }
    }
    else {
        reportSyntaxIssue(tok);
    }
}

static bool isBitInt(Type const* type, size_t bitwidth) {
    auto* intType = dyncast<IntegralType const*>(type);
    if (!intType) {
        return false;
    }
    return intType->bitwidth() == bitwidth;
}

static bool isArrayPointer(StructType const& type) {
    return type.numElements() == 2 && isa<PointerType>(type.elementAt(0)) &&
           isBitInt(type.elementAt(1), 64);
}

bool IRParser::validateFFIType(Token const& token, Type const* type) {
    // clang-format off
    bool valid = SC_MATCH (*type){
        [](VoidType const&) { return true; },
        [](IntegralType const& type) {
            return ranges::contains(std::array{ 1, 8, 16, 32, 64 },
                                    type.bitwidth());
        },
        [](FloatType const& type) {
            return type.bitwidth() == 32 || type.bitwidth() == 64;
        },
        [](PointerType const&) { return true; },
        [](StructType const& type) {
            /// Only array pointers are supported for now
            return isArrayPointer(type);
        },
        [](Type const&) {
            return false;
        }
    }; // clang-format on
    if (valid) {
        return true;
    }
    reportSemaIssue(token, SemanticIssue::InvalidFFIType);
    return false;
}

template <typename V>
V* IRParser::getValue(Type const* type, Token const& token) {
    switch (token.kind()) {
    case TokenKind::LocalIdentifier:
        [[fallthrough]];
    case TokenKind::GlobalIdentifier: {
        auto& values = token.kind() == TokenKind::LocalIdentifier ? locals :
                                                                    globals;
        auto const itr = values.find(token.id());
        if (itr == values.end()) {
            return nullptr;
        }
        auto* value = dyncast<V*>(itr->second);
        if (!value) {
            reportSemaIssue(token, SemanticIssue::InvalidEntity);
        }
        if (value->type() && type && value->type() != type) {
            reportSemaIssue(token, SemanticIssue::TypeMismatch);
        }
        return value;
    }
    default:
        reportSyntaxIssue(token);
    }
}

size_t IRParser::getIntLiteral(Token const& token) {
    if (token.kind() != TokenKind::IntLiteral) {
        reportSyntaxIssue(token);
    }
    return utl::narrow_cast<size_t>(std::atoll(token.id().data()));
}

void IRParser::registerValue(Token const& token, Value* value) {
    if (value->name().empty()) {
        return;
    }
    auto& values = [&]() -> auto& {
        switch (token.kind()) {
        case TokenKind::GlobalIdentifier:
            return globals;
        case TokenKind::LocalIdentifier:
            return locals;
        default:
            reportSyntaxIssue(token);
        }
    }();
    if (values.contains(value->name())) {
        reportSemaIssue(token, SemanticIssue::Redeclaration);
    }
    values[value->name()] = value;
    executePendingUpdates(token, value);
}

void IRParser::executePendingUpdates(Token const& name, Value* value) {
    auto& map = [&]() -> auto& {
        switch (name.kind()) {
        case TokenKind::GlobalIdentifier:
            return globalPendingUpdates;
        case TokenKind::LocalIdentifier:
            return localPendingUpdates;
        default:
            SC_UNREACHABLE();
        }
    }();
    auto const itr = map.find(name.id());
    if (itr == map.end()) {
        return;
    }
    auto& updateList = itr->second;
    for (auto& updateFn: updateList) {
        updateFn(value);
    }
    map.erase(itr);
}

void IRParser::postProcess() {
    for (auto& function: mod) {
        for (auto& BB: function) {
            for (auto& phi: BB.phiNodes()) {
                auto sortedArgs =
                    BB.predecessors() |
                    ranges::views::transform([&](BasicBlock* pred) {
                    return PhiMapping{ pred, phi.operandOf(pred) };
                }) | ToSmallVector<>;
                phi.setArguments(sortedArgs);
            }
        }
    }
}
