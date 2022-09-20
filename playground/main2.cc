#include <iostream>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "AST/AST.h"


namespace scatha::vm {


	struct RegisterScope {
		
		
		i64* i64Ptr;
		f64* f64Ptr;
	};

	enum class Instruction: u32 {
		allocScope, // (i64 register count, f64 register count)
		movRR, // (index of register to move to, index of register to move)
		movRV, // (index of register to move to, value to move)
		movMR, // (index of register holding memory pointer to move to, index of register to move)
		addRR, // (registerIndex, registerIndex)
		addRM, // (registerIndex, ptr)
		// faddRR,
		// faddRM,
		mulRR, // (registerIndex, registerIndex)
		mulRM, // (registerIndex, ptr)
		callExt, // registerIndex holding call target
		_count
	};
	
	
	
	struct Program {
		utl::vector<u32> instructions;
		
		void add(Instruction i, u32 a) {
			instructions.push_back((u32)i);
			instructions.push_back((u32)a);
		}
		void add(Instruction i, u32 a, u32 b) {
			add(i, a);
			instructions.push_back((u32)b);
		}
	};
	
	struct Machine {
		
		Machine():
			instructionTable((size_t)vm::Instruction::_count)
		{
			using enum vm::Instruction;
			auto at = [&](vm::Instruction i) -> auto& { return instructionTable[(size_t)i]; };
			
			at(allocScope) = [](Machine& m, u32 a, u32 b) {
				m.crs = { (i64*)malloc(8 * a), (f64*)malloc(8 * b) };
				m.iptr += 3;
			};
			
			at(movRR) = [](Machine& m, u32 a, u32 b) {
				m.crs.i64Ptr[a] = m.crs.i64Ptr[b];
				m.iptr += 3;
			};
			at(movRV) = [](Machine& m, u32 a, u32 b) {
				m.crs.i64Ptr[a] = b;
				m.iptr += 3;
			};
			at(movMR) = [](Machine& m, u32 a, u32 b) {
				(i64&)m.memory[m.crs.i64Ptr[a]] = m.crs.i64Ptr[b];
				m.iptr += 3;
			};
			
			at(addRR) = [](Machine& m, u32 a, u32 b) {
				m.crs.i64Ptr[a] += m.crs.i64Ptr[b];
				m.iptr += 3;
			};
			at(addRM) = [](Machine& m, u32 a, u32 b) {
				m.crs.i64Ptr[a] += (i64 const&)m.memory[m.crs.i64Ptr[b]];
				m.iptr += 3;
			};
			
			at(mulRR) = [](Machine& m, u32 a, u32 b) {
				m.crs.i64Ptr[a] *= m.crs.i64Ptr[b];
				m.iptr += 3;
			};
			at(mulRM) = [](Machine& m, u32 a, u32 b) {
				m.crs.i64Ptr[a] *= (i64 const&)m.memory[m.crs.i64Ptr[b]];
				m.iptr += 3;
			};
			
			at(callExt) = [](Machine& m, u32 a, u32) {
				m.extFunctionTable[a](m);
				m.iptr += 2;
			};
			
			extFunctionTable.push_back([](Machine& m) { std::cout << (i64&)m.memory[m.crs.i64Ptr[0]]; });
			
			memory.resize(128);
		}
		
		void execute(Program const& program) {
			size_t const icount = program.instructions.size() - 1;
			while (iptr < icount) {
				u32 const instruction = program.instructions[iptr];
				instructionTable[instruction](*this, program.instructions[iptr + 1], program.instructions[iptr + 2]);
			}
		}
		
		
	private:
		template <typename T>
		static T as(u64 x) { return static_cast<T>(x); }
	
	private:
		u64 iptr = 0;
		RegisterScope crs{};
		using Instruction = void(*)(Machine&, u32, u32);
		utl::vector<Instruction> instructionTable;
		using ExtFunction = void(*)(Machine&);
		utl::vector<ExtFunction> extFunctionTable;
		utl::vector<u8> memory;
	};

	
}

using namespace scatha;
ast::UniquePtr<ast::AbstractSyntaxTree> makeAST();

int _main() {
	
	
	using namespace scatha::vm;
	using enum Instruction;
	Program p;
	
	p.add(allocScope, 6, 0);
	p.add(movRV, 0, 0);
	p.add(movRV, 1, 5);
	p.add(movRV, 2, 4);
	p.add(mulRR, 1, 2);
	p.add(movMR, 0, 1);
	p.add(callExt, 0);
	p.instructions.push_back(0);
	Machine m;
	
	m.execute(p);
	
	std::cout << std::endl;
}
