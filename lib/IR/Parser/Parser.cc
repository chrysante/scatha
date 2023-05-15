#include "IR/Parser.h"

#include <array>
#include <functional>

#include <range/v3/algorithm.hpp>
#include <svm/Builtin.h>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/APInt.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Lexer.h"
#include "IR/Parser/Token.h"
#include "IR/Type.h"
#include "IR/Validate.h"

using namespace scatha;
using namespace ir;

static APInt parseInt(Token token, Type const* type) {
    SC_ASSERT(token.kind() == TokenKind::IntLiteral, "");
    auto value = APInt::parse(token.id(),
                              10,
                              cast<IntegralType const*>(type)->bitwidth());
    if (!value) {
        throw SyntaxIssue(token);
    }
    return *value;
}

static APFloat parseFloat(Token token, Type const* type) {
    SC_ASSERT(token.kind() == TokenKind::FloatLiteral, "");
    size_t bitwidth = cast<FloatType const*>(type)->bitwidth();
    SC_ASSERT(bitwidth == 32 || bitwidth == 64, "");
    auto value = APFloat::parse(token.id(),
                                bitwidth == 32 ? APFloatPrec::Single :
                                                 APFloatPrec::Double);
    if (!value) {
        throw SyntaxIssue(token);
    }
    return *value;
}

namespace {

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
    UniquePtr<ExtFunction> makeExtFunction(Type const* returnType,
                                           utl::small_vector<Parameter*> params,
                                           Token name);
    UniquePtr<BasicBlock> parseBasicBlock();
    UniquePtr<Instruction> parseInstruction();
    template <typename T>
    UniquePtr<Instruction> parseArithmeticConversion(std::string name);
    utl::small_vector<size_t> parseConstantIndices();
    UniquePtr<StructureType> parseStructure();
    UniquePtr<ConstantData> parseConstant();
    void parseConstantData(Type const* type, utl::vector<u8>& data);

    void parseTypeDefinition();

    /// Try to parse a type, return `nullptr` if no type could be parsed
    Type const* tryParseType();

    /// Parse a type, report issue if no type could be parsed
    Type const* parseType();

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

