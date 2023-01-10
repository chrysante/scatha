#include <Catch/Catch2.hpp>

#include <string>

#include "IR/Iterator.h"
#include "IR/CFG.h"
#include "test/IR/CompileToIR.h"

using namespace scatha;

TEST_CASE("Iterate over instructions in a function", "[codegen]") {
    std::string const text = R"(
fn f(n: int) -> int {
    var k = n;
    if k == 0 {
        k = 1;
    }
    return k;
})";
    auto mod = test::compileToIR(text);
    auto& function = mod.functions().front();
    struct Reference {
        ir::NodeType type;
        std::string_view name;
    };
    Reference const reference[] = { // clang-format off
        { ir::NodeType::Alloca,      "n-ptr" },
        { ir::NodeType::Store,       "" },
        { ir::NodeType::Alloca,      "k-ptr" },
        { ir::NodeType::Load,        "n" },
        { ir::NodeType::Store,       "" },
        { ir::NodeType::Load,        "k" },
        { ir::NodeType::CompareInst, "cmp-result" },
        { ir::NodeType::Branch,      "" },
        { ir::NodeType::Store,       "" },
        { ir::NodeType::Load,        "tmp" },
        { ir::NodeType::Goto,        "" },
        { ir::NodeType::Load,        "k-1" },
        { ir::NodeType::Return,      "" },
    }; // clang-format on
    SECTION("Simple traversal") {
        for (auto&& [index, inst]: utl::enumerate(function.instructions())) {
            auto const ref = reference[index];
            CHECK(inst.nodeType() == ref.type);
            CHECK(inst.name() == ref.name);
        }
    }
    SECTION("Erase every second element") {
        auto const instructions = function.instructions();
        size_t k = 0;
        for (auto itr = instructions.begin(); itr != instructions.end(); ++k) {
            if (k % 2 == 1) {
                itr = itr->parent()->instructions.erase(itr.instructionIterator());
            }
            else {
                ++itr;
            }
        }
        for (auto&& [index, inst]: utl::enumerate(function.instructions())) {
            auto const ref = reference[2 * index];
            CHECK(inst.nodeType() == ref.type);
            CHECK(inst.name() == ref.name);
        }
    }
    SECTION("Erase all") {
        auto const instructions = function.instructions();
        size_t k = 0;
        for (auto itr = instructions.begin(); itr != instructions.end(); ++k) {
            itr = itr->parent()->instructions.erase(itr.instructionIterator());
        }
        CHECK(function.instructions().empty());
    }
}
