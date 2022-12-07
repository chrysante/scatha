#ifndef SCATHA_IR_STRUCT_H_
#define SCATHA_IR_STRUCT_H_

#include <span>

#include <utl/vector.hpp>

#include "IR/VariableDeclaration.h"
#include "IR/Entity.h"

namespace scatha::ir {
	
class Struct: public Entity {
public:
    explicit Struct(utl::vector<VariableDeclaration> vars):
        vars(std::move(vars))
    {}
    
    std::span<VariableDeclaration const> variables() const { return vars; }
    
private:
    utl::vector<VariableDeclaration> vars;
};
	
} // namespace scatha::ir

#endif // SCATHA_IR_STRUCT_H_

