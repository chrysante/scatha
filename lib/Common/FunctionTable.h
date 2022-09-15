#ifndef SCATHA_COMMON_FUNCTIONTABLE_H_
#define SCATHA_COMMON_FUNCTIONTABLE_H_

#include <memory>
#include <string>

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include "Common/Type.h"

namespace scatha {
	
//	class TypeTable;
//	
//	struct Function {
//		TypeEx type;
//		std::string name;
//	};
//	
//	class FunctionTable {
//	public:
//		explicit FunctionTable(std::shared_ptr<TypeTable> typeTable);
//		void add(TypeEx const& type, std::string const& name);
//		utl::small_vector<Function> getFunction(std::string const& name) const;
//		
//	private:
//		std::shared_ptr<TypeTable> typeTable;
//		utl::hashmap<std::string, utl::small_vector<Function>> mutable functions;
//	};
	
}

#endif // SCATHA_COMMON_FUNCTIONTABLE_H_

