#ifndef SCATHA_IR_FUNCTION_H_
#define SCATHA_IR_FUNCTION_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/VariableDeclaration.h"
#include "IR/Statement.h"
#include "IR/Entity.h"

namespace scatha::ir {

class Function: public Entity {
public:
    explicit Function(utl::vector<VariableDeclaration> params, utl::vector<Statement> statements):
        params(std::move(params)), statements(std::move(statements))
    {}
    
private:
    utl::vector<VariableDeclaration> params;
    utl::vector<Statement> statements;
};
	
} // namespace scatha::ir

#endif // SCATHA_IR_FUNCTION_H_

