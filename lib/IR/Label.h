#ifndef SCATHA_IR_LABEL_H_
#define SCATHA_IR_LABEL_H_

#include "IR/StatementBase.h"

namespace scatha::ir {
	
class Label: public StatementBase {
public:
    
private:
    size_t _index;
};
	
} // namespace scatha::ir

#endif // SCATHA_IR_LABEL_H_

