#include "IC/TacGenerator.h"

#include <array>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>

namespace scatha::ic {

	TacGenerator::TacGenerator(sema::SymbolTable const& sym): sym(sym) {
		
	}
	
	ThreeAddressCode TacGenerator::run(ast::AbstractSyntaxTree const* root) {
		SC_ASSERT(tmpIndex == 0, "Don't reuse this");
		
		auto const* const tu = dynamic_cast<ast::TranslationUnit const*>(root);
		
		for (auto& decl: tu->declarations) {
			doRun(decl.get());
		}
		
		return ThreeAddressCode{ std::move(code) };
	}

	void TacGenerator::doRun(ast::Statement const* node) {
		switch (node->nodeType()) {
			case ast::NodeType::FunctionDefinition: {
				auto const* const fnDef = static_cast<ast::FunctionDefinition const*>(node);
				currentFunctionID = fnDef->symbolID;
				tmpIndex = 0;
				labelIndex = 0;
				submitFunctionLabel(*fnDef);
				doRun(fnDef->body.get());
				return;
			}
			case ast::NodeType::Block: {
				auto const* const block = static_cast<ast::Block const*>(node);
				for (auto& statement: block->statements) {
					doRun(statement.get());
				}
				return;
			}
			case ast::NodeType::VariableDeclaration: {
				auto const* const varDecl = static_cast<ast::VariableDeclaration const*>(node);
				SC_ASSERT(varDecl->initExpression != nullptr, "how do we handle this?");
				TasArgument const initResult = doRun(varDecl->initExpression.get());
				
				Variable const var{ varDecl->symbolID };
				
				if (initResult.is(TasArgument::temporary)) {
					// use assignment to the last temporary to directly assign to the variable
					code.back().asTas().result = var;
					--tmpIndex;
				}
				else {
					submit(var, Operation::mov, initResult);
				}
				return;
			}
			case ast::NodeType::ExpressionStatement: {
				auto const* const statement = static_cast<ast::ExpressionStatement const*>(node);
				doRun(statement->expression.get());
				return;
			}
			case ast::NodeType::IfStatement: {
				auto const* const ifStatement = static_cast<ast::IfStatement const*>(node);
				auto const cond = doRun(ifStatement->condition.get());
				size_t const cjmpIndex = submitJump(Operation::cjmp, cond);
				
				doRun(ifStatement->ifBlock.get());
				if (ifStatement->elseBlock != nullptr) {
					size_t const jmpIndex = submitJump(Operation::jmp);
					code[cjmpIndex].asTas().arg2 = submitLabel();
					doRun(ifStatement->elseBlock.get());
					code[jmpIndex].asTas().arg2 = submitLabel();
				}
				else {
					code[cjmpIndex].asTas().arg2 = submitLabel();
				}
				return;
			}
			case ast::NodeType::ReturnStatement: {
				auto const* const ret = static_cast<ast::ReturnStatement const*>(node);
				TasArgument const retValue = doRun(ret->expression.get());
				submit(Operation::ret, retValue);
				return;
			}
				
			SC_NO_DEFAULT_CASE();
		}
	}

