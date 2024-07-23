#include "Opt/ScalarEvolution.h"

#include "IR/Loop.h"
#include "IR/Print.h"

#include <iostream>

using namespace scatha;
using namespace ir;
using namespace opt;

ScevOperation ScevArithmeticExpr::operation() const {
    using enum ScevOperation;
    return SC_MATCH (*this){ [](ScevAddExpr const&) { return Add; },
                             [](ScevMulExpr const&) { return Mul; } };
}

static void write(std::ostream& str, ScevExpr const& expr);

static void writeImpl(std::ostream& str, ScevConstExpr const& expr) {
    str << expr.value().signedToString();
}

static void writeImpl(std::ostream& str, ScevUnknownExpr const& expr) {
    str << formatName(*expr.value());
}

static std::string_view toSpelling(ScevOperation op) {
    using enum ScevOperation;
    switch (op) {
    case Add:
        return "+";
    case Mul:
        return "*";
    }
}

static void writeImpl(std::ostream& str, ScevArithmeticExpr const& expr) {
    write(str, *expr.LHS());
    str << ", " << toSpelling(expr.operation()) << ", ";
    write(str, *expr.RHS());
}

static void write(std::ostream& str, ScevExpr const& expr) {
    visit(expr, [&](auto& expr) { writeImpl(str, expr); });
}

utl::vstreammanip<> opt::format(ScevExpr const& expr) {
    return [&](std::ostream& str) {
        str << "{ ";
        write(str, expr);
        str << " }";
    };
}

void opt::print(ScevExpr const& expr) { print(expr, std::cout); }

void opt::print(ScevExpr const& expr, std::ostream& str) {
    str << format(expr) << std::endl;
}

utl::hashmap<Instruction*, UniquePtr<ScevExpr>> opt::scev(Context& ctx,
                                                          LoopInfo& loop) {
    (void)ctx;
    for (auto* var: loop.inductionVariables()) {
        (void)var;
    }
    return {};
}
