#ifndef SCATHA_SEMA_NAMEMANGLING_H_
#define SCATHA_SEMA_NAMEMANGLING_H_

#include "Sema/Fwd.h"

namespace scatha::sema {

std::string mangleName(Entity const* entity);

} // namespace scatha::sema

#endif // SCATHA_SEMA_NAMEMANGLING_H_
