// clang-format off

/// ## IR Grammar in BNF
///
/// ```
/// <module>          ::= {<decl>}*
/// <decl>            ::= <func-def>
///                     | <struct-def>
///
/// <func-def>        ::= "function" <type-id> <global-id> "(" {<type-id>}* ")" <func-body>
/// <func-body>       ::= "{" {<basic-block>}* "}"
///
/// <basic-block>     ::= <local-id> ":" {<statement>}*
/// <instruction>     ::= [<local-id> "="] <inst-...>
/// <inst-...>        ::= "alloca" <type-id>
///                     | "load" <type-id> <id>
///                     | "store" <id>, <id>
///                     | "goto" "label" <local-id>
///                     | "branch" <type-id> <local-id> ","
///                                "label" <local-id> ","
///                                "label" <local-id>
///                     | "return" <type-id> <id>
///                     | "call" <type-id> <global-id> "," {<call-arg>}+
///                     | "phi" <type-id> {<phi-arg>}+
///                     | "cmp" <cmp-op> <type-id> <id> "," <type-id> <id>
///                     | <un-op> <type-id> <id>
///                     | <bin-op> <type-id> <id> "," <id>
///                     | "gep" <type-id> <id> "," <type-id> <id> "," <type-id> <id>
///                     | "insert_value" <type-id> <id> "," <type-id> <id> "," <type-id> <id>
///                     | "extract_value" <type-id> <id> "," <type-id> <id>
/// <call-arg>        ::= <type-id> <local-id>
/// <phi-arg>         ::= "[" "label" <local-id> ":" <id> "]"
/// <cmp-op>          ::= "le", "leq",
/// <un-op>           ::= "neg" | ...
/// <bin-op>          ::= "add" | "sub | "mul" | "div" | "rem" | ...
///
/// <struct-def>      ::= "structure" <identifier> "{" {<type-id>}* "}"
///
/// <id>              ::= <local-id> | <global-id>
/// <type-id>         ::= "iN" | "fN" | <global-id>
///
/// ```

// clang-format on

#include "IR/Parser/Parser.h"

