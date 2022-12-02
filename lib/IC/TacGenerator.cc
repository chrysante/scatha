#include "IC/TacGenerator.h"

#include <array>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "AST/Visit.h"
#include "IC/ThreeAddressCode.h"
#include "IC/ThreeAddressStatement.h"
#include "Sema/SymbolID.h"

namespace scatha::ic {

namespace {

struct OpTable {
    OpTable(sema::SymbolTable const& sym) {
        using enum ast::BinaryOperator;
        set(sym.Int(), Addition)       = Operation::add;
        set(sym.Int(), Subtraction)    = Operation::sub;
        set(sym.Int(), Multiplication) = Operation::mul;
        set(sym.Int(), Division)       = Operation::idiv;
        set(sym.Int(), Remainder)      = Operation::irem;
        
        set(sym.Int(), Equals)    = Operation::eq;
        set(sym.Int(), NotEquals) = Operation::neq;
        set(sym.Int(), Less)      = Operation::ils;
        set(sym.Int(), LessEq)    = Operation::ileq;
        
        set(sym.Int(), LeftShift)  = Operation::sl;
        set(sym.Int(), RightShift) = Operation::sr;
        
        set(sym.Int(), BitwiseAnd) = Operation::And;
        set(sym.Int(), BitwiseOr)  = Operation::Or;
        set(sym.Int(), BitwiseXOr) = Operation::XOr;
        
        set(sym.Bool(), Equals)     = Operation::eq;
        set(sym.Bool(), NotEquals)  = Operation::neq;
        set(sym.Bool(), BitwiseAnd) = Operation::And;
        set(sym.Bool(), BitwiseOr)  = Operation::Or;
        set(sym.Bool(), BitwiseXOr) = Operation::XOr;
        
        set(sym.Float(), Addition)       = Operation::fadd;
        set(sym.Float(), Subtraction)    = Operation::fsub;
        set(sym.Float(), Multiplication) = Operation::fmul;
        set(sym.Float(), Division)       = Operation::fdiv;
        
        set(sym.Float(), Equals)    = Operation::feq;
        set(sym.Float(), NotEquals) = Operation::fneq;
        set(sym.Float(), Less)      = Operation::fls;
        set(sym.Float(), LessEq)    = Operation::fleq;
    }
    
    Operation& set(sema::TypeID typeID, ast::BinaryOperator op) {
        auto const [itr, success] =
        table.insert({ typeID, std::array<Operation, static_cast<size_t>(ast::BinaryOperator::_count)>{} });
        if (success) {
            std::fill(itr->second.begin(), itr->second.end(), Operation::_count);
        }
        auto& result = itr->second[static_cast<size_t>(op)];
        SC_ASSERT(result == Operation::_count, "");
        return result;
    }
    
    Operation get(sema::TypeID typeID, ast::BinaryOperator op) const {
        auto const itr = table.find(typeID);
        if (itr == table.end()) {
            SC_DEBUGFAIL();
        }
        auto const result = itr->second[static_cast<size_t>(op)];
        SC_ASSERT(result != Operation::_count, "");
        return result;
    }
    
    utl::hashmap<sema::TypeID, std::array<Operation, static_cast<size_t>(ast::BinaryOperator::_count)>> table;
};

struct Context {
    void dispatch(ast::AbstractSyntaxTree const&);
    TasArgument dispatchExpression(ast::Expression const&);
    
    void generate(ast::TranslationUnit const&);
    void generate(ast::FunctionDefinition const&);
    void generate(ast::StructDefinition const&);
    void generate(ast::CompoundStatement const&);
    void generate(ast::VariableDeclaration const&);
    void generate(ast::ExpressionStatement const&);
    void generate(ast::IfStatement const&);
    void generate(ast::WhileStatement const&);
    void generate(ast::ReturnStatement const&);
    void generate(ast::AbstractSyntaxTree const&) { SC_DEBUGFAIL(); }
    
    TasArgument generateExpression(ast::Identifier const&);
    TasArgument generateExpression(ast::MemberAccess const&);
    TasArgument generateExpression(ast::IntegerLiteral const&);
    TasArgument generateExpression(ast::BooleanLiteral const&);
    TasArgument generateExpression(ast::FloatingPointLiteral const&);
    TasArgument generateExpression(ast::BinaryExpression const&);
    TasArgument generateExpression(ast::UnaryPrefixExpression const&);
    TasArgument generateExpression(ast::Conditional const&);
    TasArgument generateExpression(ast::FunctionCall const&);
    TasArgument generateExpression(ast::AbstractSyntaxTree const&) { SC_DEBUGFAIL(); }
    
