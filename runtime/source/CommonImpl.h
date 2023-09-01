#ifndef SCATHA_RUNTIME_COMMONIMPL_H_
#define SCATHA_RUNTIME_COMMONIMPL_H_

#include <span>
#include <string>

#include <scatha/Runtime/Common.h>
#include <scatha/Sema/Fwd.h>

namespace scatha {

sema::QualType toSemaType(sema::SymbolTable& sym, QualType type);

sema::FunctionSignature toSemaSig(sema::SymbolTable& sym,
                                  QualType returnType,
                                  std::span<QualType const> argTypes);

std::string mangleFunctionName(std::string_view name,
                               std::span<QualType const> argTypes);

} // namespace scatha

#endif // SCATHA_RUNTIME_COMMONIMPL_H_
