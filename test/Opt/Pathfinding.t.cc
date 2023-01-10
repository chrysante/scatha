#include <Catch/Catch2.hpp>

#include <string>

#include "Opt/ControlFlowPath.h"
#include "Opt/Pathfinder.h"
#include "IR/CFG.h"
#include "test/IR/CompileToIR.h"

using namespace scatha;

TEST_CASE("Pathfinding", "[codegen]") {
    std::string const text = R"(
fn f() -> int {
    var k = 0;
    if true {
        
    }
    while true {
                           
    }
    return k;
})";
    auto mod = test::compileToIR(text);
    auto& f = mod.functions().front();
    auto& entry = f.basicBlocks().front();
    /// We should have a store as the second instruction because: \code
    ///     k-ptr = alloca i64
    ///     store %k-ptr, i64 $0
    auto const* const kStore = cast<ir::Store const*>(entry.instructions.front().next());
    auto& loopEnd = f.basicBlocks().back();
    /// We should have a load as the second last instruction because: \code
    ///     k = load i64 %k-ptr
    ///     return i64 %k
    auto const* const kLoad = cast<ir::Load const*>(loopEnd.instructions.back().prev());
    REQUIRE(kLoad->address() == kStore->dest());
    auto const paths = opt::findAllPaths(kStore, kLoad);
    REQUIRE(paths.size() == 4);
    utl::vector<utl::small_vector<ir::NodeType>> const reference = {
        { ir::NodeType::Store,
          ir::NodeType::Branch,
          ir::NodeType::Goto,
          ir::NodeType::Goto,
          ir::NodeType::Branch,
          ir::NodeType::Goto,
          ir::NodeType::Branch,
          ir::NodeType::Load },
        { ir::NodeType::Store,
          ir::NodeType::Branch,
          ir::NodeType::Goto,
          ir::NodeType::Goto,
          ir::NodeType::Branch,
          ir::NodeType::Load },
        { ir::NodeType::Store,
          ir::NodeType::Branch,
          ir::NodeType::Goto,
          ir::NodeType::Branch,
          ir::NodeType::Goto,
          ir::NodeType::Branch,
          ir::NodeType::Load },
        { ir::NodeType::Store,
          ir::NodeType::Branch,
          ir::NodeType::Goto,
          ir::NodeType::Branch,
          ir::NodeType::Load }
    };
    for (size_t index = 0; auto& path: paths) {
        REQUIRE(path.valid());
        auto const& ref = reference[index++];
        for (size_t index = 0; auto& inst: path) {
            CHECK(ref[index++] == inst.nodeType());
        }
    }
}