    template <typename V = Value, std::derived_from<User> U>
    void addValueLink(U* user, Type const* type, Token token, auto fn) {
        if ((token.kind() == TokenKind::LocalIdentifier ||
             token.kind() == TokenKind::GlobalIdentifier) &&
            user->name() == token.id() &&
            !isa<Phi>(user)) /// Hack to avoid errors for phi functions. However
                             /// to be thorough we would have to test the
                             /// operand is also a phi node
        {
            /// We report self references as use of undeclared identifier
            /// because the identifier is not defined before the next
            /// declaration.
            reportSemaIssue(token, SemanticIssue::UseOfUndeclaredIdentifier);
        }
        auto* value = getValue<V>(type, token);
        if (value) {
            if (type && value->type() && value->type() != type) {
                reportSemaIssue(token, SemanticIssue::TypeMismatch);
            }
            std::invoke(fn, user, value);
            return;
        }
        _addPendingUpdate(token, [=](Value* v) {
            SC_ASSERT(v, "");
            auto* value = dyncast<V*>(v);
            if (!value) {
                reportSemaIssue(token, SemanticIssue::InvalidEntity);
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
        SC_ASSERT(i < 2, "We only have a look-ahead of 2");
        SC_ASSERT(nextToken[i].has_value(), "");
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
        ParseContext ctx(irCtx, mod, text);
        ctx.parse();
        setupInvariants(irCtx, mod);
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
        if (auto c = parseConstant()) {
            mod.addConstantData(std::move(c));
            continue;
        }
        if (auto fn = parseCallable()) {
            if (auto* nativeFunc = dyncast<Function*>(fn.get())) {
                fn.release();
                mod.addFunction(UniquePtr<Function>(nativeFunc));
            }
            else {
                SC_ASSERT(isa<ExtFunction>(*fn), "");
                mod.addGlobal(std::move(fn));
            }
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
    Token const name       = eatToken();
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
        auto result = makeExtFunction(returnType, parameters, name);
        registerValue(name, result.get());
        return result;
    }
    auto result = allocate<Function>(nullptr,
                                     returnType,
                                     parameters,
                                     std::string(name.id()),
                                     FunctionAttribute::None,
                                     Visibility::Extern); // FIXME: Parse
                                                          // function visibility
    registerValue(name, result.get());
    expect(eatToken(), TokenKind::OpenBrace);
    /// Parse the body of the function.
    while (true) {
        auto basicBlock = parseBasicBlock();
        if (!basicBlock) {
            break;
        }
        result->pushBack(std::move(basicBlock));
    }
    expect(eatToken(), TokenKind::CloseBrace);
    checkEmpty(localPendingUpdates);
    return result;
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
    SC_ASSERT(name.starts_with("__builtin_"), "");
    name.remove_prefix(std::string_view("__builtin_").size());
    for (uint32_t index = 0;
         index < static_cast<uint32_t>(svm::Builtin::_count);
         ++index)
    {
        svm::Builtin builtin         = static_cast<svm::Builtin>(index);
        std::string_view builtinName = toString(builtin);
        if (builtinName == name) {
            return index;
        }
    }
    return std::nullopt;
}

UniquePtr<ExtFunction> ParseContext::makeExtFunction(
    Type const* returnType, utl::small_vector<Parameter*> params, Token name) {
    if (name.id().starts_with("__builtin_")) {
        if (auto index = builtinIndex(name.id())) {
            return allocate<ExtFunction>(nullptr,
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
    auto _name    = [&]() -> std::optional<std::string> {
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
        auto* type  = parseType();
        auto result = allocate<Alloca>(irCtx, type, name());
        if (peekToken().kind() != TokenKind::Comma) {
            return result;
        }
        eatToken();
        auto* countType = parseType();
        auto countToken = eatToken();
        addValueLink(result.get(), countType, countToken, &Alloca::setCount);
        return result;
    }
    case TokenKind::Load: {
        eatToken();
        auto* const type = parseType();
        expect(eatToken(), TokenKind::Comma);
        expect(eatToken(), TokenKind::Ptr);
        expectAny(peekToken(),
                  TokenKind::LocalIdentifier,
                  TokenKind::GlobalIdentifier);
        auto const ptrName = eatToken();
        auto result        = allocate<Load>(nullptr, type, name());
        addValueLink(result.get(),
                     irCtx.pointerType(),
                     ptrName,
                     &Load::setAddress);
        return result;
    }
    case TokenKind::Store: {
        eatToken();
        expect(eatToken(), TokenKind::Ptr);
        auto const addrName = eatToken();
        expect(eatToken(), TokenKind::Comma);
        auto* valueType      = parseType();
        auto const valueName = eatToken();
        auto result          = allocate<Store>(irCtx, nullptr, nullptr);
        addValueLink(result.get(),
                     irCtx.pointerType(),
                     addrName,
                     &Store::setAddress);
        addValueLink(result.get(), valueType, valueName, &Store::setValue);
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
        auto conv       = toConversion(eatToken());
        auto* valueType = parseType();
        auto valueName  = eatToken();
        expect(eatToken(), TokenKind::To);
        auto* targetType = parseType();
        auto result =
            allocate<ConversionInst>(nullptr, targetType, conv, name());
        addValueLink(result.get(),
                     valueType,
                     valueName,
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
        auto* condType    = parseType();
        if (condType != irCtx.integralType(1)) {
            reportSemaIssue(condTypeName, SemanticIssue::InvalidType);
        }
        auto condName = eatToken();
        expectNext(TokenKind::Comma, TokenKind::Label);
        auto thenName = eatToken();
        expectNext(TokenKind::Comma, TokenKind::Label);
        auto elseName = eatToken();
        auto result   = allocate<Branch>(irCtx, nullptr, nullptr, nullptr);
        addValueLink(result.get(),
                     irCtx.integralType(1),
                     condName,
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
        auto result     = allocate<Return>(irCtx, nullptr);
        auto* valueType = tryParseType();
        if (!valueType) {
            result->setValue(irCtx.voidValue());
            return result;
        }
        auto valueName = eatToken();
        addValueLink(result.get(), valueType, valueName, &Return::setValue);
        return result;
    }
    case TokenKind::Call: {
        eatToken();
        auto retTypeName = peekToken();
        auto* retType    = parseType();
        auto funcName    = eatToken();
        using CallArg    = std::pair<Type const*, Token>;
        utl::small_vector<CallArg> args;
        while (true) {
            if (peekToken().kind() != TokenKind::Comma) {
                break;
            }
            eatToken();
            auto* argType = parseType();
            auto argName  = eatToken();
            args.push_back({ argType, argName });
        }
        utl::small_vector<Value*> nullArgs(args.size());
        auto result = allocate<Call>(nullptr, nullArgs, nameOrEmpty());
        addValueLink<Callable>(result.get(),
                               nullptr,
                               funcName,
                               [=](Call* call, Callable* func) {
            if (retType != func->returnType()) {
                reportSemaIssue(retTypeName, SemanticIssue::TypeMismatch);
            }
            call->setFunction(func);
        });
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
        auto* type   = parseType();
        using PhiArg = std::array<Token, 2>;
        utl::small_vector<PhiArg> args;
        while (true) {
            expectNext(TokenKind::OpenBracket, TokenKind::Label);
            auto predName = eatToken();
            expect(eatToken(), TokenKind::Colon);
            auto valueName = eatToken();
            expect(eatToken(), TokenKind::CloseBracket);
            args.push_back({ predName, valueName });
            if (peekToken().kind() != TokenKind::Comma) {
                break;
            }
            eatToken();
        }
        auto result = allocate<Phi>(type, args.size(), name());
        registerValue(_nameTok, result.get());
        for (auto [index, arg]: args | ranges::views::enumerate) {
            auto [predName, valueName] = arg;
            addValueLink<BasicBlock>(result.get(),
                                     irCtx.voidType(),
                                     predName,
                                     [index = index](Phi* phi,
                                                     BasicBlock* pred) {
                phi->setPredecessor(index, pred);
            });
            addValueLink(result.get(),
                         type,
                         valueName,
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
        auto mode     = toCompareMode(eatToken());
        auto op       = toCompareOp(eatToken());
        auto* lhsType = parseType();
        auto lhsName  = eatToken();
        expect(eatToken(), TokenKind::Comma);
        auto* rhsType = parseType();
        auto rhsName  = eatToken();
        auto result =
            allocate<CompareInst>(irCtx, nullptr, nullptr, mode, op, name());
        addValueLink(result.get(), lhsType, lhsName, &CompareInst::setLHS);
        addValueLink(result.get(), rhsType, rhsName, &CompareInst::setRHS);
        return result;
    }
    case TokenKind::Bnt:
        [[fallthrough]];
    case TokenKind::Lnt: {
        auto op         = toUnaryArithmeticOp(eatToken());
        auto* valueType = parseType();
        auto valueName  = eatToken();
        auto result = allocate<UnaryArithmeticInst>(irCtx, nullptr, op, name());
        addValueLink(result.get(),
                     valueType,
                     valueName,
                     &UnaryArithmeticInst::setOperand);
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
        auto op       = toArithmeticOp(eatToken());
        auto* lhsType = parseType();
        auto lhsName  = eatToken();
        expect(eatToken(), TokenKind::Comma);
        auto* rhsType = parseType();
        auto rhsName  = eatToken();
        auto result   = allocate<ArithmeticInst>(nullptr, nullptr, op, name());
        addValueLink(result.get(), lhsType, lhsName, &ArithmeticInst::setLHS);
        addValueLink(result.get(), rhsType, rhsName, &ArithmeticInst::setRHS);
        return result;
    }
    case TokenKind::GetElementPointer: {
        eatToken();
        expectNext(TokenKind::Inbounds);
        auto* accessedType = parseType();
        expectNext(TokenKind::Comma, TokenKind::Ptr);
        auto basePtrName = eatToken();
        expectNext(TokenKind::Comma);
        auto* indexType = parseType();
        auto indexName  = eatToken();
        auto indices    = parseConstantIndices();
        auto result     = allocate<GetElementPointer>(irCtx,
                                                  accessedType,
                                                  nullptr,
                                                  nullptr,
                                                  indices,
                                                  name());
        addValueLink(result.get(),
                     irCtx.pointerType(),
                     basePtrName,
                     &GetElementPointer::setBasePtr);
        addValueLink(result.get(),
                     indexType,
                     indexName,
                     &GetElementPointer::setArrayIndex);
        return result;
    }
    case TokenKind::InsertValue: {
        eatToken();
        auto* baseType = parseType();
        auto baseName  = eatToken();
        expectNext(TokenKind::Comma);
        auto* insType = parseType();
        auto insName  = eatToken();
        auto indices  = parseConstantIndices();
        if (indices.empty()) {
            reportSyntaxIssue(peekToken());
        }
        auto result = allocate<InsertValue>(nullptr, nullptr, indices, name());
        addValueLink(result.get(),
                     baseType,
                     baseName,
                     &InsertValue::setBaseValue);
        addValueLink(result.get(),
                     insType,
                     insName,
                     &InsertValue::setInsertedValue);
        return result;
    }
    case TokenKind::ExtractValue: {
        eatToken();
        auto* baseType = parseType();
        auto baseName  = eatToken();
        auto indices   = parseConstantIndices();
        if (indices.empty()) {
            reportSyntaxIssue(peekToken());
        }
        auto result = allocate<ExtractValue>(nullptr, indices, name());
        addValueLink(result.get(),
                     baseType,
                     baseName,
                     &ExtractValue::setBaseValue);
        return result;
    }
    case TokenKind::Select: {
        eatToken();
        auto condTypeName = peekToken();
        auto* condType    = parseType();
        if (condType != irCtx.integralType(1)) {
            reportSemaIssue(condTypeName, SemanticIssue::InvalidType);
        }
        auto condName = eatToken();
        expectNext(TokenKind::Comma);
        auto* thenType = parseType();
        auto thenName  = eatToken();
        expectNext(TokenKind::Comma);
        auto* elseType = parseType();
        auto elseName  = eatToken();
        auto result    = allocate<Select>(nullptr, nullptr, nullptr, name());
        addValueLink(result.get(),
                     irCtx.integralType(1),
                     condName,
                     &Select::setCondition);
        addValueLink(result.get(), thenType, thenName, &Select::setThenValue);
        addValueLink(result.get(), elseType, elseName, &Select::setElseValue);
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

UniquePtr<StructureType> ParseContext::parseStructure() {
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
    auto structure = allocate<StructureType>(std::string(nameID.id()), members);
    return structure;
}

UniquePtr<ConstantData> ParseContext::parseConstant() {
    Token const name = peekToken();
    if (name.kind() != TokenKind::GlobalIdentifier) {
        return nullptr;
    }
    eatToken();
    expectNext(TokenKind::Assign);
    auto* type = parseType();
    utl::small_vector<u8> data;
    parseConstantData(type, data);
    auto result =
        allocate<ConstantData>(irCtx, type, data, std::string(name.id()));
    bool success =
        globals.insert({ std::string(name.id()), result.get() }).second;
    if (!success) {
        reportSemaIssue(name, SemanticIssue::Redeclaration);
    }
    return result;
}

void ParseContext::parseConstantData(Type const* type, utl::vector<u8>& data) {
    using enum SemanticIssue::Reason;
    // clang-format off
    visit(*type, utl::overload{
        [&](StructureType const& type) { SC_UNIMPLEMENTED(); },
        [&](ArrayType const& type) {
            expect(eatToken(), TokenKind::OpenBracket);
            for (size_t i = 0; i < type.count(); ++i) {
                if (i != 0) {
                    expect(eatToken(), TokenKind::Comma);
                }
                Token typeTok  = peekToken();
                auto* elemType = parseType();
                if (elemType != type.elementType()) {
                    reportSemaIssue(typeTok, InvalidType);
                }
                parseConstantData(elemType, data);
            }
            expect(eatToken(), TokenKind::CloseBracket);
        },
        [&](IntegralType const& type) {
            size_t const bitwidth = type.bitwidth();
            SC_ASSERT(bitwidth % 8 == 0, "");
            Token const token = eatToken();
            expect(token, TokenKind::IntLiteral);
            auto value = parseInt(token, &type);
            auto* ptr  = reinterpret_cast<u8 const*>(value.limbs().data());
            for (size_t i = 0; i < bitwidth / 8; ++i, ++ptr) {
                data.push_back(*ptr);
            }
        },
        [&](FloatType const& type) {
            Token const token = eatToken();
            expect(token, TokenKind::FloatLiteral);
            auto value = parseFloat(token, &type);
            if (value.precision() == APFloatPrec::Single) {
                float f    = value.to<float>();
                auto bytes = std::bit_cast<std::array<u8, 4>>(f);
                for (u8 b: bytes) {
                    data.push_back(b);
                }
            }
            else {
                double d   = value.to<double>();
                auto bytes = std::bit_cast<std::array<u8, 8>>(d);
                for (u8 b: bytes) {
                    data.push_back(b);
                }
            }
        },
        [&](Type const& type) { reportSemaIssue(peekToken(), UnexpectedID); },
    }); // clang-format off
}

Type const* ParseContext::tryParseType() {
    Token const token = peekToken();
    switch (token.kind()) {
    case TokenKind::Void:
        eatToken();
        return irCtx.voidType();
    case TokenKind::Ptr:
        eatToken();
        return irCtx.pointerType();
    case TokenKind::GlobalIdentifier: {
        eatToken();
        auto structures = mod.structures();
        auto itr        = ranges::find_if(structures, [&](auto&& type) {
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
        return irCtx.integralType(token.width());
    case TokenKind::FloatType:
        eatToken();
        return irCtx.floatType(token.width());
    case TokenKind::OpenBrace: {
        eatToken();
        utl::small_vector<Type const*> members;
        while (true) {
            Token const typeTok = peekToken();
            auto* type          = parseType();
            if (!type) {
                reportSemaIssue(typeTok, SemanticIssue::UnexpectedID);
            }
            members.push_back(type);
            if (peekToken().kind() == TokenKind::CloseBrace) {
                eatToken();
                return irCtx.anonymousStructure(members);
            }
            expect(eatToken(), TokenKind::Comma);
        }
    }
    case TokenKind::OpenBracket: {
        eatToken();
        Token const typeTok = peekToken();
        auto* type          = parseType();
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

template <typename V>
V* ParseContext::getValue(Type const* type, Token const& token) {
    switch (token.kind()) {
    case TokenKind::LocalIdentifier:
        [[fallthrough]];
    case TokenKind::GlobalIdentifier: {
        auto& values =
            token.kind() == TokenKind::LocalIdentifier ? locals : globals;
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
    case TokenKind::UndefLiteral:
        if constexpr (std::convertible_to<V*, BasicBlock*> ||
                      std::convertible_to<V*, Callable*>)
        {
            reportSyntaxIssue(token);
        }
        else {
            return irCtx.undef(type);
        }
    case TokenKind::IntLiteral:
        if constexpr (std::convertible_to<IntegralConstant*, V*>) {
            auto* intType = dyncast<IntegralType const*>(type);
            if (!intType) {
                reportSemaIssue(token, SemanticIssue::InvalidType);
            }
            auto value = parseInt(token, intType);
            return irCtx.integralConstant(value);
        }
        else {
            SC_UNREACHABLE();
        }
    case TokenKind::FloatLiteral: {
        if constexpr (std::convertible_to<FloatingPointConstant*, V*>) {
            auto* floatType = dyncast<FloatType const*>(type);
            if (!floatType) {
                reportSemaIssue(token, SemanticIssue::InvalidType);
            }
            auto value = parseFloat(token, floatType);
            return irCtx.floatConstant(value, floatType->bitwidth());
        }
        else {
            SC_UNREACHABLE();
        }
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
