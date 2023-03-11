#include <Catch/Catch2.hpp>

#include <string>

#include "Common/UniquePtr.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Iterator.h"
#include "test/IR/CompileToIR.h"

using namespace scatha;

TEST_CASE("Iterate over instructions in a function", "[ir][opt]") {
    std::string const text         = R"(
fn f(n: int) -> int {
    var k = n;
    if k == 0 {
        k = 1;
    }
    return k;
})";
    auto mod                       = test::compileToIR(text);
    auto& function                 = mod.functions().front();
    ir::NodeType const reference[] = {
        ir::NodeType::Alloca, ir::NodeType::Store,       ir::NodeType::Alloca, ir::NodeType::Load,  ir::NodeType::Store,
        ir::NodeType::Load,   ir::NodeType::CompareInst, ir::NodeType::Branch, ir::NodeType::Store, ir::NodeType::Load,
        ir::NodeType::Goto,   ir::NodeType::Load,        ir::NodeType::Return,
    };
    SECTION("Simple traversal") {
        for (auto&& [index, inst]: ranges::views::enumerate(function.instructions())) {
            auto const type = reference[index];
            CHECK(inst.nodeType() == type);
        }
    }
    SECTION("Erase every second element") {
        auto const instructions = function.instructions();
        size_t k                = 0;
        for (auto itr = instructions.begin(); itr != instructions.end(); ++k) {
            if (k % 2 == 1) {
                itr = itr->parent()->erase(itr.instructionIterator());
            }
            else {
                ++itr;
            }
        }
        for (auto&& [index, inst]: ranges::views::enumerate(function.instructions())) {
            auto const type = reference[2 * index];
            CHECK(inst.nodeType() == type);
        }
    }
    SECTION("Erase all") {
        auto const instructions = function.instructions();
        size_t k                = 0;
        /// We first clear all the operands to prevent the next pass from using deallocated memory.
        for (auto& inst: instructions) {
            inst.clearOperands();
        }
        for (auto itr = instructions.begin(); itr != instructions.end(); ++k) {
            itr = itr->parent()->erase(itr.instructionIterator());
        }
        CHECK(function.instructions().empty());
    }
}

TEST_CASE("Phi iterator", "[ir][opt]") {
    using namespace ir;
    ir::Context ctx;
    UniquePtr f = allocate<Function>(nullptr, ctx.voidType(), std::span<Type const*>{}, "f");
    auto* entry = new BasicBlock(ctx, "entry");
    f->basicBlocks().push_back(entry);
    auto* header = new BasicBlock(ctx, "header");
    f->basicBlocks().push_back(header);
    auto* body = new BasicBlock(ctx, "body");
    f->basicBlocks().push_back(body);
    entry->pushBack(new Goto(ctx, header));
    header->pushBack(new Goto(ctx, body));
    body->pushBack(new Goto(ctx, header));
    auto* x = new Phi({ { entry, ctx.integralConstant(1, 64) }, { body, ctx.integralConstant(2, 64) } }, "x");
    header->pushFront(x);
    auto* y = new Phi({ { entry, ctx.integralConstant(2, 64) }, { body, ctx.integralConstant(3, 64) } }, "y");
    header->pushFront(y);
    auto* z = new Phi({ { entry, ctx.integralConstant(3, 64) }, { body, ctx.integralConstant(4, 64) } }, "z");
    header->pushFront(z);
    header->insert(std::prev(header->end()), new ArithmeticInst(x, z, ArithmeticOperation::Add, "sum"));
    header->insert(std::prev(header->end()), new ArithmeticInst(x, z, ArithmeticOperation::Mul, "prod"));
    auto headerPhiNodes = header->phiNodes();
    for (auto itr = headerPhiNodes.begin(); itr != headerPhiNodes.end();) {
        if (itr->name() == "y") {
            itr = header->erase(itr);
        }
        else {
            ++itr;
        }
    }
    CHECK(std::count_if(header->begin(), header->end(), [](auto&&) { return true; }) == 5);
    header->eraseAllPhiNodes();
    CHECK(std::count_if(header->begin(), header->end(), [](auto&&) { return true; }) == 3);
}
