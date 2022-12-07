#ifndef SCATHA_IR_STATEMENTBASE_H_
#define SCATHA_IR_STATEMENTBASE_H_

#include "Basic/Basic.h"

namespace scatha::ir {
	
class StatementBase {
public:
    size_t index() const { return _index; }
    
protected:
    explicit StatementBase(size_t index): _index(index) {}
    
private:
    size_t _index;
};

	
} // namespace scatha::ir

#endif // SCATHA_IR_STATEMENTBASE_H_

