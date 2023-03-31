#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>
#include <string>

#include "Common/UniquePtr.h"
#include "IR/CFG.h"
#include "IR/CFG/Iterator.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "Opt/Common.h"

using namespace scatha;

TEST_CASE("Iterate over instructions in a function", "[ir][opt]") {
    auto const text                = R"(
func i64 @ff(i64) {
  %entry:
    %n.addr = alloca i64
    store ptr %n.addr, i64 %0
    %k-ptr = alloca i64
    %n = load i64, ptr %n.addr
    store ptr %k-ptr, i64 %n
    %k = load i64, ptr %k-ptr
    %cmp.result = cmp eq i64 %k, i64 0
    branch i1 %cmp.result, label %then, label %if.end
  
  %then:
    store ptr %k-ptr, i64 1
    %tmp = load i64, ptr %k-ptr
    goto label %if.end
  
  %if.end:
    %k.1 = load i64, ptr %k-ptr
    return i64 %k.1
})";
    auto [ctx, mod]                = ir::parse(text).value();
    auto& function                 = mod.front();
    ir::NodeType const reference[] = {
        ir::NodeType::Alloca,      ir::NodeType::Store,  ir::NodeType::Alloca,
        ir::NodeType::Load,        ir::NodeType::Store,  ir::NodeType::Load,
        ir::NodeType::CompareInst, ir::NodeType::Branch, ir::NodeType::Store,
        ir::NodeType::Load,        ir::NodeType::Goto,   ir::NodeType::Load,
        ir::NodeType::Return,
    };
    SECTION("Simple traversal") {
        for (auto&& [index, inst]:
             function.instructions() | ranges::views::enumerate)
        {
            auto const type = reference[index];
            CHECK(inst.nodeType() == type);
        }
    }
    SECTION("Erase every second element") {
        auto const instructions = function.instructions();
        size_t k                = 0;
        for (auto itr = instructions.begin(); itr != instructions.end(); ++k) {
            if (k % 2 == 1) {
                opt::replaceValue(&itr.instruction(),
                                  ctx.undef(itr.instruction().type()));
                itr = itr->parent()->erase(itr.instructionIterator());
            }
            else {
                ++itr;
            }
        }
        for (auto&& [index, inst]:
             ranges::views::enumerate(function.instructions()))
        {
            auto const type = reference[2 * index];
            CHECK(inst.nodeType() == type);
        }
    }
    SECTION("Erase all") {
        auto const instructions = function.instructions();
        size_t k                = 0;
        /// We first clear all the operands to prevent the next pass from using
        /// deallocated memory.
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
    auto const text     = R"(
func i64 @f() {
  %entry:
    goto label %header

  %header:
    %z = phi i64 [label %entry: 3], [label %body: 4]
    %y = phi i64 [label %entry: 2], [label %body: 3]
    %x = phi i64 [label %entry: 1], [label %body: 2]
    %sum = add i64 %z, i64 %z
    %prod = mul i64 %z, i64 %z
    goto label %body
  
  %body:
    goto label %header
})";
    auto [ctx, mod]     = ir::parse(text).value();
    auto& f             = mod.front();
    auto& header        = *f.front().next();
    auto headerPhiNodes = header.phiNodes();
    for (auto itr = headerPhiNodes.begin(); itr != headerPhiNodes.end();) {
        if (itr->name() == "y") {
            itr = header.erase(itr);
        }
        else {
            ++itr;
        }
    }
    CHECK(ranges::distance(header) == 5);
    header.eraseAllPhiNodes();
    CHECK(ranges::distance(header) == 3);
}
