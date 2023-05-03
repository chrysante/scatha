#include "AST/Lowering/LoweringContext.h"

#include <utl/strcat.hpp>

#include "AST/LowerToIR.h" // Remove later
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h" // Remove later
#include "IR/Type.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;

[[nodiscard]] SCATHA_API ir::Module ast::lowerToIR2(
    AbstractSyntaxTree const& root,
    sema::SymbolTable const& symbolTable,
    ir::Context& ctx) {
    ir::Module mod;
    LoweringContext context(symbolTable, ctx, mod);
    context.run(root);
    return mod;
}

void LoweringContext::run(ast::AbstractSyntaxTree const& root) {
    makeDeclarations();
    generate(root);
}

ir::BasicBlock* LoweringContext::newBlock(std::string name) {
    return new ir::BasicBlock(ctx, std::move(name));
}

void LoweringContext::add(ir::BasicBlock* block) {
    currentFunction->pushBack(block);
    currentBlock = block;
}

ir::BasicBlock* LoweringContext::addNewBlock(std::string name) {
    auto* block = newBlock(std::move(name));
    add(block);
    return block;
}

void LoweringContext::add(ir::Instruction* inst) {
    currentBlock->pushBack(inst);
}

ir::Value* LoweringContext::storeLocal(ir::Value* value, std::string name) {
    if (name.empty()) {
        name = utl::strcat(value->name(), ".addr");
    }
    auto* addr = makeLocal(value->type(), std::move(name));
    add<ir::Store>(addr, value);
    return addr;
}

ir::Value* LoweringContext::makeLocal(ir::Type const* type, std::string name) {
    auto* addr = new ir::Alloca(ctx, type, std::move(name));
    allocas.push_back(addr);
    return addr;
}

void LoweringContext::memorizeVariableAddress(sema::Entity const* entity,
                                              ir::Value* value) {
    bool success = variableAddressMap.insert({ entity, value }).second;
    SC_ASSERT(success, "Redeclaration");
}

ir::Type const* LoweringContext::mapType(sema::Type const* semaType) {
    // clang-format off
    return visit(*semaType, utl::overload{
        [&](sema::QualType const& qualType) -> ir::Type const* {
            if (qualType.isReference()) {
                if (isa<sema::ArrayType>(qualType.base())) {
                    SC_DEBUGFAIL();
                    // return arrayViewType;
                }
                return ctx.pointerType();
            }
            return mapType(qualType.base());
        },
        [&](sema::StructureType const& structType) -> ir::Type const* {
            if (&structType == symbolTable.Void()) {
                return ctx.voidType();
            }
            if (&structType == symbolTable.Byte()) {
                return ctx.integralType(8);
            }
            if (&structType == symbolTable.Bool()) {
                return ctx.integralType(1);
            }
            if (&structType == symbolTable.S8() ||
                &structType == symbolTable.U8())
            {
                return ctx.integralType(8);
            }
            if (&structType == symbolTable.S16() ||
                &structType == symbolTable.U16())
            {
                return ctx.integralType(16);
            }
            if (&structType == symbolTable.S32() ||
                &structType == symbolTable.U32())
            {
                return ctx.integralType(32);
            }
            if (&structType == symbolTable.S64() ||
                &structType == symbolTable.U64())
            {
                return ctx.integralType(64);
            }
            if (&structType == symbolTable.Float()) {
                return ctx.floatType(64);
            }
            if (auto itr = typeMap.find(&structType); itr != typeMap.end()) {
                return itr->second;
            }
            SC_DEBUGFAIL();
        },
        [&](sema::ArrayType const& arrayType) -> ir::Type const* {
            SC_DEBUGFAIL();
            // return arrayViewType;
        },
    }); // clang-format on
}

ir::UnaryArithmeticOperation LoweringContext::mapUnaryOp(
    ast::UnaryPrefixOperator op) {
    switch (op) {
    case UnaryPrefixOperator::BitwiseNot:
        return ir::UnaryArithmeticOperation::BitwiseNot;
    case UnaryPrefixOperator::LogicalNot:
        return ir::UnaryArithmeticOperation::LogicalNot;
    default:
        SC_UNREACHABLE();
    }
}

