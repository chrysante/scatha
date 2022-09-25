#include "RegisterDescriptor.h"

namespace scatha::codegen {
	
	void RegisterDescriptor::declareParameters(ic::FunctionLabel const& fn) {
		SC_ASSERT(variables.empty(), "Should have been cleared");
		SC_ASSERT(index == 0, "");
		for (auto const& [id, type]: fn.parameters()) {
			auto const [itr, success] = variables.insert({ id, index });
			SC_ASSERT(success, "");
			index += 1;
		}
	}
	
	assembly::RegisterIndex RegisterDescriptor::resolve(ic::Variable const& var) {
		if (auto const itr = variables.find(var.id());
			itr != variables.end())
		{
			return assembly::RegisterIndex(itr->second);
		}
		auto const [itr, success] = variables.insert({ var.id(), index });
		index += 1;
		return assembly::RegisterIndex(itr->second);
	}
	
	assembly::RegisterIndex RegisterDescriptor::resolve(ic::Temporary const& tmp) {
		if (auto const itr = temporaries.find(tmp.index);
			itr != temporaries.end())
		{
			return assembly::RegisterIndex(itr->second);
		}
		auto const [itr, success] = temporaries.insert({ tmp.index, index });
		index += 1;
		return assembly::RegisterIndex(itr->second);
	}
	
	std::optional<assembly::RegisterIndex> RegisterDescriptor::resolve(ic::TasArgument const& arg) {
		using R = std::optional<assembly::RegisterIndex>;
		return arg.visit(utl::visitor{
			[&](ic::Variable const& var) -> R { return resolve(var); },
			[&](ic::Temporary const& tmp) -> R { return resolve(tmp); },
			[&](auto const&) -> R { return std::nullopt; }
		});
	}
	
	void RegisterDescriptor::clear() {
		index = 0;
		variables.clear();
		temporaries.clear();
	}
	
}