#include <functional>

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Lexer.h"
#include "IR/Parser/Token.h"
#include "IR/Type.h"

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
    Function* parseFunction();
    Type const* parseParamDecl();
    BasicBlock* parseBasicBlock();
    Instruction* parseInstruction();
    StructureType* parseStructure();

    void parseTypeDefinition();

    Type const* getType(Token const& token);
    Function* getFunction(Token const& token);
    BasicBlock* getBasicBlock(Token const& token);
    Value* getValue(Token const& token);

    void registerBasicBlock(BasicBlock* basicBlock);

    void registerInstruction(Instruction* instruction);

    void addPendingUpdate(std::string_view name, std::function<void()> f) {
        pendingUpdates[name].push_back(std::move(f));
    }

    void executePendingUpdates(std::string_view name);

    Token eatToken() {
        Token result = peekToken();
        if (result.kind() != TokenKind::EndOfFile) {
            auto next = lexer.next();
            if (!next) {
                throw ParseError(next.error().sourceLocation());
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

    Context& irCtx;
    Module& mod;
    Lexer lexer;
    std::optional<Token> nextToken[2];
    utl::hashmap<std::string_view, Instruction*> instructions;
    utl::hashmap<std::string, Parameter*> parameters;
    utl::hashmap<std::string_view, BasicBlock*> basicBlocks;
    utl::hashmap<std::string, utl::small_vector<std::function<void()>>>
        pendingUpdates;
};

} // namespace

Expected<Module, ParseError> ir::parse(std::string_view text, Context& irCtx) {
    Module mod;
    ParseContext ctx(irCtx, mod, text);
    try {
        ctx.parse();
        return mod;
    }
    catch (ParseError const& error) {
        return error;
    }
    catch (InvalidToken const& error) {
        return ParseError(error.sourceLocation());
    }
    SC_UNREACHABLE();
}

static void expect(Token const& token, std::string_view id) {
    if (token.id() != id) {
        throw ParseError(token.sourceLocation());
    }
}

static void expect(Token const& token, TokenKind kind) {
    if (token.kind() != kind) {
        throw ParseError(token.sourceLocation());
    }
}

void ParseContext::parse() {
    while (peekToken().kind() != TokenKind::EndOfFile) {
        if (auto* function = parseFunction()) {
            mod.addFunction(function);
            continue;
        }
        if (auto* structure = parseStructure()) {
            mod.addStructure(structure);
            continue;
        }
        throw ParseError(peekToken().sourceLocation());
    }
}

Function* ParseContext::parseFunction() {
    Token const declarator = peekToken();
    if (declarator.kind() != TokenKind::Keyword ||
        declarator.id() != "function")
    {
        return nullptr;
    }
    eatToken();
    auto* const returnType = getType(eatToken());
    Token const name       = eatToken();
    expect(eatToken(), "(");
    utl::vector<Type const*> parameterTypes;
    while (true) {
        auto* paramDecl = parseParamDecl();
        if (!paramDecl) {
            break;
        }
        parameterTypes.push_back(paramDecl);
    }
    expect(eatToken(), ")");
    auto* result = new Function(nullptr,
                                returnType,
                                parameterTypes,
                                std::string(name.id()));
    expect(eatToken(), "{");
    /// Parse the body of the function.
    instructions.clear();
    parameters = result->parameters() |
                 ranges::views::transform([i = 0](auto& p) mutable {
                     return std::pair{ std::to_string(i++), &p };
                 }) |
                 ranges::to<utl::hashmap<std::string, Parameter*>>;
    basicBlocks.clear();
    while (true) {
        auto* basicBlock = parseBasicBlock();
        if (!basicBlock) {
            break;
        }
        result->pushBack(basicBlock);
    }
    Token const closeBrace = eatToken();
    expect(closeBrace, "}");
    return result;
}

Type const* ParseContext::parseParamDecl() {
    auto* const type = getType(peekToken());
    if (!type) {
        return nullptr;
    }
    eatToken();
    return type;
}

BasicBlock* ParseContext::parseBasicBlock() {
    if (peekToken().kind() != TokenKind::LocalIdentifier) {
        return nullptr;
    }
    Token const name = eatToken();
    expect(eatToken(), ":");
    auto* result = new BasicBlock(irCtx, std::string(name.id()));
    registerBasicBlock(result);
    while (true) {
        auto* instruction = parseInstruction();
        if (!instruction) {
            break;
        }
        registerInstruction(instruction);
        result->pushBack(instruction);
    }
    return result;
}

static std::optional<CompareOperation> toCompareOp(Token token) {
#define SC_COMPARE_OPERATION_DEF(op, str)                                      \
    if (token.id() == #str) {                                                  \
        return CompareOperation::op;                                           \
    }
#include "IR/Lists.def"
    return std::nullopt;
}

static std::optional<UnaryArithmeticOperation> toUnaryArithmeticOp(
    Token token) {
#define SC_UNARY_ARITHMETIC_OPERATION_DEF(op, str)                             \
    if (token.id() == #str) {                                                  \
        return UnaryArithmeticOperation::op;                                   \
    }
#include "IR/Lists.def"
    return std::nullopt;
}

static std::optional<ArithmeticOperation> toArithmeticOp(Token token) {
#define SC_ARITHMETIC_OPERATION_DEF(op, str)                                   \
    if (token.id() == #str) {                                                  \
        return ArithmeticOperation::op;                                        \
    }
#include "IR/Lists.def"
    return std::nullopt;
}

Instruction* ParseContext::parseInstruction() {
    auto name = [&]() -> std::optional<std::string> {
        Token const nameToken   = peekToken(0);
        Token const assignToken = peekToken(1);
        if (nameToken.kind() != TokenKind::LocalIdentifier) {
            return std::nullopt;
        }
        if (assignToken.id() != "=") {
            return std::nullopt;
        }
        eatToken(2);
        return std::string(nameToken.id());
    }();
    if (peekToken().id() == "alloca") {
        eatToken();
        auto* const type = getType(eatToken());
        return new Alloca(irCtx, type, std::move(name).value());
    }
    if (peekToken().id() == "load") {
        eatToken();
        auto* const type = getType(eatToken());
        Value* const ptr = getValue(eatToken());
        return new Load(ptr, type, std::move(name).value());
    }
    if (peekToken().id() == "store") {
        eatToken();
        Value* const addr = getValue(eatToken());
        expect(eatToken(), ",");
        Value* const value = getValue(eatToken());
        return new Store(irCtx, addr, value);
    }
    if (peekToken().id() == "goto") {
        eatToken();
        expect(eatToken(), "label");
        Token const targetID = eatToken();
        expect(targetID, TokenKind::LocalIdentifier);
        BasicBlock* const target = getBasicBlock(targetID);
        auto* result             = new Goto(irCtx, target);
        if (!target) {
            addPendingUpdate(targetID.id(), [=] {
                auto* bb = getBasicBlock(targetID);
                SC_ASSERT(bb, "");
                result->setTarget(bb);
            });
        }
        return result;
    }
    if (peekToken().id() == "branch") {
        eatToken();
        auto* const type  = getType(eatToken());
        Value* const cond = getValue(eatToken());
        auto* result      = new Branch(irCtx, cond, nullptr, nullptr);
        for (size_t i = 0; i < 2; ++i) {
            expect(eatToken(), ",");
            expect(eatToken(), "label");
            Token const targetID = eatToken();
            expect(targetID, TokenKind::LocalIdentifier);
            BasicBlock* const target = getBasicBlock(targetID);
            if (target) {
                result->setTarget(i, target);
            }
            else {
                addPendingUpdate(targetID.id(), [=] {
                    auto* target = getBasicBlock(targetID);
                    SC_ASSERT(target, "");
                    result->setTarget(i, target);
                });
            }
        }
        return result;
    }
    if (peekToken().id() == "return") {
        eatToken();
        auto* const type   = getType(eatToken());
        Value* const value = getValue(eatToken());
        /// Here we could check that `value` is of type `type`
        return new Return(irCtx, value);
    }
    if (peekToken().id() == "call") {
        eatToken();
        auto* const type     = getType(eatToken());
        auto* const function = getFunction(eatToken());
        utl::small_vector<Value*> args;
        while (true) {
            if (peekToken().id() != ",") {
                break;
            }
            eatToken();
            auto* const argType = getType(eatToken());
            Value* arg          = getValue(eatToken());
            args.push_back(arg);
        }
        return new FunctionCall(function,
                                args,
                                std::move(name).value_or(std::string{}));
    }
    if (peekToken().id() == "phi") {
        SC_DEBUGFAIL();
    }
    if (peekToken().id() == "cmp") {
        eatToken();
        Token const cmpToken = eatToken();
        auto const cmpOp     = toCompareOp(cmpToken);
        if (!cmpOp) {
            throw ParseError(cmpToken.sourceLocation());
        }
        auto* const lhsType = getType(eatToken());
        Value* const lhs    = getValue(eatToken());
        expect(eatToken(), ",");
        auto* const rhsType = getType(eatToken());
        Value* const rhs    = getValue(eatToken());
        return new CompareInst(irCtx,
                               lhs,
                               rhs,
                               *cmpOp,
                               std::move(name).value());
    }
    if (auto unaryOp = toUnaryArithmeticOp(peekToken())) {
        eatToken();
        auto* type           = getType(eatToken());
        Value* const operand = getValue(eatToken());
        return new UnaryArithmeticInst(irCtx,
                                       operand,
                                       *unaryOp,
                                       std::move(name).value());
    }
    if (auto binaryOp = toArithmeticOp(peekToken())) {
        eatToken();
        auto* type       = getType(eatToken());
        Value* const lhs = getValue(eatToken());
        expect(eatToken(), ",");
        Value* const rhs = getValue(eatToken());
        return new ArithmeticInst(lhs, rhs, *binaryOp, std::move(name).value());
    }
    if (peekToken().id() == "gep") {
        SC_DEBUGFAIL();
    }
    if (peekToken().id() == "insert_value") {
        eatToken();
        auto* const structType  = getType(eatToken());
        auto* const structValue = getValue(eatToken());
        expect(eatToken(), ",");
        auto* const memberType  = getType(eatToken());
        auto* const memberValue = getValue(eatToken());
        expect(eatToken(), ",");
        auto* const indexType  = getType(eatToken());
        auto* const indexValue = getValue(eatToken());
        return new InsertValue(structValue,
                               memberValue,
                               indexValue,
                               std::move(name).value());
    }
    if (peekToken().id() == "extract_value") {
        eatToken();
        auto* const structType  = getType(eatToken());
        auto* const structValue = getValue(eatToken());
        expect(eatToken(), ",");
        auto* const indexType  = getType(eatToken());
        auto* const indexValue = getValue(eatToken());
        APInt const constantIndex =
            cast<IntegralConstant const*>(indexValue)->value();
        return new ExtractValue(cast<StructureType const*>(structType)
                                    ->memberAt(constantIndex.to<size_t>()),
                                structValue,
                                indexValue,
                                std::move(name).value());
    }
    return nullptr;
}

StructureType* ParseContext::parseStructure() {
    Token const declarator = peekToken();
    if (declarator.kind() != TokenKind::Keyword ||
        declarator.id() != "structure")
    {
        return nullptr;
    }
    eatToken();
    Token const nameID = eatToken();
    expect(nameID, TokenKind::GlobalIdentifier);
    expect(eatToken(), "{");
    utl::small_vector<Type const*> members;
    while (true) {
        auto* type = getType(eatToken());
        members.push_back(type);
        if (peekToken().id() != ",") {
            break;
        }
        eatToken();
    }
    expect(eatToken(), "}");
    auto* result = new StructureType(std::string(nameID.id()), members);
    mod.addStructure(result);
    return result;
}

Type const* ParseContext::getType(Token const& token) {
    switch (token.kind()) {
    case TokenKind::GlobalIdentifier: {
        auto structures = mod.structures();
        auto itr        = ranges::find_if(structures, [&](Type* type) {
            // TODO: Handle '@' and '%' prefixes
            return type->name() == token.id();
        });
        if (itr == ranges::end(structures)) {
            throw ParseError(token.sourceLocation());
        }
        return *itr;
    }
    case TokenKind::LocalIdentifier: return nullptr;
    case TokenKind::IntType: return irCtx.integralType(token.width());
    case TokenKind::FloatType: return irCtx.floatType(token.width());
    default: return nullptr;
    }
}

Function* ParseContext::getFunction(Token const& token) {
    if (token.kind() != TokenKind::GlobalIdentifier) {
        throw ParseError(token.sourceLocation());
    }
    auto itr = ranges::find_if(mod.functions(), [&](Function const& f) {
        return f.name() == token.id();
    });
    if (itr != mod.functions().end()) {
        return &*itr;
    }
    return nullptr;
}

BasicBlock* ParseContext::getBasicBlock(Token const& token) {
    if (token.kind() != TokenKind::LocalIdentifier) {
        throw ParseError(token.sourceLocation());
    }
    if (auto itr = basicBlocks.find(token.id()); itr != basicBlocks.end()) {
        return itr->second;
    }
    return nullptr;
}

Value* ParseContext::getValue(Token const& token) {
    switch (token.kind()) {
    case TokenKind::LocalIdentifier: {
        if (auto const itr = instructions.find(token.id());
            itr != instructions.end())
        {
            return itr->second;
        }
        if (auto const itr = parameters.find(token.id());
            itr != parameters.end())
        {
            return itr->second;
        }
        SC_DEBUGFAIL();
    }
    case TokenKind::IntLiteral: {
        auto value = APInt::parse(token.id());
        if (!value) {
            throw ParseError(token.sourceLocation());
        }
        value->zext(64);
        return irCtx.integralConstant(*value);
    }
    default: throw ParseError(token.sourceLocation());
    }
}

void ParseContext::registerBasicBlock(BasicBlock* basicBlock) {
    SC_ASSERT(!basicBlocks.contains(basicBlock->name()),
              "Actually a soft error");
    basicBlocks[basicBlock->name()] = basicBlock;
    executePendingUpdates(basicBlock->name());
}

void ParseContext::registerInstruction(Instruction* instruction) {
    if (instruction->name().empty()) {
        return;
    }
    SC_ASSERT(!instructions.contains(instruction->name()),
              "Actually a soft error");
    instructions[instruction->name()] = instruction;
    executePendingUpdates(instruction->name());
}

void ParseContext::executePendingUpdates(std::string_view name) {
    auto const itr = pendingUpdates.find(name);
    if (itr == pendingUpdates.end()) {
        return;
    }
    for (auto& updateFn: itr->second) {
        updateFn();
    }
}
