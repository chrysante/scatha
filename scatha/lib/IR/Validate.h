#ifndef SCATHA_IR_VALIDATE_H_
#define SCATHA_IR_VALIDATE_H_

#include <utl/streammanip.hpp>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::ir {

class InvariantException: public Exception {
public:
    explicit InvariantException(utl::vstreammanip<> dynMessage);

    utl::vstreammanip<> const& format() const { return dynMessage; }

private:
    utl::vstreammanip<> dynMessage;
};

SCATHA_API void assertInvariants(Context& ctx, Module const& mod);

SCATHA_API void assertInvariants(Context& ctx, Function const& function);

} // namespace scatha::ir

#endif // SCATHA_IR_VALIDATE_H_