	TasArgument TacGenerator::doRun(ast::Expression const* node) {
		switch (node->nodeType()) {
			case ast::NodeType::Identifier: {
				auto const* id = static_cast<ast::Identifier const*>(node);
				return Variable{ id->symbolID };
			}
			case ast::NodeType::IntegerLiteral: {
				auto const* lit = static_cast<ast::IntegerLiteral const*>(node);
				return LiteralValue{ *lit };
			}
			case ast::NodeType::FloatingPointLiteral: {
				auto const* lit = static_cast<ast::FloatingPointLiteral const*>(node);
				return LiteralValue{ *lit };
			}
			case ast::NodeType::BinaryExpression: {
				auto const* expr = static_cast<ast::BinaryExpression const*>(node);
				TasArgument const lhs = doRun(expr->lhs.get());
				TasArgument const rhs = doRun(expr->rhs.get());
				switch (expr->op) {
					case ast::BinaryOperator::Addition:       [[fallthrough]];
					case ast::BinaryOperator::Subtraction:    [[fallthrough]];
					case ast::BinaryOperator::Multiplication: [[fallthrough]];
					case ast::BinaryOperator::Division:       [[fallthrough]];
					case ast::BinaryOperator::Remainder:
						return submit(makeTemporary(expr->lhs->typeID),
									  selectOperation(expr->lhs->typeID, expr->op),
									  lhs, rhs);
					case ast::BinaryOperator::Less:           [[fallthrough]];
					case ast::BinaryOperator::LessEq:         [[fallthrough]];
					case ast::BinaryOperator::Equals:         [[fallthrough]];
					case ast::BinaryOperator::NotEquals:
						return submit(makeTemporary(sym.Bool()),
									  selectOperation(expr->lhs->typeID, expr->op),
									  lhs, rhs);

					case ast::BinaryOperator::Assignment: {
						auto const* lhsId = dynamic_cast<ast::Identifier const*>(expr->lhs.get());
						SC_ASSERT(lhsId != nullptr, "We don't support assigning to arbitrary expressions yet");
						auto const var = Variable{ lhsId->symbolID };
						if (lhs.is(TasArgument::temporary)) {
							code.back().asTas().result = var;
							--tmpIndex;
						}
						else {
							submit(var, Operation::mov, rhs);
						}
						return var;
					}
					SC_NO_DEFAULT_CASE();
				}
			}
			case ast::NodeType::UnaryPrefixExpression: {
				auto const* expr = static_cast<ast::UnaryPrefixExpression const*>(node);
				TasArgument const arg = doRun(expr->operand.get());
				sema::TypeID const type = expr->operand->typeID;
				switch (expr->op) {
					case ast::UnaryPrefixOperator::Promotion:
						return arg;

					case ast::UnaryPrefixOperator::Negation:
						return submit(makeTemporary(type), selectOperation(type, ast::BinaryOperator::Subtraction),
									  LiteralValue(0, type), arg);

					case ast::UnaryPrefixOperator::BitwiseNot:
						SC_ASSERT(type == sym.Int(), "Only int supported for now");
						return submit(makeTemporary(type), Operation::bnt, arg);

					case ast::UnaryPrefixOperator::LogicalNot:
						SC_ASSERT(type == sym.Bool(), "Only bool supported");
						return submit(makeTemporary(type), Operation::lnt, arg);

					SC_NO_DEFAULT_CASE();
				}
			}
			case ast::NodeType::FunctionCall: {
				auto const* expr = static_cast<ast::FunctionCall const*>(node);
				for (auto& arg: expr->arguments) {
					submit(Operation::pushParam, doRun(arg.get()));
				}
				{	// our little hack to call functions for now
					auto const* const functionId = dynamic_cast<ast::Identifier const*>(expr->object.get());
					SC_ASSERT(functionId != nullptr, "Called object must be an identifier");
					submitJump(Operation::call, {}, Label(functionId->symbolID, 0));
					return submit(makeTemporary(expr->typeID), Operation::getResult);
				}
			}
				
				
			SC_NO_DEFAULT_CASE();
		}
	}
	
	void TacGenerator::submit(Operation op, TasArgument a, TasArgument b) {
		code.push_back(ThreeAddressStatement{
			.operation = op,
			.arg1      = a,
			.arg2      = b
		});
	}
	
	TasArgument TacGenerator::submit(TasArgument result, Operation op, TasArgument a, TasArgument b) {
		SC_ASSERT(result.is(TasArgument::variable) || result.is(TasArgument::temporary),
				  "This overload must assign to a variable");
		code.push_back(ThreeAddressStatement{
			.operation = op,
			.result    = result,
			.arg1      = a,
			.arg2      = b
		});
		return result;
	}
	
	size_t TacGenerator::submitJump(Operation jmp, TasArgument cond, Label label) {
		SC_ASSERT(isJump(jmp), "Operation must be a jump");
		code.push_back(ThreeAddressStatement{
			.operation = jmp,
			.arg1      = cond,
			.arg2      = label
		});
		return code.size() - 1;
	}
	
	Label TacGenerator::submitLabel() {
		auto const result = Label(currentFunctionID,
								  labelIndex++);
		code.push_back(result);
		return result;
	}
	
	FunctionLabel TacGenerator::submitFunctionLabel(ast::FunctionDefinition const& fnDef) {
		FunctionLabel const result(fnDef);
		code.push_back(result);
		return result;
	}
	
	TasArgument TacGenerator::makeTemporary(sema::TypeID type) {
		return Temporary{ tmpIndex++, type };
	}
	
	Operation TacGenerator::selectOperation(sema::TypeID typeID, ast::BinaryOperator op) const {
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
			result(sym.Int(), Division)       = Operation::idiv;
			result(sym.Int(), Remainder)      = Operation::irem;
			
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
		SC_ASSERT(static_cast<size_t>(result) != 0, "");
		return result;
	}
	
	
}
