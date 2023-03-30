#include "IR/Parser.h"

#include <array>
#include <functional>

#include <range/v3/algorithm.hpp>
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

namespace {

struct ParseContext {
    explicit ParseContext(Context& irCtx, Module& mod, std::string_view text):
        irCtx(irCtx),
        mod(mod),
        lexer(text),
        nextToken{ lexer.next().value(), lexer.next().value() } {}

    void parse();

private:
    UniquePtr<Function> parseFunction();
    Type const* parseParamDecl();
    UniquePtr<BasicBlock> parseBasicBlock();
    UniquePtr<Instruction> parseInstruction();
    utl::small_vector<size_t> parseConstantIndices();
    UniquePtr<StructureType> parseStructure();

    void parseTypeDefinition();

    Type const* getType(Token const& token);

    template <typename V>
    V* getValue(Type const* type, Token const& token);

    void registerValue(Token const& token, Value* value);

    void _addPendingUpdate(Token const& name, std::function<void(Value*)> f) {
        switch (name.kind()) {
        case TokenKind::GlobalIdentifier:
            globalPendingUpdates[name.id()].push_back(std::move(f));
            break;
        case TokenKind::LocalIdentifier:
            localPendingUpdates[name.id()].push_back(std::move(f));
            break;
        default:
            SC_UNREACHABLE();
        }
    }

