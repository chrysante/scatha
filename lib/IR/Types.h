#ifndef SCATHA_IR_TYPES_H_
#define SCATHA_IR_TYPES_H_

#include "Basic/Basic.h"

namespace scatha::ir {

enum class FundTypes {
    i8, i16, i32, i64,
    u8, u16, u32, u64,
    f32, f64,
};

} // namespace scatha::ir

#endif // SCATHA_IR_TYPES_H_

