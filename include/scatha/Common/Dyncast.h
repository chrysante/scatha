// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_COMMON_DYNCAST_H_
#define SCATHA_COMMON_DYNCAST_H_

#include <utl/dyncast.hpp>

#define SC_DYNCAST_MAP(Type, EnumValue) UTL_DYNCAST_MAP(Type, EnumValue)

namespace scatha {

using utl::cast;
using utl::dyncast;
using utl::isa;
using utl::visit;

} // namespace scatha

#endif // SCATHA_COMMON_DYNCAST_H_