    template <typename V = Value, std::derived_from<User> U>
    void addValueLink(U* user, Type const* type, Token token, auto fn) {
        auto* value = getValue<V>(type, token);
        if (value) {
            if (type && value->type() != type) {
                reportSemaIssue();
            }
            std::invoke(fn, user, value);
            return;
        }
        _addPendingUpdate(token, [=](Value* v) {
            SC_ASSERT(v, "");
            auto* value = dyncast<V*>(v);
            if (!value) {
                reportSemaIssue();
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

    [[noreturn]] void reportSemaIssue() const { throw SemanticIssue(); }

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

    using ValueMap = utl::hashmap<std::string, Value*>;
    using PUMap    = utl::hashmap<std::string,
                               utl::small_vector<std::function<void(Value*)>>>;

    Context& irCtx;
    Module& mod;
    Lexer lexer;
    std::optional<Token> nextToken[2];
    ValueMap globals;
    ValueMap locals;
    PUMap globalPendingUpdates;
    PUMap localPendingUpdates;
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
        if (auto fn = parseFunction()) {
            mod.addFunction(std::move(fn));
            continue;
        }
        if (auto s = parseStructure()) {
            mod.addStructure(std::move(s));
            continue;
        }
        throw SyntaxIssue(peekToken());
    }
}

UniquePtr<Function> ParseContext::parseFunction() {
    Token const declarator = peekToken();
    if (declarator.kind() != TokenKind::Function) {
        return nullptr;
    }
    eatToken();
    auto* const returnType = getType(eatToken());
    Token const name       = eatToken();
    expect(eatToken(), TokenKind::OpenParan);
    utl::vector<Type const*> parameterTypes;
    if (peekToken().kind() != TokenKind::CloseParan) {
        parameterTypes.push_back(parseParamDecl());
    }
    while (true) {
        if (peekToken().kind() != TokenKind::Comma) {
            break;
        }
        eatToken(); // Comma
        parameterTypes.push_back(parseParamDecl());
    }
    expect(eatToken(), TokenKind::CloseParan);
    auto result = allocate<Function>(nullptr,
                                     returnType,
                                     parameterTypes,
                                     std::string(name.id()),
                                     FunctionAttribute::None);
    registerValue(name, result.get());
    expect(eatToken(), TokenKind::OpenBrace);
    /// Parse the body of the function.
    locals = result->parameters() |
             ranges::views::transform([i = 0](auto& p) mutable {
                 return std::pair{ std::to_string(i++), &p };
             }) |
             ranges::to<ValueMap>;
    while (true) {
        auto basicBlock = parseBasicBlock();
        if (!basicBlock) {
            break;
        }
        result->pushBack(std::move(basicBlock));
    }
    expect(eatToken(), TokenKind::CloseBrace);
    if (!localPendingUpdates.empty()) {
        reportSemaIssue(); // Use of undeclared identifier for each pending
                           // update.
    }
    return result;
}

Type const* ParseContext::parseParamDecl() {
    auto* const type = getType(peekToken());
    if (!type) {
        reportSyntaxIssue(peekToken());
    }
    eatToken();
    return type;
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
        registerValue(optInstName, instruction.get());
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
        return *std::move(_name);
    };
    auto nameOrEmpty = [&] { return _name.value_or(std::string{}); };
    switch (peekToken().kind()) {
    case TokenKind::Alloca: {
        eatToken();
        auto* const type = getType(eatToken());
        return allocate<Alloca>(irCtx, type, name());
    }
    case TokenKind::Load: {
        eatToken();
        auto* const type = getType(eatToken());
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
        auto* valueType      = getType(eatToken());
        auto const valueName = eatToken();
        auto result          = allocate<Store>(irCtx, nullptr, nullptr);
        addValueLink(result.get(),
                     irCtx.pointerType(),
                     addrName,
                     &Store::setAddress);
        addValueLink(result.get(), valueType, valueName, &Store::setValue);
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
        auto* condType = getType(eatToken());
        if (condType != irCtx.integralType(1)) {
            reportSemaIssue();
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
        auto* valueType = getType(peekToken());
        if (!valueType) {
            result->setValue(irCtx.voidValue());
            return result;
        }
        eatToken();
        auto valueName = eatToken();
        addValueLink(result.get(), valueType, valueName, &Return::setValue);
        return result;
    }
    case TokenKind::Call: {
        eatToken();
        auto* retType = getType(eatToken());
        auto funcName = eatToken();
        using CallArg = std::pair<Type const*, Token>;
        utl::small_vector<CallArg> args;
        while (true) {
            if (peekToken().kind() != TokenKind::Comma) {
                break;
            }
            eatToken();
            auto* argType = getType(eatToken());
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
                reportSemaIssue();
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
        auto* type   = getType(eatToken());
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
    case TokenKind::Cmp: {
        eatToken();
        auto op       = toCompareOp(eatToken());
        auto* lhsType = getType(eatToken());
        auto lhsName  = eatToken();
        expect(eatToken(), TokenKind::Comma);
        auto* rhsType = getType(eatToken());
        auto rhsName  = eatToken();
        auto result =
            allocate<CompareInst>(irCtx, nullptr, nullptr, op, name());
        addValueLink(result.get(), lhsType, lhsName, &CompareInst::setLHS);
        addValueLink(result.get(), rhsType, rhsName, &CompareInst::setRHS);
        return result;
    }
    case TokenKind::Bnt:
        [[fallthrough]];
    case TokenKind::Lnt: {
        auto op         = toUnaryArithmeticOp(eatToken());
        auto* valueType = getType(eatToken());
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
        auto* lhsType = getType(eatToken());
        auto lhsName  = eatToken();
        expect(eatToken(), TokenKind::Comma);
        auto* rhsType = getType(eatToken());
        auto rhsName  = eatToken();
        auto result   = allocate<ArithmeticInst>(nullptr, nullptr, op, name());
        addValueLink(result.get(), lhsType, lhsName, &ArithmeticInst::setLHS);
        addValueLink(result.get(), rhsType, rhsName, &ArithmeticInst::setRHS);
        return result;
    }
    case TokenKind::GetElementPointer: {
        eatToken();
        auto* accessedType = getType(eatToken());
        expectNext(TokenKind::Comma, TokenKind::Ptr);
        auto basePtrName = eatToken();
        expectNext(TokenKind::Comma);
        auto* indexType = getType(eatToken());
        auto indexName  = eatToken();
        auto indices    = parseConstantIndices();
        if (indices.empty()) {
            reportSyntaxIssue(peekToken());
        }
        auto result = allocate<GetElementPointer>(irCtx,
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
        auto* baseType = getType(eatToken());
        auto baseName  = eatToken();
        expectNext(TokenKind::Comma);
        auto* insType = getType(eatToken());
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
        auto* baseType = getType(eatToken());
        auto baseName  = eatToken();
        auto indices   = parseConstantIndices();
        if (indices.empty()) {
            reportSyntaxIssue(peekToken());
        }
        auto result = allocate<ExtractValue>(nullptr, indices, name());
        addValueLink(result.get(),
                     baseType,
                     baseName,
                     &ExtractValue::setOperand);
        return result;
    }
    case TokenKind::Select: {
        eatToken();
        auto* condType = getType(eatToken());
        if (condType != irCtx.integralType(1)) {
            reportSemaIssue();
        }
        auto condName = eatToken();
        expectNext(TokenKind::Comma);
        auto* thenType = getType(eatToken());
        auto thenName  = eatToken();
        expectNext(TokenKind::Comma);
        auto* elseType = getType(eatToken());
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
        auto indexToken = eatToken();
        if (indexToken.kind() != TokenKind::IntLiteral) {
            reportSyntaxIssue(indexToken);
        }
        size_t index =
            utl::narrow_cast<size_t>(std::atoll(indexToken.id().data()));
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
        auto* type = getType(eatToken());
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

Type const* ParseContext::getType(Token const& token) {
    switch (token.kind()) {
    case TokenKind::Void:
        return irCtx.voidType();
    case TokenKind::Ptr:
        return irCtx.pointerType();
    case TokenKind::GlobalIdentifier: {
        auto structures = mod.structures();
        auto itr        = ranges::find_if(structures, [&](auto&& type) {
            // TODO: Handle '@' and '%' prefixes
            return type->name() == token.id();
        });
        if (itr == ranges::end(structures)) {
            reportSemaIssue();
        }
        return itr->get();
    }
    case TokenKind::LocalIdentifier:
        reportSemaIssue();
    case TokenKind::IntType:
        return irCtx.integralType(token.width());
    case TokenKind::FloatType:
        return irCtx.floatType(token.width());
    default:
        return nullptr;
    }
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
            reportSemaIssue();
        }
        if (value->type() && type && value->type() != type) {
            reportSemaIssue();
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
            auto value = APInt::parse(token.id());
            if (!value) {
                throw SyntaxIssue(token);
            }
            auto* intType = dyncast<IntegralType const*>(type);
            if (!intType) {
                reportSemaIssue();
            }
            value->zext(intType->bitWidth());
            return irCtx.integralConstant(*value);
        }
        else {
            SC_UNREACHABLE();
        }
    case TokenKind::FloatLiteral: {
        SC_DEBUGFAIL();
    }
    default:
        reportSemaIssue();
    }
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
        reportSemaIssue();
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
