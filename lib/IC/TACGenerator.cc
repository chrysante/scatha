#include "TACGenerator.h"

#include <array>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>

namespace scatha::ic {

	TACGenerator::TACGenerator(sema::SymbolTable const& sym): sym(sym) {
		
	}
	
	[[nodiscard]] TAC TACGenerator::run(ast::FunctionDefinition const* fn) {
		SC_ASSERT(tmpIndex == 0, "Don't reuse this");
		doRun(fn->body.get());
		return TAC{ std::move(code) };
	}

	TAS::Element TACGenerator::doRun(ast::AbstractSyntaxTree const* node) {
		switch (node->nodeType()) {
			case ast::NodeType::Block: {
				auto const* const block = static_cast<ast::Block const*>(node);
				for (auto& statement: block->statements) {
					doRun(statement.get());
				}
				return {};
			}
			case ast::NodeType::VariableDeclaration: {
				auto const* const varDecl = static_cast<ast::VariableDeclaration const*>(node);
				SC_ASSERT(varDecl->initExpression != nullptr, "how do we handle this?");
				auto const initResult = doRun(varDecl->initExpression.get());
				if (initResult.isTemporary) {
					code.back().resultIsTemp = false;
					code.back().result = varDecl->symbolID.id();
					--tmpIndex;
				}
				else {
					submit(TAS::makeVariable(varDecl->symbolID.id()), Operation::mov, initResult);
				}
				return {};
			}
			case ast::NodeType::ExpressionStatement: {
				auto const* const statement = static_cast<ast::ExpressionStatement const*>(node);
				doRun(statement->expression.get());
				return {};
			}
			case ast::NodeType::IfStatement: {
				auto const* const ifStatement = static_cast<ast::IfStatement const*>(node);
				auto const cond = doRun(ifStatement->condition.get());
				size_t const cjmpIndex = submitCJump(cond);
				doRun(ifStatement->ifBlock.get());
				
				if (ifStatement->elseBlock != nullptr) {
					size_t const jmpIndex = submitJump();
					code[cjmpIndex].b = submitLabel();
					doRun(ifStatement->elseBlock.get());
					code[jmpIndex].a = submitLabel();
				}
				else {
					code[cjmpIndex].b = submitLabel();
				}
				
				return {};
			}
			case ast::NodeType::ReturnStatement: {
				auto const* const ret = static_cast<ast::ReturnStatement const*>(node);
				doRun(ret->expression.get());
				return {};
			}
			case ast::NodeType::BinaryExpression: {
				auto const* expr = static_cast<ast::BinaryExpression const*>(node);
				auto const lhs = doRun(expr->lhs.get());
				auto const rhs = doRun(expr->rhs.get());
				switch (expr->op) {
					case ast::BinaryOperator::Addition:       [[fallthrough]];
					case ast::BinaryOperator::Subtraction:    [[fallthrough]];
					case ast::BinaryOperator::Multiplication: [[fallthrough]];
					case ast::BinaryOperator::Division:       [[fallthrough]];
					case ast::BinaryOperator::Remainder:      [[fallthrough]];
					case ast::BinaryOperator::Less:           [[fallthrough]];
					case ast::BinaryOperator::LessEq:         [[fallthrough]];
					case ast::BinaryOperator::Equals:         [[fallthrough]];
					case ast::BinaryOperator::NotEquals:
						return submit(selectOperation(expr->lhs->typeID, expr->op), lhs, rhs);
					
					case ast::BinaryOperator::Assignment: {
						auto const* lhsId = dynamic_cast<ast::Identifier const*>(expr->lhs.get());
						SC_ASSERT(lhsId != nullptr, "We don't support assigning to arbitrary expressions yet");
						if (rhs.isTemporary) {
							code.back().resultIsTemp = false;
							code.back().result = lhsId->symbolID.id();
							--tmpIndex;
						}
						else {
							submit(TAS::makeVariable(lhsId->symbolID.id()), Operation::mov, rhs);
						}
						
						return TAS::makeVariable(lhsId->symbolID.id());
					}
					SC_NO_DEFAULT_CASE();
				}
			}
			case ast::NodeType::UnaryPrefixExpression: {
				auto const* expr = static_cast<ast::UnaryPrefixExpression const*>(node);
				auto const result = doRun(expr->operand.get());
				switch (expr->op) {
					case ast::UnaryPrefixOperator::Promotion:
						return result;
						
					case ast::UnaryPrefixOperator::Negation:
						return submit(selectOperation(expr->operand->typeID, ast::BinaryOperator::Subtraction), TAS::makeValue(0), result);
						
					case ast::UnaryPrefixOperator::LogicalNot:
						SC_ASSERT(expr->operand->typeID == sym.Int(), "Only int supported for now");
						return submit(Operation::lnt, result);
						
					case ast::UnaryPrefixOperator::BitwiseNot:
						SC_ASSERT(expr->operand->typeID == sym.Bool(), "Only bool supported");
						return submit(Operation::bnt, result);
						
					SC_NO_DEFAULT_CASE();
				}
			}
			case ast::NodeType::Identifier: {
				auto const* id = static_cast<ast::Identifier const*>(node);
				return TAS::makeVariable(id->symbolID.id());
			}
			case ast::NodeType::IntegerLiteral: {
				auto const* lit = static_cast<ast::IntegerLiteral const*>(node);
				return TAS::makeValue(lit->value);
			}
			case ast::NodeType::FloatingPointLiteral: {
				auto const* lit = static_cast<ast::FloatingPointLiteral const*>(node);
				return TAS::makeValue(utl::bit_cast<u64>(lit->value));
			}
			SC_NO_DEFAULT_CASE();
		}
	}
	
