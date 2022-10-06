#include <Catch/Catch2.hpp>

#include "Sema/FunctionSignature.h"
#include "Sema/SymbolTable.h"

#include <iostream>

using namespace scatha;

TEST_CASE("FunctionSignature hash", "[sema]") {
    sema::SymbolTable sym;
    auto const        i_i   = sema::FunctionSignature({sym.Int()}, sym.Int());
    auto const        f_f_i = sema::FunctionSignature({sym.Float(), sym.Float()}, sym.Int());
    auto const        v_i   = sema::FunctionSignature({}, sym.Int());

    CHECK(i_i.argumentHash() != f_f_i.argumentHash());
    CHECK(i_i.argumentHash() != v_i.argumentHash());
    CHECK(v_i.argumentHash() != f_f_i.argumentHash());
}

TEST_CASE("Function Type", "[sema]") {
    sema::SymbolTable sym;
    auto const        fSig = sema::FunctionSignature({sym.Int()}, sym.Int());
    auto const        gSig = sema::FunctionSignature({sym.Int()}, sym.Void());

    CHECK(fSig.argumentHash() == gSig.argumentHash());

    auto const fnF = sym.addFunction(Token("f"), fSig);
    REQUIRE(fnF.hasValue());
    CHECK(fnF.value().signature().argumentTypeID(0) == sym.Int());
    CHECK(fnF.value().signature().returnTypeID() == sym.Int());
    auto const fnG = sym.addFunction(Token("g"), gSig);
    REQUIRE(fnG.hasValue());
    CHECK(fnG.value().signature().argumentTypeID(0) == sym.Int());
    CHECK(fnG.value().signature().returnTypeID() == sym.Void());
}
