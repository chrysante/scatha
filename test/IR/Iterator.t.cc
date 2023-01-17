#include <Catch/Catch2.hpp>

#include <string>

#include "IR/CFG.h"
#include "IR/Iterator.h"
#include "test/IR/CompileToIR.h"

using namespace scatha;

TEST_CASE("Iterate over instructions in a function", "[codegen]") {
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
        for (auto&& [index, inst]: utl::enumerate(function.instructions())) {
            auto const type = reference[index];
            CHECK(inst.nodeType() == type);
        }
    }
    SECTION("Erase every second element") {
        auto const instructions = function.instructions();
        size_t k                = 0;
        for (auto itr = instructions.begin(); itr != instructions.end(); ++k) {
            if (k % 2 == 1) {
                itr = itr->parent()->instructions.erase(itr.instructionIterator());
            }
            else {
                ++itr;
            }
        }
        for (auto&& [index, inst]: utl::enumerate(function.instructions())) {
            auto const type = reference[2 * index];
            CHECK(inst.nodeType() == type);
        }
    }
    SECTION("Erase all") {
        auto const instructions = function.instructions();
        size_t k                = 0;
        for (auto itr = instructions.begin(); itr != instructions.end(); ++k) {
            itr = itr->parent()->instructions.erase(itr.instructionIterator());
        }
        CHECK(function.instructions().empty());
    }
}
