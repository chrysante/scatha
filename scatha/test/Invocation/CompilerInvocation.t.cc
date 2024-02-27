#include <catch2/catch_test_macros.hpp>

#include <svm/VirtualMachine.h>

#include "Invocation/CompilerInvocation.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

TEST_CASE("Target symbol table", "[invocation]") {
    CompilerInvocation inv(TargetType::Executable, "test");
    inv.setInputs({ SourceFile::make(R"(
public fn foo() -> int { return 42; }
public fn bar(n: int) -> int { return 2 * n; }
public struct Baz {
    fn baz() { return 7; }
}
)") });
    auto target = inv.run();
    REQUIRE(target);
    svm::VirtualMachine vm;
    vm.loadBinary(target->binary().data());
    auto& sym = target->symbolTable();
    auto* foo = sym.globalScope().findFunctions("foo").front();
    CHECK(*vm.execute(foo->binaryAddress().value(), {}) == 42);

    auto* bar = sym.globalScope().findFunctions("bar").front();
    CHECK(*vm.execute(bar->binaryAddress().value(),
                      std::array{ uint64_t{ 21 } }) == 42);

    auto* Baz = cast<sema::StructType const*>(
        sema::stripAlias(sym.globalScope().findEntities("Baz").front()));
    auto* baz = Baz->findFunctions("baz").front();
    CHECK(*vm.execute(baz->binaryAddress().value(), {}) == 7);
}
