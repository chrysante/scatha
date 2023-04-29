#include <Catch/Catch2.hpp>

#include "Sema/Entity.h"
#include "Sema/FunctionSignature.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

TEST_CASE("FunctionSignature hash", "[sema]") {
    sema::SymbolTable sym;
    auto const i_i = sema::FunctionSignature({ sym.qualInt() }, sym.qualInt());
    auto const f_f_i =
        sema::FunctionSignature({ sym.qualFloat(), sym.qualFloat() },
                                sym.qualInt());
    auto const v_i = sema::FunctionSignature({}, sym.qualInt());
    CHECK(i_i.argumentHash() != f_f_i.argumentHash());
    CHECK(i_i.argumentHash() != v_i.argumentHash());
    CHECK(v_i.argumentHash() != f_f_i.argumentHash());
}

TEST_CASE("Function Type", "[sema]") {
    sema::SymbolTable sym;
    auto const fSig = sema::FunctionSignature({ sym.qualInt() }, sym.qualInt());
    auto const gSig =
        sema::FunctionSignature({ sym.qualInt() }, sym.qualVoid());
    CHECK(fSig.argumentHash() == gSig.argumentHash());
    auto* fnF                  = &sym.declareFunction("f").value();
    auto const overloadSuccess = sym.setSignature(fnF, fSig);
    REQUIRE(overloadSuccess);
    CHECK(fnF->signature().argumentType(0)->base() == sym.Int());
    CHECK(fnF->signature().returnType()->base() == sym.Int());
    auto const fnG              = &sym.declareFunction("g").value();
    auto const overloadSuccess2 = sym.setSignature(fnG, gSig);
    REQUIRE(overloadSuccess2);
    CHECK(fnG->signature().argumentType(0)->base() == sym.Int());
    CHECK(fnG->signature().returnType()->base() == sym.Void());
}