ir::CompareOperation LoweringContext::mapCompareOp(ast::BinaryOperator op) {
    switch (op) {
    case BinaryOperator::Less:
        return ir::CompareOperation::Less;
    case BinaryOperator::LessEq:
        return ir::CompareOperation::LessEq;
    case BinaryOperator::Greater:
        return ir::CompareOperation::Greater;
    case BinaryOperator::GreaterEq:
        return ir::CompareOperation::GreaterEq;
    case BinaryOperator::Equals:
        return ir::CompareOperation::Equal;
    case BinaryOperator::NotEquals:
        return ir::CompareOperation::NotEqual;
    default:
        SC_UNREACHABLE("Only handle compare operations here.");
    }
}

ir::ArithmeticOperation LoweringContext::mapArithmeticOp(
    sema::StructureType const* type, ast::BinaryOperator op) {
    switch (op) {
    case BinaryOperator::Multiplication:
        if (type == symbolTable.S64()) {
            return ir::ArithmeticOperation::Mul;
        }
        if (type == symbolTable.Float()) {
            return ir::ArithmeticOperation::FMul;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Division:
        if (type == symbolTable.S64()) {
            return ir::ArithmeticOperation::SDiv;
        }
        if (type == symbolTable.Float()) {
            return ir::ArithmeticOperation::FDiv;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Remainder:
        if (type == symbolTable.S64()) {
            return ir::ArithmeticOperation::SRem;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Addition:
        if (type == symbolTable.S64()) {
            return ir::ArithmeticOperation::Add;
        }
        if (type == symbolTable.Float()) {
            return ir::ArithmeticOperation::FAdd;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Subtraction:
        if (type == symbolTable.S64()) {
            return ir::ArithmeticOperation::Sub;
        }
        if (type == symbolTable.Float()) {
            return ir::ArithmeticOperation::FSub;
        }
        SC_UNREACHABLE();
    case BinaryOperator::LeftShift:
        return ir::ArithmeticOperation::LShL;
    case BinaryOperator::RightShift:
        return ir::ArithmeticOperation::LShR;
    case BinaryOperator::BitwiseAnd:
        return ir::ArithmeticOperation::And;
    case BinaryOperator::BitwiseXOr:
        return ir::ArithmeticOperation::XOr;
    case BinaryOperator::BitwiseOr:
        return ir::ArithmeticOperation::Or;
    default:
        SC_UNREACHABLE("Only handle arithmetic operations here.");
    }
}

ir::ArithmeticOperation LoweringContext::mapArithmeticAssignOp(
    sema::StructureType const* type, ast::BinaryOperator op) {
    auto nonAssign = [&] {
        switch (op) {
        case BinaryOperator::AddAssignment:
            return BinaryOperator::Addition;
        case BinaryOperator::SubAssignment:
            return BinaryOperator::Subtraction;
        case BinaryOperator::MulAssignment:
            return BinaryOperator::Multiplication;
        case BinaryOperator::DivAssignment:
            return BinaryOperator::Division;
        case BinaryOperator::RemAssignment:
            return BinaryOperator::Remainder;
        case BinaryOperator::LSAssignment:
            return BinaryOperator::LeftShift;
        case BinaryOperator::RSAssignment:
            return BinaryOperator::RightShift;
        case BinaryOperator::AndAssignment:
            return BinaryOperator::BitwiseAnd;
        case BinaryOperator::OrAssignment:
            return BinaryOperator::BitwiseOr;
        case BinaryOperator::XOrAssignment:
            return BinaryOperator::BitwiseXOr;
        default:
            SC_UNREACHABLE("Only handle arithmetic assign operations here.");
        }
    }();
    return mapArithmeticOp(type, nonAssign);
}

ir::CompareMode LoweringContext::mapCompareMode(
    sema::StructureType const* type) {
    if (type == symbolTable.Bool()) {
        return ir::CompareMode::Unsigned;
    }
    if (type == symbolTable.S64()) {
        return ir::CompareMode::Signed;
    }
    if (type == symbolTable.Float()) {
        return ir::CompareMode::Float;
    }
    SC_DEBUGFAIL();
}

ir::FunctionAttribute LoweringContext::mapFuncAttrs(
    sema::FunctionAttribute attr) {
    switch (attr) {
        using enum ir::FunctionAttribute;
    case sema::FunctionAttribute::Pure:
        return Memory_WriteNone;
    case sema::FunctionAttribute::Const:
        return Memory_ReadNone | Memory_WriteNone;
    default:
        return None;
    }
}

ir::Visibility LoweringContext::accessSpecToVisibility(
    sema::AccessSpecifier spec) {
    switch (spec) {
    case sema::AccessSpecifier::Public:
        return ir::Visibility::Extern;
    case sema::AccessSpecifier::Private:
        return ir::Visibility::Static;
    }
}
