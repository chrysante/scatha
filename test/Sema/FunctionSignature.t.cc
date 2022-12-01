#include <Catch/Catch2.hpp>

#include "Sema/FunctionSignature.h"
#include "Sema/SymbolTable.h"

#include <iostream>

using namespace scatha;

TEST_CASE("FunctionSignature hash", "[sema]") {
    sema::SymbolTable sym;
    auto const i_i   = sema::FunctionSignature({ sym.Int() }, sym.Int());
    auto const f_f_i = sema::FunctionSignature({ sym.Float(), sym.Float() }, sym.Int());
    auto const v_i   = sema::FunctionSignature({}, sym.Int());
    CHECK(i_i.argumentHash() != f_f_i.argumentHash());
    CHECK(i_i.argumentHash() != v_i.argumentHash());
    CHECK(v_i.argumentHash() != f_f_i.argumentHash());
}

TEST_CASE("Function Type", "[sema]") {
    sema::SymbolTable sym;
    auto const fSig = sema::FunctionSignature({ sym.Int() }, sym.Int());
    auto const gSig = sema::FunctionSignature({ sym.Int() }, sym.Void());
    CHECK(fSig.argumentHash() == gSig.argumentHash());
    auto const fnF = sym.declareFunction(Token("f", TokenType::Identifier));
    REQUIRE(fnF.hasValue());
    auto const overloadSuccess = sym.setSignature(fnF->symbolID(), fSig);
    REQUIRE(overloadSuccess);
    CHECK(fnF.value().signature().argumentTypeID(0) == sym.Int());
    CHECK(fnF.value().signature().returnTypeID() == sym.Int());
    auto const fnG = sym.declareFunction(Token("g", TokenType::Identifier));
    REQUIRE(fnG.hasValue());
    auto const overloadSuccess2 = sym.setSignature(fnG->symbolID(), gSig);
    REQUIRE(overloadSuccess2);
    CHECK(fnG.value().signature().argumentTypeID(0) == sym.Int());
    CHECK(fnG.value().signature().returnTypeID() == sym.Void());
}
