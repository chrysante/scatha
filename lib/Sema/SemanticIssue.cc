#include "Sema/SemanticIssue.h"

#include <ostream>

#include <utl/utility.hpp>

namespace scatha::sema {

	std::ostream& operator<<(std::ostream& str, BadFunctionCall::Reason r) {
		return str << UTL_SERIALIZE_ENUM(r, {
			{ BadFunctionCall::Reason::WrongArgumentCount, "Wrong argument count" },
			{ BadFunctionCall::Reason::NoMatchingFunction, "No matching function" },
			{ BadFunctionCall::Reason::ObjectNotCallable,  "Object not callable" },
		});
	}
	
	std::ostream& operator<<(std::ostream& str, InvalidStatement::Reason r) {
		return str << UTL_SERIALIZE_ENUM(r, {
			{ InvalidStatement::Reason::ExpectedDeclaration,      "Expected declaration" },
			{ InvalidStatement::Reason::InvalidDeclaration,       "Invalid declaration" },
			{ InvalidStatement::Reason::InvalidScopeForStatement, "Invalid scope for statement" },
		});
	}
	
	std::ostream& operator<<(std::ostream& str, InvalidDeclaration::Reason r) {
		return str << UTL_SERIALIZE_ENUM(r, {
			{ InvalidDeclaration::Reason::InvalidInCurrentScope,    "Invalid in currentScope" },
			{ InvalidDeclaration::Reason::Redeclaration,            "Redeclaration" },
			{ InvalidDeclaration::Reason::CantOverloadOnReturnType, "Cant overload on ReturnType" },
			{ InvalidDeclaration::Reason::CantInferType,            "Cant infer type" },
			
		});
	}
	
}
