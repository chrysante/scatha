#include "IR/IRParser.h"

#include <array>
#include <functional>

#include <range/v3/algorithm.hpp>
#include <svm/Builtin.h>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/APInt.h"
#include "Common/EscapeSequence.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/InvariantSetup.h"
#include "IR/Module.h"
#include "IR/Parser/IRLexer.h"
#include "IR/Parser/IRToken.h"
#include "IR/Type.h"
#include "IR/Validate.h"

using namespace scatha;
using namespace ir;

static APInt parseInt(Token token, Type const* type) {
    SC_EXPECT(token.kind() == TokenKind::IntLiteral);
    auto value = APInt::parse(token.id(),
                              10,
                              cast<IntegralType const*>(type)->bitwidth());
    if (!value) {
        throw SyntaxIssue(token);
    }
    return *value;
}

static APFloat parseFloat(Token token, Type const* type) {
    SC_EXPECT(token.kind() == TokenKind::FloatLiteral);
    size_t bitwidth = cast<FloatType const*>(type)->bitwidth();
    SC_EXPECT(bitwidth == 32 || bitwidth == 64);
    auto value = APFloat::parse(token.id(),
                                bitwidth == 32 ? APFloatPrec::Single :
                                                 APFloatPrec::Double);
    if (!value) {
        throw SyntaxIssue(token);
    }
    return *value;
}

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

struct ParseContext {
    using ValueMap = utl::hashmap<std::string, Value*>;
    struct PendingUpdate: std::function<void(Value*)> {
        using Base = std::function<void(Value*)>;
        PendingUpdate(Token token, Base base):
            Base(std::move(base)), token(token) {}

        Token token;
    };
    using PUMap = utl::hashmap<std::string, utl::small_vector<PendingUpdate>>;

    Context& irCtx;
    Module& mod;
    Lexer lexer;
    std::optional<Token> nextToken[2];
    ValueMap globals;
    ValueMap locals;
    PUMap globalPendingUpdates;
    PUMap localPendingUpdates;

    explicit ParseContext(Context& irCtx, Module& mod, std::string_view text):
        irCtx(irCtx),
        mod(mod),
        lexer(text),
        nextToken{ lexer.next().value(), lexer.next().value() } {}

    void parse();

    UniquePtr<Callable> parseCallable();
    UniquePtr<Parameter> parseParamDecl(size_t index);
    UniquePtr<ForeignFunction> makeForeignFunction(
        Type const* returnType,
        utl::small_vector<Parameter*> params,
        Token name);
    UniquePtr<BasicBlock> parseBasicBlock();
    UniquePtr<Instruction> parseInstruction();
    template <typename T>
    UniquePtr<Instruction> parseArithmeticConversion(std::string name);
    utl::small_vector<size_t> parseConstantIndices();
    UniquePtr<StructType> parseStructure();
    UniquePtr<GlobalVariable> parseGlobal();
    void parseTypeDefinition();

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

