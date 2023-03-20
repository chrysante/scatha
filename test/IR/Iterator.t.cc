#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>
#include <string>

#include "Common/UniquePtr.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Iterator.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "Opt/Common.h"

using namespace scatha;

TEST_CASE("Iterate over instructions in a function", "[ir][opt]") {
    auto const text = R"(
function i64 @ff(i64) {
  %entry:
    %n.addr = alloca i64
    store %n.addr, %0
    %k-ptr = alloca i64
    %n = load i64 %n.addr
    store %k-ptr, %n
    %k = load i64 %k-ptr
    %cmp.result = cmp eq i64 %k, i64 $0
    branch i1 %cmp.result, label %then, label %if.end
  %then:
    store %k-ptr, $1
    %tmp = load i64 %k-ptr
    goto label %if.end
  %if.end:
    %k.1 = load i64 %k-ptr
    return i64 %k.1
})";
    ir::Context ctx;
    auto mod                       = ir::parse(text, ctx).value();
    auto& function                 = mod.functions().front();
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
                opt::replaceValue(&itr.instruction(), ctx.undef(itr.instruction().type()));
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
    auto const text = R"(
function i64 @f() {
  %entry:
    goto label %header
  %header:
    %z = phi i64 [label %entry: $3], [label %body: $4]
    %y = phi i64 [label %entry: $2], [label %body: $3]
    %x = phi i64 [label %entry: $1], [label %body: $2]
    %sum = add i64 %z, %z
    %prod = mul i64 %z, %z
    goto label %body
  %body:
    goto label %header
})";
    ir::Context ctx;
    auto mod            = ir::parse(text, ctx).value();
    auto& f             = mod.functions().front();
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
