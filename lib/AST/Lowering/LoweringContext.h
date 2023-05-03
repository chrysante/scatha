#ifndef SCATHA_AST_LOWERINGCONTEXT_H_
#define SCATHA_AST_LOWERINGCONTEXT_H_

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/List.h"
#include "IR/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::ast {

struct Loop {
    ir::BasicBlock* header = nullptr;
    ir::BasicBlock* body   = nullptr;
    ir::BasicBlock* inc    = nullptr;
    ir::BasicBlock* end    = nullptr;
};

struct LoweringContext {
    sema::SymbolTable const& symbolTable;
    ir::Context& ctx;
    ir::Module& mod;

    /// ## Maps
    utl::hashmap<sema::Type const*, ir::Type const*> typeMap;
    utl::hashmap<sema::Entity const*, ir::Value*> variableAddressMap;
    utl::hashmap<sema::Function const*, ir::Callable*> functionMap;

    /// ## Current state

    ir::Function* currentFunction = nullptr;
    ir::BasicBlock* currentBlock  = nullptr;
    utl::small_vector<ir::Alloca*> allocas;
    Loop currentLoop;

    /// # -

    LoweringContext(sema::SymbolTable const& symbolTable,
                    ir::Context& ctx,
                    ir::Module& mod):
        symbolTable(symbolTable), ctx(ctx), mod(mod) {}

    /// # Forward declarations

    void declareTypes();

    void declareFunction(sema::Function const* function);

    /// # Statements

    void generate(AbstractSyntaxTree const& node);

    void generateImpl(AbstractSyntaxTree const& node) { SC_UNREACHABLE(); }

    void generateImpl(TranslationUnit const&);

    void generateImpl(CompoundStatement const&);

    void generateImpl(FunctionDefinition const&);

    void generateParameter(List<ir::Parameter*>::iterator& paramItr,
                           ParameterDeclaration const* paramDecl);

    void generateImpl(StructDefinition const&);

    void generateImpl(VariableDeclaration const&);

    void generateImpl(ParameterDeclaration const&);

    void generateImpl(ExpressionStatement const&);

    void generateImpl(EmptyStatement const&) {}

    void generateImpl(ReturnStatement const&);

    void generateImpl(IfStatement const&);

    void generateImpl(LoopStatement const&);

    void generateImpl(JumpStatement const&);

    /// # Expressions

    /// **Always** dereferences implicit references
    /// Only explicit references are returned as pointers
    ir::Value* getValue(Expression const* expr) { SC_DEBUGFAIL(); }

    /// Return the logical address, i.e. if expression is an implicit reference,
    /// it returns the address it refers to, if it is an l-value object then it
    /// returns the address of that
    ir::Value* getAddress(Expression const* expr) { SC_DEBUGFAIL(); }

    /// Returns the address of a reference to reassign it
    ir::Value* getReferenceAddress(Expression const& node);

    /// # Utils

    ir::BasicBlock* newBlock(std::string name);

    void add(ir::BasicBlock* basicBlock);

    ir::BasicBlock* addNewBlock(std::string name);

    void add(ir::Instruction* inst);

    template <std::derived_from<ir::Instruction> Inst, typename... Args>
        requires std::constructible_from<Inst, Args...>
    void add(Args&&... args) {
        add(new Inst(std::forward<Args>(args)...));
    }

    template <std::derived_from<ir::Instruction> Inst, typename... Args>
        requires std::constructible_from<Inst, ir::Context&, Args...>
    void add(Args&&... args) {
        add(new Inst(ctx, std::forward<Args>(args)...));
    }

    ir::Value* storeLocal(ir::Value* value, std::string name = {});

    ir::Value* makeLocal(ir::Type const* type, std::string name);

    void memorizeVariableAddress(sema::Entity const* entity,
                                 ir::Value* address);

    /// # Map utils

    ir::Type const* mapType(sema::Type const* semaType);

    ir::UnaryArithmeticOperation mapUnaryOp(ast::UnaryPrefixOperator op);

    ir::CompareOperation mapCompareOp(ast::BinaryOperator op);

    ir::ArithmeticOperation mapArithmeticOp(sema::StructureType const* type,
                                            ast::BinaryOperator op);

    ir::ArithmeticOperation mapArithmeticAssignOp(
        sema::StructureType const* type, ast::BinaryOperator op);

    ir::CompareMode mapCompareMode(sema::StructureType const* type);

    ir::FunctionAttribute mapFuncAttrs(sema::FunctionAttribute attr);

    ir::Visibility accessSpecToVisibility(sema::AccessSpecifier spec);
};

} // namespace scatha::ast

#endif // SCATHA_AST_LOWERINGCONTEXT_H_
