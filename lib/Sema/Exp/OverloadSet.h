#ifndef SCATHA_SEMA_OVERLOADSET_H_
#define SCATHA_SEMA_OVERLOADSET_H_

#include <span>

#include <utl/hashset.hpp>

#include "Sema/Exp/EntityBase.h"
#include "Sema/Exp/Function.h"
#include "Sema/Exp/SymbolID.h"

namespace scatha::sema::exp {
	
	class SCATHA(API) OverloadSet: public EntityBase {
	public:
		explicit OverloadSet(std::string name, SymbolID id):
			EntityBase(std::move(name), id)
		{}
		
		///
		Function const* find(std::span<TypeID const> argumentTypes) const;
		
		std::pair<Function*, bool> add(Function);
		
	private:
		using SetType = utl::node_hashset<
			Function,
			Function::ArgumentHash,
			Function::ArgumentEqual
		>;
		SetType functions;
	};
	
}

#endif // SCATHA_SEMA_OVERLOADSET_H_
