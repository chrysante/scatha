#include "IC/TacGenerator.h"

#include <array>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>

namespace scatha::ic {

	TacGenerator::TacGenerator(sema::SymbolTable const& sym): sym(sym) {
		
	}
	
	ThreeAddressCode TacGenerator::run(ast::AbstractSyntaxTree const* root) {
		SC_ASSERT(tmpIndex == 0, "Don't reuse this");
		SC_ASSERT(root->nodeType() == ast::NodeType::TranslationUnit,
				  "TacGenerator must be run on a translation unit");
		auto const& tu = static_cast<ast::TranslationUnit const&>(*root);
		
		for (auto& decl: tu.declarations) {
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
						SC_ASSERT(expr->lhs->nodeType() == ast::NodeType::Identifier,
								  "We don't support assigning to arbitrary expressions yet");
						auto const& lhsId = static_cast<ast::Identifier const&>(*expr->lhs);
						auto const var = Variable{ lhsId.symbolID };
						if (lhs.is(TasArgument::temporary)) {
							code.back().asTas().result = var;
							--tmpIndex;
						}
						else {
							submit(var, Operation::mov, rhs);
						}
						return var;
					}
					case ast::BinaryOperator::Comma: {
						doRun(expr->lhs.get());
						return doRun(expr->rhs.get());
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
				submitJump(Operation::call, Label(expr->functionID));
				return submit(makeTemporary(expr->typeID), Operation::getResult);
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
			Operation& set(sema::TypeID typeID, ast::BinaryOperator op) {
				auto const [itr, success] = table.insert({
					typeID, std::array<Operation, (size_t)ast::BinaryOperator::_count>{}
				});
				if (success) {
					std::fill(itr->second.begin(), itr->second.end(), Operation::_count);
				}
				auto& result = itr->second[(size_t)op];
				SC_ASSERT(result == Operation::_count, "");
				return result;
			}
			
			Operation get(sema::TypeID typeID, ast::BinaryOperator op) const {
				auto const itr = table.find(typeID);
				if (itr == table.end()) {
					SC_DEBUGFAIL();
				}
				auto const result = itr->second[(size_t)op];
				SC_ASSERT(result != Operation::_count, "");
				return result;
			}
			
			utl::hashmap<sema::TypeID, std::array<Operation, (size_t)ast::BinaryOperator::_count>> table;
		};
		
		static OpTable table = [this]{
			OpTable result;
			using enum ast::BinaryOperator;
			result.set(sym.Int(), Addition)       = Operation::add;
			result.set(sym.Int(), Subtraction)    = Operation::sub;
			result.set(sym.Int(), Multiplication) = Operation::mul;
			result.set(sym.Int(), Division)       = Operation::idiv;
			result.set(sym.Int(), Remainder)      = Operation::irem;
			
			result.set(sym.Int(), Equals)    = Operation::eq;
			result.set(sym.Int(), NotEquals) = Operation::neq;
			result.set(sym.Int(), Less)      = Operation::ils;
			result.set(sym.Int(), LessEq)    = Operation::ileq;
			
			result.set(sym.Int(), LeftShift)  = Operation::sl;
			result.set(sym.Int(), RightShift) = Operation::sr;
			
			result.set(sym.Int(), BitwiseAnd) = Operation::And;
			result.set(sym.Int(), BitwiseOr)  = Operation::Or;
			result.set(sym.Int(), BitwiseXOr) = Operation::XOr;
			
			result.set(sym.Bool(), Equals)    = Operation::eq;
			result.set(sym.Bool(), NotEquals) = Operation::neq;
			result.set(sym.Bool(), BitwiseAnd) = Operation::And;
			result.set(sym.Bool(), BitwiseOr)  = Operation::Or;
			result.set(sym.Bool(), BitwiseXOr) = Operation::XOr;
			
			result.set(sym.Float(), Addition)       = Operation::fadd;
			result.set(sym.Float(), Subtraction)    = Operation::fsub;
			result.set(sym.Float(), Multiplication) = Operation::fmul;
			result.set(sym.Float(), Division)       = Operation::fdiv;
			
			result.set(sym.Float(), Equals)    = Operation::feq;
			result.set(sym.Float(), NotEquals) = Operation::fneq;
			result.set(sym.Float(), Less)      = Operation::fls;
			result.set(sym.Float(), LessEq)    = Operation::fleq;
			
			return result;
		}();
		
		return table.get(typeID, op);
	}
	
	
}
