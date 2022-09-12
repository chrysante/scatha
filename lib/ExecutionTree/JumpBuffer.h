#pragma once

#ifndef SCATHA_EXECUTIONTREE_JUMPBUFFER_H_
#define SCATHA_EXECUTIONTREE_JUMPBUFFER_H_

#include <setjmp.h>

namespace scatha::execution {

	
	struct JumpBuffer {
		bool set() {
			return !setjmp(buffer);
		}
		[[noreturn]] void jump() { longjmp(buffer, 1); }
	
	private:
		jmp_buf buffer;
	};

}

#endif // SCATHA_EXECUTIONTREE_JUMPBUFFER_H_