	TAS::Element TACGenerator::submit(TAS::Element result, Operation op, TAS::Element a, TAS::Element b) {
		SC_ASSERT(!result.isTemporary, "");
		code.push_back(TAS{
			.isLabel = false,
			.resultIsTemp = false,
			.aIsVar  = a.isVariable,
			.aIsTemp = a.isTemporary,
			.bIsVar  = b.isVariable,
			.bIsTemp = b.isTemporary,
			.op = op,
			.result = result.value,
			.a = a.value,
			.b = b.value
		});
		return result;
	}
	
	TAS::Element TACGenerator::submit(Operation op, TAS::Element a, TAS::Element b) {
		size_t const result = tmpIndex++;
		code.push_back(TAS{
			.isLabel = false,
			.resultIsTemp = true,
			.aIsVar = a.isVariable,
			.aIsTemp = a.isTemporary,
			.bIsVar = b.isVariable,
			.bIsTemp = b.isTemporary,
			.op = op,
			.result = result,
			.a = a.value,
			.b = b.value
		});
		return TAS::makeTemporary(result);
	}
	
	
	
	size_t TACGenerator::submitJump() {
		code.push_back(TAS{
			.isLabel = false,
			.resultIsTemp = false,
			.aIsVar = false,
			.aIsTemp = false,
			.bIsVar = false,
			.bIsTemp = false,
			.op = Operation::jmp,
			.result = (u64)-1,
			.a = (u64)-1,
			.b = (u64)-1
		});
		return code.size() - 1;
	}
	
	size_t TACGenerator::submitCJump(TAS::Element cond) {
		code.push_back(TAS{
			.isLabel = false,
			.resultIsTemp = false,
			.aIsVar = cond.isVariable,
			.aIsTemp = cond.isTemporary,
			.bIsVar = false,
			.bIsTemp = false,
			.op = Operation::cjmp,
			.result = (u64)-1,
			.a = cond.value,
			.b = (u64)-1
		});
		return code.size() - 1;
	}
	
	size_t TACGenerator::submitLabel() {
		u64 const result = labelIndex++;
		code.push_back(TAS{
			.isLabel = true,
			.label = static_cast<u16>(result)
		});
		return result;
	}
	
	Operation TACGenerator::selectOperation(sema::TypeID typeID, ast::BinaryOperator op) const {
		struct OpTable {
			Operation& operator()(sema::TypeID typeID, ast::BinaryOperator op) {
				return table[typeID][(size_t)op];
			}
			
			utl::hashmap<sema::TypeID, std::array<Operation, (size_t)ast::BinaryOperator::_count>> table;
		};
		
		static OpTable table = [this]{
			OpTable result;
			using enum ast::BinaryOperator;
			result(sym.Int(), Addition)       = Operation::add;
			result(sym.Int(), Subtraction)    = Operation::sub;
			result(sym.Int(), Multiplication) = Operation::mul;
			result(sym.Int(), Division)       = Operation::div;
			result(sym.Int(), Remainder)      = Operation::rem;
			
			result(sym.Int(), Equals)    = Operation::eq;
			result(sym.Int(), NotEquals) = Operation::neq;
			result(sym.Int(), Less)      = Operation::ls;
			result(sym.Int(), LessEq)    = Operation::leq;
			
			result(sym.Float(), Addition)       = Operation::fadd;
			result(sym.Float(), Subtraction)    = Operation::fsub;
			result(sym.Float(), Multiplication) = Operation::fmul;
			result(sym.Float(), Division)       = Operation::fdiv;
			
			result(sym.Float(), Equals)    = Operation::feq;
			result(sym.Float(), NotEquals) = Operation::fneq;
			result(sym.Float(), Less)      = Operation::fls;
			result(sym.Float(), LessEq)    = Operation::fleq;
			
			return result;
		}();
		
		auto result = table(typeID, op);
		SC_ASSERT((size_t)result != 0, "");
		return result;
	}
	
}
