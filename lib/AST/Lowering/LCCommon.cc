#include "AST/Lowering/LoweringContext.h"

#include <utl/strcat.hpp>

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;

using enum ValueLocation;

void LoweringContext::emitDestructorCalls(sema::DTorStack const& dtorStack) {
    for (auto call: dtorStack) {
        auto* function = getFunction(call.destructor);
        auto* object = objectMap[call.object].get();
        SC_ASSERT(object, "");
        add<ir::Call>(function,
                      std::array<ir::Value*, 1>{ object },
                      std::string{});
    }
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

ir::Value* LoweringContext::toRegister(Value value) {
    switch (value.location()) {
    case Register:
        return value.get();
    case Memory:
        return add<ir::Load>(value.get(),
                             value.type(),
                             std::string(value.get()->name()));
    }
}

ir::Value* LoweringContext::toMemory(Value value) {
    switch (value.location()) {
    case Register:
        return storeLocal(value.get());

    case Memory:
        return value.get();
    }
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

ir::Value* LoweringContext::loadIfRef(Expression const* expr,
                                      ir::Value* value) {
    if (!expr->type()->isReference()) {
        return value;
    }
    auto* refType = isa<sema::ArrayType>(expr->type()->base()) ?
                        arrayViewType :
                        ctx.pointerType();
    return add<ir::Load>(value, refType, utl::strcat(value->name(), ".value"));
}

ir::Callable* LoweringContext::getFunction(sema::Function const* function) {
    switch (function->kind()) {
    case sema::FunctionKind::Native:
        return functionMap.find(function)->second;

    case sema::FunctionKind::External: {
        auto itr = functionMap.find(function);
        if (itr != functionMap.end()) {
            return itr->second;
        }
        return declareFunction(function);
    }

    default:
        SC_UNREACHABLE();
    }
}

utl::small_vector<ir::Value*> LoweringContext::mapArguments(auto&& args) {
    utl::small_vector<ir::Value*> result;
    for (auto* arg: args) {
        result.push_back(getValue(arg));
    }
    return result;
}

ir::Value* LoweringContext::intConstant(APInt value) {
    return ctx.integralConstant(value);
}

ir::Value* LoweringContext::intConstant(size_t value, size_t bitwidth) {
    return intConstant(APInt(value, bitwidth));
}

ir::Value* LoweringContext::floatConstant(APFloat value) {
    return ctx.floatConstant(value,
                             value.precision() == APFloatPrec::Single ? 32 :
                                                                        64);
}

ir::Value* LoweringContext::constant(ssize_t value, ir::Type const* type) {
    return ctx.arithmeticConstant(value, type);
}

void LoweringContext::memorizeObject(sema::Object const* object, Value value) {
    bool success = objectMap.insert({ object, value }).second;
    SC_ASSERT(success, "Redeclaration");
}

void LoweringContext::memorizeArraySize(uint32_t ID, Value size) {
    bool success = arraySizeMap.insert({ ID, size }).second;
    SC_ASSERT(success, "ID already present");
}

void LoweringContext::memorizeArraySize(uint32_t ID, size_t count) {
    memorizeArraySize(ID, Value(newID(), intConstant(count, 64), Register));
}

Value LoweringContext::getArraySize(uint32_t ID) const {
    auto itr = arraySizeMap.find(ID);
    SC_ASSERT(itr != arraySizeMap.end(), "Not found");
    return itr->second;
}

ir::Type const* LoweringContext::mapType(sema::Type const* semaType) {
    // clang-format off
    return visit(*semaType, utl::overload{
        [&](sema::QualType const& qualType) -> ir::Type const* {
            if (!qualType.isReference()) {
                return mapType(qualType.base());
            }
            return ctx.pointerType();
        },
        [&](sema::VoidType const&) -> ir::Type const* {
            return ctx.voidType();
        },
        [&](sema::BoolType const&) -> ir::Type const* {
            return ctx.integralType(1);
        },
        [&](sema::ByteType const&) -> ir::Type const* {
            return ctx.integralType(8);
        },
        [&](sema::IntType const& intType) -> ir::Type const* {
            return ctx.integralType(intType.bitwidth());
        },
        [&](sema::FloatType const& floatType) -> ir::Type const* {
            return ctx.floatType(floatType.bitwidth());
        },
        [&](sema::StructureType const& structType) -> ir::Type const* {
            auto itr = typeMap.find(&structType);
            SC_ASSERT(itr != typeMap.end(), "Type not found");
            return itr->second;
        },
        [&](sema::ArrayType const& arrayType) -> ir::Type const* {
            return ctx.arrayType(mapType(arrayType.elementType()), arrayType.count());
        },
    }); // clang-format on
}

ir::UnaryArithmeticOperation LoweringContext::mapUnaryOp(
    ast::UnaryOperator op) {
    switch (op) {
    case UnaryOperator::BitwiseNot:
        return ir::UnaryArithmeticOperation::BitwiseNot;
    case UnaryOperator::LogicalNot:
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
    sema::BuiltinType const* type, ast::BinaryOperator op) {
    switch (op) {
    case BinaryOperator::Multiplication:
        // clang-format off
        return visit(*type, utl::overload{
            [](sema::IntType const&) {
                return ir::ArithmeticOperation::Mul;
            },
            [](sema::FloatType const&) {
                return ir::ArithmeticOperation::FMul;
            },
            [](sema::BuiltinType const&) -> ir::ArithmeticOperation {
                SC_UNREACHABLE();
            },
        }); // clang-format on

    case BinaryOperator::Division:
        // clang-format off
        return visit(*type, utl::overload{
            [](sema::IntType const& type) {
                return type.isSigned() ?
                    ir::ArithmeticOperation::SDiv :
                    ir::ArithmeticOperation::UDiv;
            },
            [](sema::FloatType const&) {
                return ir::ArithmeticOperation::FDiv;
            },
            [](sema::BuiltinType const&) -> ir::ArithmeticOperation {
                SC_UNREACHABLE();
            },
        }); // clang-format on

    case BinaryOperator::Remainder:
        return cast<sema::IntType const*>(type)->isSigned() ?
                   ir::ArithmeticOperation::SRem :
                   ir::ArithmeticOperation::URem;

    case BinaryOperator::Addition:
        // clang-format off
        return visit(*type, utl::overload{
            [](sema::IntType const&) {
                return ir::ArithmeticOperation::Add;
            },
            [](sema::FloatType const&) {
                return ir::ArithmeticOperation::FAdd;
            },
            [](sema::BuiltinType const&) -> ir::ArithmeticOperation {
                SC_UNREACHABLE();
            },
        }); // clang-format on

    case BinaryOperator::Subtraction:
        // clang-format off
        return visit(*type, utl::overload{
            [](sema::IntType const&) {
                return ir::ArithmeticOperation::Sub;
            },
            [](sema::FloatType const&) {
                return ir::ArithmeticOperation::FSub;
            },
            [](sema::BuiltinType const&) -> ir::ArithmeticOperation {
                SC_UNREACHABLE();
            },
        }); // clang-format on

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
    sema::BuiltinType const* type, ast::BinaryOperator op) {
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

ir::CompareMode LoweringContext::mapCompareMode(sema::BuiltinType const* type) {
    // clang-format off
    return visit(*type, utl::overload{
        [](sema::VoidType const&) -> ir::CompareMode {
            SC_UNREACHABLE();
        },
        [](sema::BoolType const&) {
            return ir::CompareMode::Unsigned;
        },
        [](sema::ByteType const&) {
            return ir::CompareMode::Unsigned;
        },
        [](sema::IntType const& type) {
            return type.isSigned() ?
                ir::CompareMode::Signed :
                ir::CompareMode::Unsigned;
        },
        [](sema::FloatType const&) {
            return ir::CompareMode::Float;
        },
    }); // clang-format on
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
