#ifndef SCATHA_IR_STATEMENT_H_
#define SCATHA_IR_STATEMENT_H_

#include <variant>

#include "Basic/Basic.h"
#include "IR/VariableDeclaration.h"
#include "IR/Label.h"

namespace scatha::ir {
	
using StatementVariant = std::variant<VariableDeclaration, /*ThreeAddressStatement, */Label>;

class Statement: public StatementVariant {
public:
    using StatementVariant::StatementVariant;
    
    template <typename F>
    decltype(auto) visit(F&& f) const { return visitImpl(*this, std::forward<F>(f)); }
    
    template <typename F>
    decltype(auto) visit(F&& f) { return visitImpl(*this, std::forward<F>(f)); }
    
private:
    template <typename Self, typename F>
    static decltype(auto) visitImpl(Self&& self, F&& f) {
        return std::visit(std::forward<Self>(self), std::forward<F>(f));
    }
};
	
} // namespace scatha::ir

#endif // SCATHA_IR_STATEMENT_H_

