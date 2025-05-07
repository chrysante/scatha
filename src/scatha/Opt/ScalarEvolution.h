#ifndef SCATHA_OPT_SCALAREVOLUTION_H_
#define SCATHA_OPT_SCALAREVOLUTION_H_

#include <iosfwd>

#include <utl/hashtable.hpp>
#include <utl/streammanip.hpp>

#include "Common/APInt.h"
#include "Common/Dyncast.h"
#include "Common/UniquePtr.h"
#include "IR/Fwd.h"
#include "IR/ValueRef.h"

namespace scatha::opt {

enum class ScevOperation { Add, Mul };

enum class ScevExprKind {
#define SC_OPT_SCEV_DEF(Name, ...) Name,
#include "Opt/SCEV.def.h"
};

/// Base class of all scev expressions
class ScevExpr: public csp::base_helper<ScevExpr, ScevExprKind> {
protected:
    using base_helper::base_helper;
};

class ScevNullaryExpr: public ScevExpr {
protected:
    using ScevExpr::ScevExpr;
};

class ScevConstExpr: public ScevNullaryExpr {
public:
    ScevConstExpr(APInt value):
        ScevNullaryExpr(ScevExprKind::ScevConstExpr),
        _value(std::move(value)) {}

    APInt const& value() const { return _value; }

private:
    APInt _value;
};

class ScevUnknownExpr: public ScevNullaryExpr {
public:
    ScevUnknownExpr(ir::ValueRef value):
        ScevNullaryExpr(ScevExprKind::ScevUnknownExpr),
        _value(std::move(value)) {}

    ir::Value* value() const { return _value.value(); }

private:
    ir::ValueRef _value;
};

class ScevArithmeticExpr: public ScevExpr {
public:
    ScevNullaryExpr* LHS() { return _lhs.get(); }
    ScevNullaryExpr const* LHS() const { return _lhs.get(); }

    ScevExpr* RHS() { return _rhs.get(); }
    ScevExpr const* RHS() const { return _rhs.get(); }

    ///
    ScevOperation operation() const;

protected:
    ScevArithmeticExpr(ScevExprKind kind, UniquePtr<ScevNullaryExpr> lhs,
                       UniquePtr<ScevExpr> rhs):
        ScevExpr(kind), _lhs(std::move(lhs)), _rhs(std::move(rhs)) {}

private:
    UniquePtr<ScevNullaryExpr> _lhs;
    UniquePtr<ScevExpr> _rhs;
};

class ScevAddExpr: public ScevArithmeticExpr {
public:
    ScevAddExpr(UniquePtr<ScevNullaryExpr> lhs, UniquePtr<ScevExpr> rhs):
        ScevArithmeticExpr(ScevExprKind::ScevAddExpr, std::move(lhs),
                           std::move(rhs)) {}
};

class ScevMulExpr: public ScevArithmeticExpr {
public:
    ScevMulExpr(UniquePtr<ScevNullaryExpr> lhs, UniquePtr<ScevExpr> rhs):
        ScevArithmeticExpr(ScevExprKind::ScevMulExpr, std::move(lhs),
                           std::move(rhs)) {}
};

using NoParent = void;

} // namespace scatha::opt

#define SC_OPT_SCEV_DEF(Name, Parent, Corporeality)                            \
    SC_DYNCAST_DEFINE(scatha::opt::Name, scatha::opt::ScevExprKind::Name,      \
                      scatha::opt::Parent, Corporeality)
#include "Opt/SCEV.def.h"

namespace scatha::opt {

/// \Returns a deep copy of \p expr
UniquePtr<ScevExpr> clone(ScevExpr const& expr);

/// \overload
template <std::derived_from<ScevExpr> Expr>
UniquePtr<Expr> clone(Expr const& expr) {
    auto copy = clone((ScevExpr const&)expr);
    auto* raw = cast<Expr*>(copy.get());
    copy.release();
    return UniquePtr<Expr>(raw);
}

///
utl::vstreammanip<> format(ScevExpr const& expr);

/// Pretty-prints \p expr to `std::cout`
void print(ScevExpr const& expr);

/// \overload for arbitrary ostream
void print(ScevExpr const& expr, std::ostream& ostream);

///
void scev(ir::Context& ctx, ir::LNFNode& loop);

}; // namespace scatha::opt

#endif // SCATHA_OPT_SCALAREVOLUTION_H_