    void checkSelfRef(auto* user, OptValue optVal) const {
        if (optVal.value()) {
            return;
        }
        if (isa<Phi>(user)) {
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
        reportSemaIssue(token, SemanticIssue::UseOfUndeclaredIdentifier);
    }

    template <typename V = Value, std::derived_from<User> U>
    void addValueLink(U* user, Type const* type, OptValue optVal, auto fn) {
        checkSelfRef(user, optVal);
        auto* value = [&] {
            if (optVal.value()) {
                return dyncast<V*>(optVal.value());
            }
            return getValue<V>(type, optVal.token());
        }();
        if (value) {
            if (type && value->type() && value->type() != type) {
                reportSemaIssue(optVal.token(), SemanticIssue::TypeMismatch);
            }
            std::invoke(fn, user, value);
            return;
        }
        _addPendingUpdate(optVal.token(), [=](Value* v) {
            SC_EXPECT(v);
            auto* value = dyncast<V*>(v);
            if (!value) {
                reportSemaIssue(optVal.token(), SemanticIssue::InvalidEntity);
            }
            std::invoke(fn, user, value);
        });
    }

    void executePendingUpdates(Token const& name, Value* value);

    Token eatToken() {
        Token result = peekToken();
        if (result.kind() != TokenKind::EndOfFile) {
            auto next = lexer.next();
            if (!next) {
                throw next.error();
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

    [[noreturn]] void reportSyntaxIssue(Token const& token) const {
        throw SyntaxIssue(token);
    }

    [[noreturn]] void reportSemaIssue(Token const& token,
                                      SemanticIssue::Reason reason) const {
        throw SemanticIssue(token, reason);
    }

    void expectAny(Token const& token,
                   std::same_as<TokenKind> auto... kind) const {
        if (((token.kind() != kind) && ...)) {
            reportSyntaxIssue(token);
        }
    }

    void expect(Token const& token, TokenKind kind) const {
        expectAny(token, kind);
    }

    void expectNext(std::same_as<TokenKind> auto... next) {
        (expect(eatToken(), next), ...);
    }

    Conversion toConversion(Token token) const {
        switch (token.kind()) {
#define SC_CONVERSION_DEF(Op, Keyword)                                         \
    case TokenKind::Op:                                                        \
        return Conversion::Op;
#include "IR/Lists.def"
        default:
            reportSyntaxIssue(token);
        }
    }

    CompareMode toCompareMode(Token token) const {
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

    CompareOperation toCompareOp(Token token) const {
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

    UnaryArithmeticOperation toUnaryArithmeticOp(Token token) const {
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

    ArithmeticOperation toArithmeticOp(Token token) const {
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

    void checkEmpty(PUMap const& updates) const {
        /// We expect `globalPendingUpdates` to be empty when parsed
        /// successfully.
        for (auto&& [name, list]: updates) {
            for (auto&& update: list) {
                reportSemaIssue(update.token,
                                SemanticIssue::UseOfUndeclaredIdentifier);
            }
        }
    }
};

} // namespace

Expected<std::pair<Context, Module>, ParseIssue> ir::parse(
    std::string_view text) {
    try {
        Context irCtx;
        Module mod;
        ParseContext parseContext(irCtx, mod, text);
        parseContext.parse();
        assertInvariants(irCtx, mod);
        return { std::move(irCtx), std::move(mod) };
    }
    catch (LexicalIssue const& issue) {
        return issue;
    }
    catch (SyntaxIssue const& issue) {
        return issue;
    }
    catch (SemanticIssue const& issue) {
        return issue;
    }
}

void ParseContext::parse() {
    while (peekToken().kind() != TokenKind::EndOfFile) {
        if (auto s = parseStructure()) {
            mod.addStructure(std::move(s));
            continue;
        }
        if (auto c = parseGlobal()) {
            mod.addGlobal(std::move(c));
            continue;
        }
        if (auto fn = parseCallable()) {
            mod.addGlobal(std::move(fn));
            continue;
        }
        throw SyntaxIssue(peekToken());
    }
    checkEmpty(globalPendingUpdates);
}

UniquePtr<Callable> ParseContext::parseCallable() {
    locals.clear();
    bool const isExt = peekToken().kind() == TokenKind::Ext;
    if (isExt) {
        eatToken();
    }
    Token const declarator = peekToken();
    if (isExt) {
        expect(declarator, TokenKind::Function);
    }
    if (declarator.kind() != TokenKind::Function) {
        return nullptr;
    }
    eatToken();
    auto* const returnType = parseType();
    Token const name = eatToken();
    expect(eatToken(), TokenKind::OpenParan);
    utl::small_vector<Parameter*> parameters;
    size_t index = 0;
    if (peekToken().kind() != TokenKind::CloseParan) {
        parameters.push_back(parseParamDecl(index++).release());
    }
    while (true) {
        if (peekToken().kind() != TokenKind::Comma) {
            break;
        }
        eatToken(); // Comma
        parameters.push_back(parseParamDecl(index++).release());
    }
    expect(eatToken(), TokenKind::CloseParan);
    if (isExt) {
        auto function = makeForeignFunction(returnType, parameters, name);
        registerValue(name, function.get());
        return function;
    }
    auto function =
        allocate<Function>(irCtx,
                           returnType,
                           parameters,
                           std::string(name.id()),
                           FunctionAttribute::None,
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
    setupInvariants(irCtx, *function);
    return function;
}

UniquePtr<Parameter> ParseContext::parseParamDecl(size_t index) {
    auto* type = parseType();
    UniquePtr<Parameter> result;
    if (peekToken().kind() == TokenKind::LocalIdentifier) {
        result = allocate<Parameter>(type,
                                     index,
                                     std::string(eatToken().id()),
                                     nullptr);
    }
    else {
        result = allocate<Parameter>(type, index, nullptr);
    }
    locals[std::string(result->name())] = result.get();
    return result;
}

static std::optional<uint32_t> builtinIndex(std::string_view name) {
    SC_EXPECT(name.starts_with("__builtin_"));
    name.remove_prefix(std::string_view("__builtin_").size());
    for (uint32_t index = 0;
         index < static_cast<uint32_t>(svm::Builtin::_count);
         ++index)
    {
        svm::Builtin builtin = static_cast<svm::Builtin>(index);
        std::string_view builtinName = toString(builtin);
        if (builtinName == name) {
            return index;
        }
    }
    return std::nullopt;
}

UniquePtr<ForeignFunction> ParseContext::makeForeignFunction(
    Type const* returnType, utl::small_vector<Parameter*> params, Token name) {
    if (name.id().starts_with("__builtin_")) {
        if (auto index = builtinIndex(name.id())) {
            return allocate<ForeignFunction>(irCtx,
                                             returnType,
                                             params,
                                             std::string(name.id()),
                                             svm::BuiltinFunctionSlot,
                                             *index,
                                             FunctionAttribute::None);
        }
    }
    reportSemaIssue(name, SemanticIssue::InvalidEntity);
}

UniquePtr<BasicBlock> ParseContext::parseBasicBlock() {
    if (peekToken().kind() != TokenKind::LocalIdentifier) {
        return nullptr;
    }
    Token const name = eatToken();
    expect(eatToken(), TokenKind::Colon);
    auto result = allocate<BasicBlock>(irCtx, std::string(name.id()));
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
        result->pushBack(std::move(instruction));
    }
    return result;
}

UniquePtr<Instruction> ParseContext::parseInstruction() {
    auto _nameTok = peekToken(0);
    auto _name = [&]() -> std::optional<std::string> {
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
            throw SyntaxIssue(_nameTok);
        }
        return *_name;
    };
    auto nameOrEmpty = [&] { return _name.value_or(std::string{}); };
    switch (peekToken().kind()) {
    case TokenKind::Alloca: {
        eatToken();
        auto* type = parseType();
        auto result = allocate<Alloca>(irCtx, type, name());
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
        addValueLink(result.get(), irCtx.ptrType(), ptr, &Load::setAddress);
        return result;
    }
    case TokenKind::Store: {
        eatToken();
        auto* ptrType = parseType();
        auto const addr = parseValue(ptrType);
        expect(eatToken(), TokenKind::Comma);
        auto* valueType = parseType();
        auto const value = parseValue(valueType);
        auto result = allocate<Store>(irCtx, nullptr, nullptr);
        addValueLink(result.get(), irCtx.ptrType(), addr, &Store::setAddress);
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
        addValueLink(result.get(),
                     valueType,
                     value,
                     &ConversionInst::setOperand);
        return result;
    }
    case TokenKind::Goto: {
        eatToken();
        expect(eatToken(), TokenKind::Label);
        auto targetName = eatToken();
        expect(targetName, TokenKind::LocalIdentifier);
        auto result = allocate<Goto>(irCtx, nullptr);
        addValueLink<BasicBlock>(result.get(),
                                 nullptr,
                                 targetName,
                                 &Goto::setTarget);
        return result;
    }
    case TokenKind::Branch: {
        eatToken();
        auto condTypeName = peekToken();
        auto* condType = parseType();
        if (condType != irCtx.intType(1)) {
            reportSemaIssue(condTypeName, SemanticIssue::InvalidType);
        }
        auto cond = parseValue(condType);
        expectNext(TokenKind::Comma, TokenKind::Label);
        auto thenName = eatToken();
        expectNext(TokenKind::Comma, TokenKind::Label);
        auto elseName = eatToken();
        auto result = allocate<Branch>(irCtx, nullptr, nullptr, nullptr);
        addValueLink(result.get(),
                     irCtx.intType(1),
                     cond,
                     &Branch::setCondition);
        addValueLink<BasicBlock>(result.get(),
                                 irCtx.voidType(),
                                 thenName,
                                 &Branch::setThenTarget);
        addValueLink<BasicBlock>(result.get(),
                                 irCtx.voidType(),
                                 elseName,
                                 &Branch::setElseTarget);
        return result;
    }
    case TokenKind::Return: {
        eatToken();
        auto result = allocate<Return>(irCtx, nullptr);
        auto* valueType = tryParseType();
        if (!valueType) {
            result->setValue(irCtx.voidValue());
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
        addValueLink(result.get(),
                     nullptr,
                     funcName,
                     [=](Call* call, Value* value) {
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
            addValueLink(result.get(),
                         type,
                         name,
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
            addValueLink<BasicBlock>(result.get(),
                                     irCtx.voidType(),
                                     predName,
                                     [index = index](Phi* phi,
                                                     BasicBlock* pred) {
                phi->setPredecessor(index, pred);
            });
            addValueLink(result.get(),
                         type,
                         value,
                         [index = index](Phi* phi, Value* value) {
                phi->setArgument(index, value);
            });
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
            allocate<CompareInst>(irCtx, nullptr, nullptr, mode, op, name());
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
        auto result = allocate<UnaryArithmeticInst>(irCtx, nullptr, op, name());
        addValueLink(result.get(), valueType, value, [&](auto* inst, auto* op) {
            inst->setOperand(irCtx, op);
        });
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
        auto result = allocate<GetElementPointer>(irCtx,
                                                  accessedType,
                                                  nullptr,
                                                  nullptr,
                                                  indices,
                                                  name());
        addValueLink(result.get(),
                     irCtx.ptrType(),
                     basePtr,
                     &GetElementPointer::setBasePtr);
        addValueLink(result.get(),
                     indexType,
                     index,
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
        addValueLink(result.get(),
                     insType,
                     ins,
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
        if (condType != irCtx.intType(1)) {
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

utl::small_vector<size_t> ParseContext::parseConstantIndices() {
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

UniquePtr<StructType> ParseContext::parseStructure() {
    if (peekToken().kind() != TokenKind::Structure) {
        return nullptr;
    }
    eatToken();
    Token const nameID = eatToken();
    expect(nameID, TokenKind::GlobalIdentifier);
    expect(eatToken(), TokenKind::OpenBrace);
    utl::small_vector<Type const*> members;
    while (true) {
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

UniquePtr<GlobalVariable> ParseContext::parseGlobal() {
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
            reportSemaIssue(kind, SemanticIssue::ExpectedGlobalKind);
        }
    }();
    auto* type = parseType();
    auto value = parseValue(type);
    auto global =
        allocate<GlobalVariable>(irCtx, mut, nullptr, std::string(name.id()));
    addValueLink<Constant>(global.get(),
                           type,
                           value,
                           &GlobalVariable::setInitializer);
    bool success =
        globals.insert({ std::string(name.id()), global.get() }).second;
    if (!success) {
        reportSemaIssue(name, SemanticIssue::Redeclaration);
    }
    return global;
}

Type const* ParseContext::tryParseType() {
    Token const token = peekToken();
    switch (token.kind()) {
    case TokenKind::Void:
        eatToken();
        return irCtx.voidType();
    case TokenKind::Ptr:
        eatToken();
        return irCtx.ptrType();
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
    case TokenKind::LocalIdentifier:
        eatToken();
        reportSemaIssue(token, SemanticIssue::UnexpectedID);
    case TokenKind::IntType:
        eatToken();
        return irCtx.intType(token.width());
    case TokenKind::FloatType:
        eatToken();
        return irCtx.floatType(token.width());
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
                return irCtx.anonymousStruct(members);
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
        return irCtx.arrayType(type, count);
    }
    default:
        return nullptr;
    }
}

Type const* ParseContext::parseType() {
    auto* type = tryParseType();
    if (type) {
        return type;
    }
    reportSemaIssue(peekToken(), SemanticIssue::ExpectedType);
}

OptValue ParseContext::parseValue(Type const* type) {
    auto token = eatToken();
    switch (token.kind()) {
    case TokenKind::LocalIdentifier:
        [[fallthrough]];
    case TokenKind::GlobalIdentifier:
        return token;
    case TokenKind::NullLiteral:
        return { token, irCtx.nullpointer() };
    case TokenKind::UndefLiteral:
        return { token, irCtx.undef(type) };
    case TokenKind::IntLiteral: {
        auto* intType = dyncast<IntegralType const*>(type);
        if (!intType) {
            reportSemaIssue(token, SemanticIssue::InvalidType);
        }
        auto value = parseInt(token, intType);
        return { token, irCtx.intConstant(value) };
    }
    case TokenKind::FloatLiteral: {
        auto* floatType = dyncast<FloatType const*>(type);
        if (!floatType) {
            reportSemaIssue(token, SemanticIssue::InvalidType);
        }
        auto value = parseFloat(token, floatType);
        return { token, irCtx.floatConstant(value) };
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
            irCtx.recordConstant(utl::small_vector<Constant*>(elems.size()),
                                 recordType);
        for (auto [index, elem]: elems | ranges::views::enumerate) {
            addValueLink<Constant>(aggrValue,
                                   recordType->elementAt(index),
                                   elem,
                                   [index = index](User* u, Constant* c) {
                u->setOperand(index, c);
            });
        }
        return { token, aggrValue };
    }
    case TokenKind::StringLiteral: {
        auto text = toEscapedValue(token.id());
        auto elems = text | ranges::views::transform([&](char c) -> Constant* {
                         return irCtx.intConstant(static_cast<unsigned>(c), 8);
                     }) |
                     ToSmallVector<>;
        auto* arrayType = dyncast<ArrayType const*>(type);
        if (!arrayType || arrayType->elementType() != irCtx.intType(8) ||
            arrayType->count() != elems.size())
        {
            reportSemaIssue(token, SemanticIssue::TypeMismatch);
        }
        return { token, irCtx.arrayConstant(elems, arrayType) };
    }
    default:
        reportSemaIssue(token, SemanticIssue::UnexpectedID);
    }
}

template <typename V>
V* ParseContext::getValue(Type const* type, Token const& token) {
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
        reportSemaIssue(token, SemanticIssue::UnexpectedID);
    }
}

size_t ParseContext::getIntLiteral(Token const& token) {
    if (token.kind() != TokenKind::IntLiteral) {
        reportSyntaxIssue(token);
    }
    return utl::narrow_cast<size_t>(std::atoll(token.id().data()));
}

void ParseContext::registerValue(Token const& token, Value* value) {
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
            SC_UNREACHABLE();
        }
    }();
    if (values.contains(value->name())) {
        reportSemaIssue(token, SemanticIssue::Redeclaration);
    }
    values[value->name()] = value;
    executePendingUpdates(token, value);
}

void ParseContext::executePendingUpdates(Token const& name, Value* value) {
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
