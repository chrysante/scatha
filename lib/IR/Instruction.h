#ifndef SCATHA_INSTRUCTION_H_
#define SCATHA_INSTRUCTION_H_

#include "IR/Value.h"

namespace scatha::ir {
	
class Type;

class Instruction: public Value {
protected:
    explicit Instruction(Type const* type): Value(type) {}
    
private:
    
};
	
} // namespace scatha::ir

#endif // SCATHA_INSTRUCTION_H_

