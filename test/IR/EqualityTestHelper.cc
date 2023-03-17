#include "EqualityTestHelper.h"

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "IR/Module.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace test;
using namespace test::ir;

#define CHECK(cond) assert(cond)

void StructureEqTester::test(scatha::ir::StructureType const& structure) const {
    CHECK(structure.name() == name);
    CHECK(structure.members().size() == memberTypenames.size());
    for (auto&& [memType, name]: ranges::views::zip(structure.members(), memberTypenames)) {
        CHECK(memType->name() == name);
    }
}

void InstructionEqTester::test(scatha::ir::Instruction const& inst) const {
    CHECK(inst.name() == name);
    for (auto& refName: referedNames) {
        auto const operands = inst.operands();
        auto itr = ranges::find_if(operands, [&](scatha::ir::Value const* operand) {
            return operand->name() == refName;
        });
        CHECK(itr != ranges::end(operands));
    }
}

void BasicBlockEqTester::test(scatha::ir::BasicBlock const& basicBlock) const {
    CHECK(basicBlock.name() == name);
    CHECK(ranges::distance(basicBlock) == instTesters.size());
    for (auto&& [inst, tester]: ranges::views::zip(basicBlock, instTesters)) {
        tester.test(inst);
    }
}

void FunctionEqTester::test(scatha::ir::Function const& function) const {
    CHECK(function.name() == name);
    CHECK(ranges::distance(function.parameters()) == paramTypenames.size());
    for (auto&& [param, typeName]:
         ranges::views::zip(function.parameters(), paramTypenames))
    {
        CHECK(param.type()->name() == typeName);
    }
    CHECK(ranges::distance(function) == bbTesters.size());
    for (auto&& [basicBlock, tester]: ranges::views::zip(function, bbTesters)) {
        tester.test(basicBlock);
    }
}

void ModuleEqTester::testStructures(scatha::ir::Module const& mod) const {
    CHECK(mod.structures().size() == structs.size());
    for (auto&& [structure, tester]: ranges::views::zip(mod.structures(), structs)) {
        tester.test(*structure);
    }
}

void ModuleEqTester::testFunctions(scatha::ir::Module const& mod) const {
    CHECK(ranges::distance(mod.functions()) == funcs.size());
    for (auto&& [function, tester]: ranges::views::zip(mod.functions(), funcs)) {
        tester.test(function);
    }
}
