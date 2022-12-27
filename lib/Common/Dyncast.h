#ifndef SCATHA_COMMON_DYNCAST_H_
#define SCATHA_COMMON_DYNCAST_H_

#include <utl/dyncast.hpp>

#define SC_DYNCAST_MAP UTL_DYNCAST_MAP

namespace scatha {

using utl::isa;
using utl::cast;
using utl::dyncast;
using utl::visit;

} // namespace scatha

#endif // SCATHA_COMMON_DYNCAST_H_
