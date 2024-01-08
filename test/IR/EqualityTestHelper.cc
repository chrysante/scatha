#include "EqualityTestHelper.h"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Type.h"

using namespace scatha;
using namespace test;
using namespace test::ir;

void StructureEqTester::test(scatha::ir::StructType const& structure) const {
    CHECK(structure.name() == name);
    CHECK(structure.numElements() == memberTypenames.size());
    for (auto&& [member, name]:
         ranges::views::zip(structure.members(), memberTypenames))
    {
        CHECK(member.type->name() == name);
    }
}

void InstructionEqTester::test(scatha::ir::Instruction const& inst) const {
    CHECK(inst.name() == name);
    for (auto& refName: referedNames) {
        auto const operands = inst.operands();
        auto itr =
            ranges::find_if(operands, [&](scatha::ir::Value const* operand) {
            return operand->name() == refName;
        });
        CHECK(itr != ranges::end(operands));
    }
}

void BasicBlockEqTester::test(scatha::ir::BasicBlock const& basicBlock) const {
    CHECK(basicBlock.name() == name);
    CHECK(ranges::distance(basicBlock) == (ssize_t)instTesters.size());
    for (auto&& [inst, tester]: ranges::views::zip(basicBlock, instTesters)) {
        tester.test(inst);
    }
}

void FunctionEqTester::test(scatha::ir::Function const& function) const {
    CHECK(function.name() == name);
    CHECK(ranges::distance(function.parameters()) ==
          (ssize_t)paramTypenames.size());
    for (auto&& [param, typeName]:
         ranges::views::zip(function.parameters(), paramTypenames))
    {
        CHECK(param.type()->name() == typeName);
    }
    CHECK(ranges::distance(function) == (ssize_t)bbTesters.size());
    for (auto&& [basicBlock, tester]: ranges::views::zip(function, bbTesters)) {
        tester.test(basicBlock);
    }
}

void ModuleEqTester::testStructures(scatha::ir::Module const& mod) const {
    CHECK(mod.structures().size() == structs.size());
    for (auto&& [structure, tester]:
         ranges::views::zip(mod.structures(), structs))
    {
        tester.test(*structure);
    }
}

void ModuleEqTester::testFunctions(scatha::ir::Module const& mod) const {
    CHECK(ranges::distance(mod) == (ssize_t)funcs.size());
    for (auto&& [function, tester]: ranges::views::zip(mod, funcs)) {
        tester.test(function);
    }
}
