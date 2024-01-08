#ifndef SCATHA_SEMA_FORMAT_H_
#define SCATHA_SEMA_FORMAT_H_

#include <utl/streammanip.hpp>

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

///
SCATHA_API utl::vstreammanip<> format(Entity const* entity);

///
SCATHA_API void print(Entity const* entity, std::ostream&);

///
SCATHA_API void print(Entity const* entity);

///
SCATHA_API utl::vstreammanip<> format(QualType type);

///
SCATHA_API utl::vstreammanip<> formatType(ast::Expression const* expr);

} // namespace scatha::sema

#endif // SCATHA_SEMA_FORMAT_H_
