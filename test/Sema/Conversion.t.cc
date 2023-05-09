#include <Catch/Catch2.hpp>

#include <array>

#include "AST/AST.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

TEST_CASE("Implicit conversion rank", "[sema]") {
    sema::SymbolTable sym;
    ast::UnaryExpression expr(ast::UnaryOperator::Promotion,
                              nullptr,
                              SourceRange{});
    SECTION("1") {
        expr.decorate(nullptr, sym.qU16());
        CHECK(implicitConversionRank(&expr, sym.qU16()).value() == 0);
    }
    SECTION("2") {
        expr.decorate(nullptr, sym.qS64());
        CHECK(implicitConversionRank(&expr, sym.qS64()).value() == 0);
    }
    SECTION("3") {
        expr.decorate(nullptr, sym.qU16());
        CHECK(implicitConversionRank(&expr, sym.qS32()).value() == 1);
    }
    SECTION("4") {
        expr.decorate(nullptr, sym.qU16());
        CHECK(implicitConversionRank(&expr, sym.qU32()).value() == 1);
    }
    SECTION("5") {
        expr.decorate(nullptr, sym.qS16());
        CHECK(!implicitConversionRank(&expr, sym.qU32()));
    }
}

TEST_CASE("Arithemetic conversions", "[sema]") {
    sema::SymbolTable sym;
    IssueHandler iss;
    ast::UnaryExpression
        base(ast::UnaryOperator::Promotion,
             allocate<ast::UnaryExpression>(ast::UnaryOperator::Promotion,
                                            nullptr,
                                            SourceRange{}),
             SourceRange{});
    auto* expr   = base.operand();
    auto setType = [&](QualType const* type) { expr->decorate(nullptr, type); };
    auto set     = [&](QualType const* type, auto value) {
        setType(type);
        auto* arithType = cast<ArithmeticType const*>(type->base());
        APInt apValue(value, arithType->bitwidth());
        expr->setConstantValue(
            allocate<IntValue>(apValue, arithType->isSigned()));
    };
    auto getResult = [&] {
        return cast<IntValue const*>(
                   cast<ast::Expression const*>(expr->parent())
                       ->constantValue())
            ->value();
    };

    /// # Widening
    SECTION("u32(5) to u64") {
        set(sym.qU32(), 5);
        CHECK(convertImplicitly(expr, sym.qU64(), iss));
        CHECK(iss.empty());
    }
    SECTION("u64(5) to u64") {
        set(sym.qU64(), 5);
        CHECK(convertImplicitly(expr, sym.qU64(), iss));
        CHECK(iss.empty());
    }

    /// # Narrowing
    SECTION("s64(5) to s8") {
        set(sym.qS64(), 5);
        CHECK(convertImplicitly(expr, sym.qS8(), iss));
        CHECK(iss.empty());
    }
    SECTION("s64(<unknown>) to s8") {
        setType(sym.qS64());
        CHECK(!convertImplicitly(expr, sym.qS8(), iss));
        CHECK(convertExplicitly(expr, sym.qS8(), iss));
    }
    SECTION("S64(-1) to u32") {
        set(sym.qS64(), -1);
        CHECK(!convertImplicitly(expr, sym.qU32(), iss));
        CHECK(convertExplicitly(expr, sym.qU32(), iss));
        APInt result = getResult();
        CHECK(result.bitwidth() == 32);
        CHECK(ucmp(result, ~0u) == 0);
    }
    SECTION("U32(0x1000000F) to s16") {
        set(sym.qU32(), 0x1000000F);
        CHECK(!convertImplicitly(expr, sym.qS16(), iss));
        CHECK(convertExplicitly(expr, sym.qS16(), iss));
        APInt result = getResult();
        CHECK(result.bitwidth() == 16);
        CHECK(ucmp(result, 0xF) == 0);
    }
    SECTION("-1 to u64") {
        set(sym.qS64(), -1);
        CHECK(!convertImplicitly(expr, sym.qU64(), iss));
        CHECK(convertExplicitly(expr, sym.qU32(), iss));
    }
    SECTION("s64(5) to byte") {
        set(sym.qS64(), 5);
        CHECK(convertImplicitly(expr, sym.qByte(), iss));
        CHECK(iss.empty());
    }
    SECTION("s64(256) to byte") {
        set(sym.qS64(), 256);
        CHECK(!convertImplicitly(expr, sym.qByte(), iss));
    }
}
