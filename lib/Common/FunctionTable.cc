#include "Common/FunctionTable.h"

#include "Common/TypeTable.h"

namespace scatha {
	
	FunctionTable::FunctionTable(std::shared_ptr<TypeTable> typeTable):
		typeTable(std::move(typeTable))
	{
		
	}
	
	void FunctionTable::add(TypeEx const& type, std::string const& name) {
		Function f{ .type = type, .name = name };
		auto& overloadSet = functions[name];
		for (auto const& function: overloadSet) {
			if (function.type == type) {
				throw;
			}
		}
		overloadSet.push_back(std::move(f));
	}
	
	utl::small_vector<Function> FunctionTable::getFunction(std::string const& name) const {
		return functions[name];
	}
	
}
