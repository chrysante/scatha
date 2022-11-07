#pragma once

#ifndef SCATHA_COMMON_SOURCELOCATION_H_
#define SCATHA_COMMON_SOURCELOCATION_H_

#include <iosfwd>

#include "Basic/Basic.h"

namespace scatha {

struct SourceLocation {
    i64 index = 0;
    i32 line = 0, column = 0;
};

SCATHA(API) std::ostream& operator<<(std::ostream&, SourceLocation const&);

} // namespace scatha

#endif // SCATHA_COMMON_SOURCELOCATION_H_
