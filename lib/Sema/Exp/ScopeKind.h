#ifndef SCATHA_SEMA_SCOPEKIND_H_
#define SCATHA_SEMA_SCOPEKIND_H_

namespace scatha::sema::exp {
	
	enum class ScopeKind {
		Global,
		Variable,
		Function,
		Object
	};
	
}

#endif // SCATHA_SEMA_SCOPEKIND_H_

