#include <Catch/Catch2.hpp>

#include <string>

#include "Opt/ControlFlowPath.h"
#include "IR/CFG.h"
#include "test/IR/CompileToIR.h"

using namespace scatha;

TEST_CASE("Iterate over control flow path", "[codegen]") {
    std::string const text = R"(
fn f(n: int) -> int {
    var k = n;
    if k == 0 {
        k = 1;
    }
    return k;
})";
    auto mod = test::compileToIR(text);
    auto& f = mod.functions().front();
    auto& entry = f.basicBlocks().front();
    auto const* const kStore = entry.instructions.front()
        .next()->next()->next()->next();
    REQUIRE(kStore->name().empty());
    REQUIRE(kStore->nodeType() == ir::NodeType::Store);
    auto& ifEnd = f.basicBlocks().back();
    auto const* const kLoad = ifEnd.instructions.back().prev();
    REQUIRE(kLoad->name() == "k-1");
    REQUIRE(kLoad->nodeType() == ir::NodeType::Load);
    opt::ControlFlowPath const path(kStore, {
        &entry,
        entry.next(),
        &ifEnd
    }, kLoad);
    REQUIRE(path.valid());
    struct Reference {
        ir::NodeType type;
        std::string_view name;
    };
    Reference const reference[] = { // clang-format off
        { ir::NodeType::Store,       "" },
        { ir::NodeType::Load,        "k" },
        { ir::NodeType::CompareInst, "cmp-result" },
        { ir::NodeType::Branch,      "" },
        { ir::NodeType::Store,       "" },
        { ir::NodeType::Load,        "tmp" },
        { ir::NodeType::Goto,        "" },
        { ir::NodeType::Load,        "k-1" },
    }; // clang-format on
    SECTION("Forward traversal") {
        for (auto&& [index, inst]: utl::enumerate(path)) {
            auto& ref = reference[index];
            CHECK(inst.nodeType() == ref.type);
            CHECK(inst.name() == ref.name);
        }
    }
    SECTION("Reverse traversal") {
        for (size_t index = std::size(reference); auto& inst: utl::reverse(path)) {
            auto& ref = reference[--index];
            CHECK(inst.nodeType() == ref.type);
            CHECK(inst.name() == ref.name);
        }
    }
}
