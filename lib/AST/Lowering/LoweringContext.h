#ifndef SCATHA_AST_LOWERINGCONTEXT_H_
#define SCATHA_AST_LOWERINGCONTEXT_H_

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/APFloat.h"
#include "Common/APInt.h"
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

    /// # Other data
    ir::Type const* arrayViewType = nullptr;

    /// # -

    LoweringContext(sema::SymbolTable const& symbolTable,
                    ir::Context& ctx,
                    ir::Module& mod);

    void run(ast::AbstractSyntaxTree const& root);

    /// # Declarations

    void makeDeclarations();

    void declareType(sema::StructureType const* structType);

    ir::Callable* declareFunction(sema::Function const* function);

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

    ///
    /// Let `X` be the raw `sema::ObjectType` of the expression \p expr
    /// The return value of this function depends on the `sema::QualType` of
    /// \p expr in the following way:
    ///
    /// ` X -> Value of the expression `
    /// Regardless of wether `X` is a struct or an array, the value of the
    /// object will be returned
    ///
    /// `'X -> Value of the referred object`
    /// Same as above
    ///
    /// `&X -> Address of the referred object`
    /// If `X` is a struct type, a value of type `ptr` will be returned
    /// If `X` is an array type, a value of type `{ ptr, i64 }` will be returned
    ///
    ir::Value* getValue(Expression const* expr);

    ///
    /// Let `X` be the raw `sema::ObjectType` of the expression \p expr
    /// The return value of this function depends on the `sema::QualType` of
    /// \p expr in the following way:
    ///
    /// ` X -> -`
    /// Traps if `X` is a struct type. Temporaries don't have addresses. We
    /// might however consider to store the value to memory and return a pointer
    /// to it. If `X` is an array type, a value of type `{ ptr, i64 }` will be
    /// returned
    ///
    /// `'X -> Address of the referred object`
    /// If `X` is a struct type, a value of type `ptr` will be returned.
    /// If `X` is an array type, a value of type `{ ptr, i64 }` will be returned
    ///
    /// `&X -> Address of the reference`
    /// If `X` is a struct type, a value of type `ptr`, pointing to another
    /// `ptr`, will be returned.
    /// If `X` is an array type, a value of type `ptr`, pointing to a value of
    /// type `{ ptr, i64 }`, will be returned
    ///
    ir::Value* getAddress(Expression const* expr);

    ir::Value* getValueImpl(AbstractSyntaxTree const& expr) {
        SC_UNREACHABLE();
    } // Delete later
    ir::Value* getValueImpl(Expression const& expr) { SC_UNREACHABLE(); }
    ir::Value* getValueImpl(Identifier const&);
    ir::Value* getValueImpl(Literal const&);
    ir::Value* getValueImpl(UnaryPrefixExpression const&);
    ir::Value* getValueImpl(BinaryExpression const&);
    ir::Value* getValueImpl(MemberAccess const&);
    ir::Value* getValueImpl(ReferenceExpression const&);
    ir::Value* getValueImpl(UniqueExpression const&);
    ir::Value* getValueImpl(Conditional const&);
    ir::Value* getValueImpl(FunctionCall const&);
    ir::Value* getValueImpl(Subscript const&);
    ir::Value* getValueImpl(Conversion const&);
    ir::Value* getValueImpl(ListExpression const&);

    ir::Value* getAddressImpl(AbstractSyntaxTree const& expr) {
        SC_UNREACHABLE();
    } // Delete this later
    ir::Value* getAddressImpl(Literal const& lit);
    ir::Value* getAddressImpl(Identifier const&);
    ir::Value* getAddressImpl(MemberAccess const&);
    ir::Value* getAddressImpl(FunctionCall const&);
    ir::Value* getAddressImpl(Subscript const&);
    ir::Value* getAddressImpl(ReferenceExpression const&);
    ir::Value* getAddressImpl(Conversion const&);
    ir::Value* getAddressImpl(ListExpression const&);

    ir::Value* getAddressLocation(Expression const* expr);

    ir::Value* getAddressLocImpl(AbstractSyntaxTree const& expr) {
        SC_UNREACHABLE();
    } // Delete this later
    ir::Value* getAddressLocImpl(Identifier const& expr);
    ir::Value* getAddressLocImpl(MemberAccess const& expr);

    /// # Utils

    ir::BasicBlock* newBlock(std::string name);

    void add(ir::BasicBlock* basicBlock);

    ir::BasicBlock* addNewBlock(std::string name);

    void add(ir::Instruction* inst);

    template <std::derived_from<ir::Instruction> Inst, typename... Args>
        requires std::constructible_from<Inst, Args...>
    Inst* add(Args&&... args) {
        Inst* result = new Inst(std::forward<Args>(args)...);
        add(result);
        return result;
    }

    template <std::derived_from<ir::Instruction> Inst, typename... Args>
        requires std::constructible_from<Inst, ir::Context&, Args...>
    Inst* add(Args&&... args) {
        auto* result = new Inst(ctx, std::forward<Args>(args)...);
        add(result);
        return result;
    }

    ir::Value* storeLocal(ir::Value* value, std::string name = {});

    ir::Value* makeLocal(ir::Type const* type, std::string name);

    ir::Value* makeArrayRef(ir::Value* addr, ir::Value* count);

    ir::Value* getArrayAddr(ir::Value* arrayRef);

    ir::Value* getArrayCount(ir::Value* arrayRef);

    ir::Callable* getFunction(sema::Function const*);

    ir::Value* genCall(FunctionCall const*);

    utl::small_vector<ir::Value*> mapArguments(auto&& args);

    ir::Value* intConstant(APInt value);

    ir::Value* intConstant(size_t value, size_t bitwidth);

    ir::Value* floatConstant(APFloat value);

    ir::Value* constant(ssize_t value, ir::Type const* type);

    bool hasAddress(ast::Expression const* expr) const;

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
