#ifndef SCATHA_SEMA_FORMAT_H_
#define SCATHA_SEMA_FORMAT_H_

#include <utl/streammanip.hpp>

#include "AST/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

///
utl::vstreammanip<> format(Type const* type);

///
utl::vstreammanip<> format(QualType type);

///
utl::vstreammanip<> formatType(ast::Expression const* expr);

} // namespace scatha::sema

#endif // SCATHA_SEMA_FORMAT_H_
