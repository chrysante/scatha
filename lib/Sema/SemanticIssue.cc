#include "Sema/SemanticIssue.h"

#include <ostream>

#include <utl/utility.hpp>

namespace scatha::sema {

std::ostream& operator<<(std::ostream& str, BadFunctionCall::Reason r) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(r, {
        { BadFunctionCall::Reason::NoMatchingFunction, "No matching function" },
        { BadFunctionCall::Reason::ObjectNotCallable,  "Object not callable" },
    });
    // clang-format on
}

std::ostream& operator<<(std::ostream& str, InvalidStatement::Reason r) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(r, {
        { InvalidStatement::Reason::ExpectedDeclaration,      "Expected declaration" },
        { InvalidStatement::Reason::InvalidDeclaration,       "Invalid declaration" },
        { InvalidStatement::Reason::InvalidScopeForStatement, "Invalid scope for statement" },
    });
    // clang-format on
}

std::ostream& operator<<(std::ostream& str, InvalidDeclaration::Reason r) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(r, {
        { InvalidDeclaration::Reason::InvalidInCurrentScope,    "Invalid in currentScope" },
        { InvalidDeclaration::Reason::Redefinition,             "Redefinition" },
        { InvalidDeclaration::Reason::CantOverloadOnReturnType, "Cant overload on ReturnType" },
        { InvalidDeclaration::Reason::CantInferType,            "Cant infer type" },
        { InvalidDeclaration::Reason::ReservedIdentifier,       "Reserved identifier" },
    });
    // clang-format on
}

} // namespace scatha::sema