    void submit(Operation, TasArgument a = {}, TasArgument b = {});
    TasArgument submit(TasArgument result, Operation, TasArgument a = {}, TasArgument b = {});
    
    void submitDeclaration(utl::small_vector<sema::SymbolID>, TasArgument);
    
    /// Returns the code position of the submitted jump,
    /// so the label can be updated later.
    size_t submitJump(Operation, Label label);
    
    // Returns the label
    Label submitLabel();
    
    FunctionLabel submitFunctionLabel(ast::FunctionDefinition const&);
    
    void submitFunctionEndLabel();
    
    /// Returns the appropriate jump instruction to jump over the then block.
    Operation processIfCondition(ast::Expression const& condition);
    
    TasArgument makeTemporary(sema::TypeID type);
    
    Operation selectOperation(sema::TypeID, ast::BinaryOperator) const;
    
    sema::SymbolTable const& sym;
    utl::vector<TacLine>& code;
    
    size_t tmpIndex = 0;
    /// Will be set by the FunctionDefinition case.
    sema::SymbolID currentFunctionID = sema::SymbolID::Invalid;
    /// Will be reset to 0 by the FunctionDefinition case.
    size_t labelIndex = 0;
    
    OpTable opTable{ sym };
};

} // namespace

ThreeAddressCode generateTac(ast::AbstractSyntaxTree const& root, sema::SymbolTable const& sym) {
    SC_ASSERT(root.nodeType() == ast::NodeType::TranslationUnit, "generateTac must be run on a translation unit");
    ThreeAddressCode result;
    Context ctx{ sym, result.statements };
    ctx.dispatch(root);
    return result;
}

void Context::dispatch(ast::AbstractSyntaxTree const& node) {
    ast::visit(node, [this](auto const& node) { this->generate(node); });
}

TasArgument Context::dispatchExpression(ast::Expression const& node) {
    return ast::visit(node, [this](auto const& node) { return this->generateExpression(node); });
}

void Context::generate(ast::TranslationUnit const& tu) {
    for (auto& decl: tu.declarations) {
        dispatch(*decl);
    }
}

void Context::generate(ast::FunctionDefinition const& def) {
    currentFunctionID = def.symbolID();
    tmpIndex          = 0;
    labelIndex        = 0;
    submitFunctionLabel(def);
    dispatch(*def.body);
    submitFunctionEndLabel();
}

void Context::generate(ast::StructDefinition const& def) {
    for (auto& statement: def.body->statements) {
        if (statement->nodeType() == ast::NodeType::FunctionDefinition ||
            statement->nodeType() == ast::NodeType::StructDefinition)
        {
            dispatch(*statement);
        }
    }
}

void Context::generate(ast::CompoundStatement const& block) {
    SC_ASSERT(block.scopeKind() == sema::ScopeKind::Function || block.scopeKind() == sema::ScopeKind::Anonymous,
              "Handle structs only in the struct case");
    for (auto& statement: block.statements) {
        dispatch(*statement);
    }
    return;
}

void Context::generate(ast::VariableDeclaration const& decl) {
    const TasArgument initResult = decl.initExpression ? dispatchExpression(*decl.initExpression) : EmptyArgument();
    submitDeclaration({ decl.symbolID() }, initResult);
}

void Context::generate(ast::ExpressionStatement const& statement) {
    dispatchExpression(*statement.expression);
}

void Context::generate(ast::IfStatement const& ifStatement) {
    Operation const cjmpOp = processIfCondition(*ifStatement.condition);
    size_t const cjmpIndex = submitJump(cjmpOp, Label{});
    dispatch(*ifStatement.ifBlock);
    if (ifStatement.elseBlock != nullptr) {
        size_t const jmpIndex        = submitJump(Operation::jmp, Label{});
        code[cjmpIndex].asTas().arg1 = submitLabel();
        dispatch(*ifStatement.elseBlock);
        code[jmpIndex].asTas().arg1 = submitLabel();
    }
    else {
        code[cjmpIndex].asTas().arg1 = submitLabel();
    }
}

void Context::generate(ast::WhileStatement const& whileStatement) {
    Label const loopBeginLabel = submitLabel();
    Operation const cjmpOp     = processIfCondition(*whileStatement.condition);
    size_t const cjmpIndex     = submitJump(cjmpOp, Label{});
    dispatch(*whileStatement.block);
    submitJump(Operation::jmp, loopBeginLabel);
    code[cjmpIndex].asTas().arg1 = submitLabel();
}

void Context::generate(ast::ReturnStatement const& ret) {
    TasArgument const retValue = dispatchExpression(*ret.expression);
    submit(Operation::ret, retValue);
}

TasArgument Context::generateExpression(ast::Identifier const& id) {
    return Variable(id.symbolID());
}

TasArgument Context::generateExpression(ast::MemberAccess const& ma) {
    auto lhs = dispatchExpression(*ma.object);
    auto rhs = dispatchExpression(*ma.member);
    SC_ASSERT(lhs.is(TasArgument::variable), "");
    SC_ASSERT(rhs.is(TasArgument::variable), "");
    lhs.as<Variable>().append(rhs.as<Variable>());
    return lhs;
}

TasArgument Context::generateExpression(ast::IntegerLiteral const& lit) {
    return LiteralValue{ lit };
}

TasArgument Context::generateExpression(ast::BooleanLiteral const& lit) {
    return LiteralValue{ lit };
}

TasArgument Context::generateExpression(ast::FloatingPointLiteral const& lit) {
    return LiteralValue{ lit };
}

static sema::SymbolID getSymbolID(ast::Expression const& expr) {
    return ast::visit(expr,
                      utl::visitor{ [](ast::Identifier const& id) { return id.symbolID(); },
        [](ast::MemberAccess const& ma) { return ma.symbolID(); },
        [](ast::AbstractSyntaxTree const&) -> sema::SymbolID { SC_DEBUGFAIL(); } });
}

TasArgument Context::generateExpression(ast::BinaryExpression const& expr) {
    TasArgument const lhs = dispatchExpression(*expr.lhs);
    TasArgument const rhs = dispatchExpression(*expr.rhs);
    switch (expr.operation()) {
    case ast::BinaryOperator::Addition: [[fallthrough]];
    case ast::BinaryOperator::Subtraction: [[fallthrough]];
    case ast::BinaryOperator::Multiplication: [[fallthrough]];
    case ast::BinaryOperator::Division: [[fallthrough]];
    case ast::BinaryOperator::Remainder: [[fallthrough]];
    case ast::BinaryOperator::LeftShift: [[fallthrough]];
    case ast::BinaryOperator::RightShift: [[fallthrough]];
    case ast::BinaryOperator::BitwiseOr: [[fallthrough]];
    case ast::BinaryOperator::BitwiseXOr: [[fallthrough]];
    case ast::BinaryOperator::BitwiseAnd:
        return submit(makeTemporary(expr.lhs->typeID()), selectOperation(expr.lhs->typeID(), expr.operation()), lhs, rhs);
    case ast::BinaryOperator::Less: [[fallthrough]];
    case ast::BinaryOperator::LessEq: [[fallthrough]];
    case ast::BinaryOperator::Equals: [[fallthrough]];
    case ast::BinaryOperator::NotEquals:
        return submit(makeTemporary(sym.Bool()), selectOperation(expr.lhs->typeID(), expr.operation()), lhs, rhs);
    case ast::BinaryOperator::Assignment: {
        /// **WARNING: We don't support assigning to arbitrary expressions yet
        auto const lhsSymId = getSymbolID(*expr.lhs);
        auto const var      = Variable{ lhsSymId };
        if (lhs.is(TasArgument::temporary)) {
            code.back().asTas().result = var;
            --tmpIndex;
        }
        else {
            submit(var, Operation::mov, rhs);
        }
        return var;
    }
    case ast::BinaryOperator::Comma: {
        dispatchExpression(*expr.lhs);
        return dispatchExpression(*expr.rhs);
    }
    /// Compound assignment operations like AddAssign, MulAssign etc. must not be here, they should have been
    /// transformed by the canonicalizer.
    default: SC_UNREACHABLE();
    }
}

TasArgument Context::generateExpression(ast::UnaryPrefixExpression const& expr) {
    TasArgument const arg   = dispatchExpression(*expr.operand);
    sema::TypeID const type = expr.typeID();
    switch (expr.operation()) {
    case ast::UnaryPrefixOperator::Promotion: return arg;
    case ast::UnaryPrefixOperator::Negation:
        return submit(makeTemporary(type),
                      selectOperation(type, ast::BinaryOperator::Subtraction),
                      LiteralValue(0, type),
                      arg);
    case ast::UnaryPrefixOperator::BitwiseNot:
        SC_ASSERT(type == sym.Int(), "Only int supported for now");
        return submit(makeTemporary(type), Operation::bnt, arg);
    case ast::UnaryPrefixOperator::LogicalNot:
        SC_ASSERT(type == sym.Bool(), "Only bool supported");
        return submit(makeTemporary(type), Operation::lnt, arg);
    default: SC_UNREACHABLE();
    }
}

TasArgument Context::generateExpression(ast::Conditional const& expr) {
    SC_ASSERT(expr.condition != nullptr, "Invalid argument.");
    SC_ASSERT(expr.ifExpr    != nullptr, "Invalid argument.");
    SC_ASSERT(expr.elseExpr  != nullptr, "Invalid argument.");
    Operation const cjmpOp = processIfCondition(*expr.condition);
    size_t const cjmpIndex = submitJump(cjmpOp, Label{});
    TasArgument const result = makeTemporary(expr.typeID());
    submit(result, Operation::mov, dispatchExpression(*expr.ifExpr));
    size_t const jmpIndex        = submitJump(Operation::jmp, Label{});
    code[cjmpIndex].asTas().arg1 = submitLabel();
    submit(result, Operation::mov, dispatchExpression(*expr.elseExpr));
    code[jmpIndex].asTas().arg1 = submitLabel();
    return result;
}

TasArgument Context::generateExpression(ast::FunctionCall const& expr) {
    for (auto& arg: expr.arguments) {
        submit(Operation::param, dispatchExpression(*arg));
    }
    submitJump(Operation::call, Label(expr.functionID()));
    return submit(makeTemporary(expr.typeID()), Operation::getResult);
}

void Context::submit(Operation op, TasArgument a, TasArgument b) {
    code.push_back(ThreeAddressStatement{ .operation = op, .arg1 = a, .arg2 = b });
}

TasArgument Context::submit(TasArgument result, Operation op, TasArgument a, TasArgument b) {
    SC_ASSERT(result.is(TasArgument::variable)  ||
              result.is(TasArgument::temporary) ||
              result.is(TasArgument::conditional), "");
    code.push_back(ThreeAddressStatement{ .operation = op, .result = result, .arg1 = a, .arg2 = b });
    return result;
}

void Context::submitDeclaration(utl::small_vector<sema::SymbolID> lhsId, TasArgument arg) {
    SC_ASSERT(!lhsId.empty(), "");
    auto const& var  = sym.getVariable(lhsId.back());
    auto const& type = sym.getObjectType(var.typeID());
    
    if (type.isBuiltin()) {
        code.push_back(ThreeAddressStatement{ .operation = Operation::mov, .result = Variable(lhsId), .arg1 = arg });
        return;
    }
    SC_ASSERT(arg.is(TasArgument::empty), "");
    lhsId.emplace_back();
    for (auto const& childID: type.symbols()) {
        if (childID.category() != sema::SymbolCategory::Variable) {
            continue;
        }
        lhsId.back() = childID;
        submitDeclaration(lhsId, arg);
    }
}

size_t Context::submitJump(Operation jmp, Label label) {
    SC_ASSERT(isJump(jmp), "Operation must be a jump");
    code.push_back(ThreeAddressStatement{ .operation = jmp, .arg1 = label });
    return code.size() - 1;
}

Label Context::submitLabel() {
    auto const result = Label(currentFunctionID, labelIndex++);
    code.push_back(result);
    return result;
}

FunctionLabel Context::submitFunctionLabel(ast::FunctionDefinition const& fnDef) {
    FunctionLabel const result(fnDef);
    code.push_back(result);
    return result;
}

void Context::submitFunctionEndLabel() {
    code.push_back(FunctionEndLabel{});
}

Operation Context::processIfCondition(ast::Expression const& condition) {
    TasArgument const condResult = dispatchExpression(condition);
    // clang-format off
    auto& condStatement = [&]() -> auto& {
        if (code.back().isTas()) {
            /// The condition generated a TAS.
            return code.back().asTas();
        }
        if (condResult.is(TasArgument::literalValue)) {
            /// The condition is a literal.
            submit(makeTemporary(sym.Bool()), Operation::mov, condResult);
            return code.back().asTas();
        }
        /// What is the condition?
        SC_DEBUGFAIL();
    }();
    // clang-format on
    
    /// Make the condition a tas conditional statement.
    if (isRelop(condStatement.operation)) {
        condStatement.result    = If{};
        condStatement.operation = reverseRelop(condStatement.operation);
    }
    else {
        auto const& condition = condStatement.result;
        submit(If{}, Operation::ifPlaceholder, condition);
    }
    return Operation::jmp;
}

TasArgument Context::makeTemporary(sema::TypeID type) {
    return Temporary{ tmpIndex++, type };
}

Operation Context::selectOperation(sema::TypeID typeID, ast::BinaryOperator op) const {
    return opTable.get(typeID, op);
}

} // namespace scatha::ic
