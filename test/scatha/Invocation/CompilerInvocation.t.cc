#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <svm/VirtualMachine.h>

#include "Invocation/CompilerInvocation.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

TEST_CASE("Target symbol table", "[invocation][end-to-end]") {
    CompilerInvocation inv(TargetType::BinaryOnly, "test");
    inv.setInputs({ SourceFile::make(R"(
public fn foo() -> int { return 42; }
public fn bar(n: int) -> int { return 2 * n; }
public struct Baz {
    fn baz() { return 7; }
}
)") });
    auto target = inv.run();
    REQUIRE(target);
    if (GENERATE(true, false)) {
        target->writeToDisk("test-targets/");
        target = Target::ReadFromDisk("test-targets/test.scbin");
        REQUIRE(target);
    }
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

TEST_CASE("Mapped memory", "[invocation][end-to-end]") {
    CompilerInvocation inv(TargetType::Executable, "test");
    inv.setInputs({ SourceFile::make(R"(
public fn foo(p: *int) -> bool {
    return *p == 42;
}
)") });
    auto target = inv.run();
    REQUIRE(target);
    svm::VirtualMachine vm;
    vm.loadBinary(target->binary().data());
    auto& sym = target->symbolTable();
    auto* foo = sym.globalScope().findFunctions("foo").front();
    int64_t argValue = 42;
    svm::VirtualPointer ptrArg = vm.mapMemory(&argValue, 8);
    uint64_t result =
        *vm.execute(foo->binaryAddress().value(),
                    std::array{ std::bit_cast<uint64_t>(ptrArg) });
    vm.unmapMemory(ptrArg);
    CHECK(result == 1);
}
