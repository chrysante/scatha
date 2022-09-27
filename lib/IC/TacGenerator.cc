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
				submitFunctionEndLabel();
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
				
				Operation const cjmpOp = processIfCondition(ifStatement->condition.get());
				size_t const cjmpIndex = submitJump(cjmpOp, Label{});
				
				doRun(ifStatement->ifBlock.get());
				if (ifStatement->elseBlock != nullptr) {
					size_t const jmpIndex = submitJump(Operation::jmp, Label{});
					code[cjmpIndex].asTas().arg1 = submitLabel();
					doRun(ifStatement->elseBlock.get());
					code[jmpIndex].asTas().arg1 = submitLabel();
				}
				else {
					code[cjmpIndex].asTas().arg1 = submitLabel();
				}
				return;
			}
			case ast::NodeType::WhileStatement: {
				auto const* const whileStatement = static_cast<ast::WhileStatement const*>(node);
				
				Label const loopBeginLabel = submitLabel();
				Operation const cjmpOp = processIfCondition(whileStatement->condition.get());
				size_t const cjmpIndex = submitJump(cjmpOp, Label{});
				doRun(whileStatement->block.get());
				submitJump(Operation::jmp, loopBeginLabel);
				code[cjmpIndex].asTas().arg1 = submitLabel();
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
			case ast::NodeType::BooleanLiteral: {
				auto const* lit = static_cast<ast::BooleanLiteral const*>(node);
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
					case ast::BinaryOperator::LeftShift:
					case ast::BinaryOperator::RightShift:
					case ast::BinaryOperator::BitwiseOr:
					case ast::BinaryOperator::BitwiseXOr:
					case ast::BinaryOperator::BitwiseAnd:
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
					submit(Operation::param, doRun(arg.get()));
				}
				{	// our little hack to call functions for now
					auto const* const functionId = dynamic_cast<ast::Identifier const*>(expr->object.get());
					SC_ASSERT(functionId != nullptr, "Called object must be an identifier");
					submitJump(Operation::call, Label(functionId->symbolID));
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
		SC_ASSERT(result.is(TasArgument::variable) ||
				  result.is(TasArgument::temporary) ||
				  result.is(TasArgument::conditional),
				  "This overload must assign to a variable");
		code.push_back(ThreeAddressStatement{
			.operation = op,
			.result    = result,
			.arg1      = a,
			.arg2      = b
		});
		return result;
	}
	
	size_t TacGenerator::submitJump(Operation jmp, Label label) {
		SC_ASSERT(isJump(jmp), "Operation must be a jump");
		code.push_back(ThreeAddressStatement{
			.operation = jmp,
			.arg1      = label
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
	
	void TacGenerator::submitFunctionEndLabel() {
		code.push_back(FunctionEndLabel{});
	}
	
	Operation TacGenerator::processIfCondition(ast::Expression const* condition) {
		TasArgument const condResult = doRun(condition);
		auto& condStatement = [&]() -> auto&{
			if (code.back().isTas()) {
				// The condition generated a TAS
				return code.back().asTas();
			}
			if (condResult.is(TasArgument::literalValue)) {
				// The condition is a literal
				submit(makeTemporary(sym.Bool()), Operation::mov, condResult);
				return code.back().asTas();
			}
			// What is the condition?
			SC_DEBUGFAIL();
		}();
		
		// Make the condition a tas conditional statement
		if (isRelop(condStatement.operation)) {
			condStatement.result = If{};
			condStatement.operation = reverseRelop(condStatement.operation);
		}
		else {
			auto const& condition = condStatement.result;
			submit(If{}, Operation::ifPlaceholder, condition);
		}
		
		return Operation::jmp;
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
			result(sym.Int(), Less)      = Operation::ils;
			result(sym.Int(), LessEq)    = Operation::ileq;
			
			result(sym.Int(), LeftShift)  = Operation::sl;
			result(sym.Int(), RightShift) = Operation::sr;
			
			result(sym.Int(), BitwiseAnd) = Operation::And;
			result(sym.Int(), BitwiseOr)  = Operation::Or;
			result(sym.Int(), BitwiseXOr) = Operation::XOr;
			
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
