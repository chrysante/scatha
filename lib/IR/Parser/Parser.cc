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
///                     | <cmp-inst>
///                     | <un-arith-inst>
///                     | <bin-arith-inst>
/// <call-arg>        ::= <type-id> <local-id>
/// <phi-arg>         ::= "[" "label" <local-id> ":" <id> "]"
/// <cmp-inst>        ::= "cmp" <cmp-op> <type-id> <id> "," <type-id> <id>
/// <cmp-op>          ::= "le", "leq",
/// <un-arith-inst>   ::= <un-op> <type-id> <id>
/// <un-op>           ::= "neg" | ...
/// <bin-arith-inst>  ::= <bin-op> <type-id> <id> "," <id>
/// <bin-op>          ::= "add" | "sub | "mul" | "div" | "rem" | ...
///
/// <struct-def>      ::= "structure" <identifier> "{" {<type-id>}* "}"
///
/// <id>              ::= <local-id> | <global-id>
/// <type-id>         ::= "iN" | "fN" | <global-id>
///
/// ```

#include "IR/Parser/Parser.h"

#include <regex>

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Token.h"
#include "IR/Parser/Lexer.h"

using namespace scatha;
using namespace ir;

namespace {

struct ParseContext {
    explicit ParseContext(Context& irCtx, Module& mod, std::string_view text):
        irCtx(irCtx), mod(mod), lexer(text), nextToken(lexer.next().value()) {}
    
    void parse();
    
private:
    Function* parseFunction();
    Type const* parseParamDecl();
    BasicBlock* parseBasicBlock();
    Instruction* parseInstruction();
    StructureType* parseStructure();
    
    
    void parseTypeDefinition();

    Type const* getType(Token const& token);
    Value* getValue(Token const& token);
    
    Token eatToken() {
        Token result = peekToken();
        if (result.kind() != TokenKind::EndOfFile) {
            nextToken = lexer.next().value();
        }
        return result;
    }
    
    Token peekToken() {
        SC_ASSERT(nextToken.has_value(), "");
        return *nextToken;
    }
    
    Context& irCtx;
    Module& mod;
    Lexer lexer;
    std::optional<Token> nextToken;
    utl::hashmap<std::string_view, Instruction*> instructions;
    utl::hashmap<std::string, Parameter*> parameters;
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

static void expectID(Token const& token, std::string_view id) {
    if (token.id() != id) {
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
    if (declarator.kind() != TokenKind::Keyword && declarator.id() != "function") {
        return nullptr;
    }
    eatToken();
    Token const returnTypeID = eatToken();
    auto* const returnType = getType(returnTypeID);
    Token const name = eatToken();
    Token const openParan = eatToken();
    expectID(openParan, "(");
    utl::vector<Type const*> parameterTypes;
    while (true) {
        auto* paramDecl = parseParamDecl();
        if (!paramDecl) {
            break;
        }
        parameterTypes.push_back(paramDecl);
    }
    Token const closeParan = eatToken();
    expectID(closeParan, ")");
    auto* result = new Function(nullptr,
                                returnType,
                                parameterTypes,
                                std::string(name.id()));
    Token const openBrace = eatToken();
    expectID(openBrace, "{");
    /// Parse the body of the function.
    instructions.clear();
    parameters = result->parameters() |
                 ranges::views::transform([i = 0](auto& p) mutable {
                     return std::pair{ std::to_string(i++), &p };
                 }) |
                 ranges::to<utl::hashmap<std::string, Parameter*>>;
    while (true) {
        auto* basicBlock = parseBasicBlock();
        if (!basicBlock) {
            break;
        }
        result->pushBack(basicBlock);
    }
    Token const closeBrace = eatToken();
    expectID(closeBrace, "}");
    return result;
}

Type const* ParseContext::parseParamDecl() {
    Token const typeID = peekToken();
    auto* const type = getType(typeID);
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
    if (peekToken().id() != ":") {
        throw ParseError(peekToken().sourceLocation());
    }
    eatToken();
    auto* result = new BasicBlock(irCtx, std::string(name.id()));
    while (true) {
        auto* instruction = parseInstruction();
        if (!instruction) {
            break;
        }
        instructions[instruction->name()] = instruction;
        result->pushBack(instruction);
    }
    return result;
}

Instruction* ParseContext::parseInstruction() {
    std::string name = [&]() -> std::string {
        if (peekToken().kind() != TokenKind::LocalIdentifier) {
            return {};
        }
        Token const nameToken = eatToken();
        Token const assignToken = eatToken();
        expectID(assignToken, "=");
        return std::string(nameToken.id());
    }();
    if (peekToken().id() == "alloca") {
        eatToken();
        Token const typeID = eatToken();
        auto* const type = getType(typeID);
        return new Alloca(irCtx, type, std::move(name));
    }
    if (peekToken().id() == "load") {
        eatToken();
        Token const typeID = eatToken();
        auto* const type = getType(typeID);
        Token const ptrID = eatToken();
        Value* const ptr = getValue(ptrID);
        return new Load(ptr, std::move(name));
    }
    // ...
    if (peekToken().id() == "return") {
        eatToken();
        Token const typeID = eatToken();
        auto* const type = getType(typeID);
        Token const valueID = eatToken();
        Value* const value = getValue(valueID);
        /// Here we could check that `value` is of type `type`
        return new Return(irCtx, value);
    }
    return nullptr;
}
           
StructureType* ParseContext::parseStructure() {
    return nullptr;
}

Type const* ParseContext::getType(Token const& token) {
    switch (token.kind()) {
    case TokenKind::GlobalIdentifier: {
        auto structures = mod.structures();
        auto itr = ranges::find_if(structures, [&](Type* type) {
            // TODO: Handle '@' and '%' prefixes
            return type->name() == token.id();
        });
        if (itr == ranges::end(structures)) {
            throw ParseError(token.sourceLocation());
        }
        return *itr;
    }
    case TokenKind::LocalIdentifier:
        return nullptr;
    case TokenKind::IntType:
        return irCtx.integralType(token.width());
    case TokenKind::FloatType:
        return irCtx.floatType(token.width());
    default:
        return nullptr;
    }
}

Value* ParseContext::getValue(Token const& token) {
    switch (token.kind()) {
    case TokenKind::LocalIdentifier: {
        if (auto itr = instructions.find(token.id());
            itr != instructions.end())
        {
            return itr->second;
        }
        if (auto itr = parameters.find(token.id());
            itr != parameters.end())
        {
            return itr->second;
        }
        SC_DEBUGFAIL();
    }
    default:
        throw ParseError(token.sourceLocation());
    }
}
