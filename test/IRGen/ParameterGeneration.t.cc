#include <string>
#include <vector>
#include <span>
#include <stdexcept>
#include <sstream>

#include <range/v3/view.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Sema/SymbolTable.h"
#include "Sema/Analyze.h"
#include "Parser/Parser.h"
#include "IRGen/IRGen.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/CFG.h"
#include "IR/Type.h"
#include "Common/SourceFile.h"
#include "Issue/IssueHandler.h"

using namespace scatha;

static void validateEmpty(std::span<SourceFile const> sources,
                          IssueHandler const& issues) {
    if (issues.empty()) {
        return;
    }
    std::stringstream sstr;
    issues.print(sources, sstr);
    throw std::runtime_error(sstr.str());
}

static std::pair<ir::Context, ir::Module> makeIR(std::vector<std::string> sourceTexts) {
    IssueHandler issues;
    auto sourceFiles = sourceTexts | ranges::views::transform([](auto& text) {
        return SourceFile::make(std::move(text));
    }) | ranges::to<std::vector>;
    auto ast = parser::parse(sourceFiles, issues);
    validateEmpty(sourceFiles, issues);
    sema::SymbolTable sym;
    auto analysisResult = sema::analyze(*ast, sym, issues);
    validateEmpty(sourceFiles, issues);
    ir::Context ctx;
    ir::Module mod;
    irgen::generateIR(ctx, mod, *ast, sym, analysisResult, {});
    return { std::move(ctx), std::move(mod) };
}

static ir::Type const* arrayPtrType(ir::Context& ctx) {
    return ctx.anonymousStruct({ ctx.ptrType(), ctx.intType(64) });
}

TEST_CASE("Parameter generation", "[irgen]") {
    using namespace ir;
    SECTION("Static array pointer") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: *[int, 3]) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 1);
        auto itr = F.front().begin();

        auto& allocaInst = dyncast<Alloca const&>(*itr++);
        CHECK(allocaInst.allocatedType() == ctx.ptrType());
        CHECK(allocaInst.count() == ctx.intConstant(1, 32));

        auto& store = dyncast<Store const&>(*itr++);
        CHECK(store.address() == &allocaInst);
        CHECK(store.value() == &F.parameters().front());

        CHECK(isa<Return>(*itr++));
    }
    SECTION("Dynamic array pointer") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: *[int]) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 2);
        auto itr = F.front().begin();

        auto& allocaInst = dyncast<Alloca const&>(*itr++);
        CHECK(allocaInst.allocatedType() == arrayPtrType(ctx));
        CHECK(allocaInst.count() == ctx.intConstant(1, 32));

        CHECK(isa<InsertValue>(*itr++));

        auto& packedValue = dyncast<InsertValue const&>(*itr++);
        CHECK(packedValue.type() == arrayPtrType(ctx));

        auto& store = dyncast<Store const&>(*itr++);
        CHECK(store.address() == &allocaInst);
        CHECK(store.value() == &packedValue);

        CHECK(isa<Return>(*itr++));
    }
    SECTION("Big object") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: [int, 10]) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 1);
        CHECK(F.parameters().front().type() == ctx.ptrType());
        CHECK(F.entry().emptyExceptTerminator());
    }
    SECTION("Reference to int") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: &int) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 1);
        CHECK(F.parameters().front().type() == ctx.ptrType());
        CHECK(F.entry().emptyExceptTerminator());
    }
    SECTION("Reference to dynamic array") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: &[int]) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 2);
        CHECK(F.parameters().front().type() == ctx.ptrType());
        CHECK(F.parameters().back().type() == ctx.intType(64));
        CHECK(F.entry().emptyExceptTerminator());
    }
    
    SECTION("Reference to dynamic array pointer") {
        auto [ctx, mod] = makeIR({ "public fn foo(data: &*[int]) {}" });
        auto& F = mod.front();
        CHECK(ranges::distance(F.parameters()) == 1);
        CHECK(F.parameters().front().type() == ctx.ptrType());
        CHECK(F.entry().emptyExceptTerminator());
    }
}
