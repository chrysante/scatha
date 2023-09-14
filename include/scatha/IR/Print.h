#ifndef SCATHA_IR_IRPRINT_H_
#define SCATHA_IR_IRPRINT_H_

#include <iosfwd>
#include <string>

#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Print module \p mod to `std::cout`
SCATHA_API void print(Module const& mod);

/// Print module \p mod to \p ostream
SCATHA_API void print(Module const& mod, std::ostream& ostream);

/// Print function \p function to `std::cout`
SCATHA_API void print(Callable const& function);

/// Print function \p function to \p ostream
SCATHA_API void print(Callable const& function, std::ostream& ostream);

/// Print instruction \p inst to `std::cout`
SCATHA_API void print(Instruction const& inst);

/// Print instruction \p inst to \p ostream
SCATHA_API void print(Instruction const& inst, std::ostream& ostream);


/// Print instruction \p inst to \p ostream
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    Instruction const& inst);

/// Print type \p type to `std::cout`
SCATHA_API void print(Type const& type);

/// Print type \p type to \p ostream
SCATHA_API void print(Type const& type, std::ostream& ostream);

/// Print the formatted typename of \p type to \p ostream
SCATHA_API std::ostream& operator<<(std::ostream& ostream, Type const& type);

/// Format value \p value to a string
SCATHA_API std::string toString(Value const* value);

} // namespace scatha::ir

#endif // SCATHA_IR_IRPRINT_H_
