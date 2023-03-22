#ifndef SCATHA_IR_IRPRINT_H_
#define SCATHA_IR_IRPRINT_H_

#include <iosfwd>

#include "Basic/Basic.h"

namespace scatha::ir {

class Module;
class Value;
class Function;
class Instruction;

SCATHA_API void print(Module const& program);

SCATHA_API void print(Module const& program, std::ostream& ostream);

SCATHA_API void print(Function const& function);

SCATHA_API void print(Function const& function, std::ostream& ostream);

SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    Instruction const& inst);

SCATHA_API std::string toString(Value const& value);

} // namespace scatha::ir

#endif // SCATHA_IR_IRPRINT_H_
