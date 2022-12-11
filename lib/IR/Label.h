#ifndef SCATHA_IR_LABEL_H_
#define SCATHA_IR_LABEL_H_

#include "IR/Value.h"

namespace scatha::ir {
	
class Label: public Value {
public:
    
private:
    size_t _index;
};
	
} // namespace scatha::ir

#endif // SCATHA_IR_LABEL_H_

