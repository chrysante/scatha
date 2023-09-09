#ifndef SCATHA_IRGEN_LOWERINGCONTEXT_H_
#define SCATHA_IRGEN_LOWERINGCONTEXT_H_

#include <utl/function_view.hpp>
#include <utl/hashmap.hpp>
#include <utl/ipp.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "Common/List.h"
#include "IR/Fwd.h"
#include "IRGen/CallingConvention.h"
#include "IRGen/Maps.h"
#include "IRGen/Value.h"
#include "Sema/AnalysisResult.h"
#include "Sema/DTorStack.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

struct Loop {
    ir::BasicBlock* header = nullptr;
    ir::BasicBlock* body = nullptr;
    ir::BasicBlock* inc = nullptr;
    ir::BasicBlock* end = nullptr;
};

struct LoweringContext {
    sema::SymbolTable const& symbolTable;
    sema::AnalysisResult const& analysisResult;
    ir::Context& ctx;
    ir::Module& mod;

    /// ## Maps
    TypeMap typeMap;

    /// Maps variables to IR values in stack memory
    utl::hashmap<sema::Object const*, Value> objectMap;
    /// Maps array IDs to their respective sizes
    utl::hashmap<uint32_t, Value> arraySizeMap;

    /// Maps variables to SSA values
    /// Right now this map exists solely to map the `.count` member variable to
    /// the size of the array
    utl::hashmap<sema::Entity const*, Value> valueMap;
    utl::hashmap<sema::Function const*, ir::Callable*> functionMap;
    utl::hashmap<sema::Function const*, CallingConvention> CCMap;
    /// Maps member indices of `sema::StructureType` to indices of
    /// `ir::StructureType`. These indices are not necessarily the same. Right
    /// now they only differ if the struct contains array references, because
    /// these are only value in Sema but two values in IR `(ptr, i64)`.
    utl::hashmap<std::pair<sema::StructureType const*, size_t>, size_t>
        structIndexMap;

    uint32_t _valueID = 0;
    uint32_t newID() { return ++_valueID; }

    /// ## Current state

    ir::Function* currentFunction = nullptr;
    sema::Function const* currentSemaFunction = nullptr;
    ir::BasicBlock* currentBlock = nullptr;
    utl::small_vector<ir::Alloca*> allocas;
    utl::stack<Loop, 4> loopStack;

    /// # Other data
    ir::Type const* arrayViewType = nullptr;

    /// # -

    LoweringContext(sema::SymbolTable const& symbolTable,
                    sema::AnalysisResult const& analysisResult,
                    ir::Context& ctx,
                    ir::Module& mod);

    void run(ast::ASTNode const& root);

    /// # Declarations

    void makeDeclarations();

    void declareType(sema::StructureType const* structType);

    ir::Callable* declareFunction(sema::Function const* function);

    /// # Statements

    void generate(ast::ASTNode const& node);

    void generateImpl(ast::ASTNode const& node) { SC_UNREACHABLE(); }

    void generateImpl(ast::TranslationUnit const&);

    void generateImpl(ast::CompoundStatement const&);

    void generateImpl(ast::FunctionDefinition const&);

    void generateParameter(ast::ParameterDeclaration const* paramDecl,
                           PassingConvention pc,
                           List<ir::Parameter>::iterator& paramItr);

    void generateImpl(ast::StructDefinition const&);

    void generateImpl(ast::VariableDeclaration const&);

    void generateImpl(ast::ExpressionStatement const&);

    void generateImpl(ast::EmptyStatement const&) {}

    void generateImpl(ast::ReturnStatement const&);

    void generateImpl(ast::IfStatement const&);

    void generateImpl(ast::LoopStatement const&);

    void generateImpl(ast::JumpStatement const&);

    /// # Expressions

    Value getValue(ast::Expression const* expr);

    template <ValueLocation Loc>
    ir::Value* getValue(ast::Expression const* expr) {
        auto value = getValue(expr);
        switch (Loc) {
        case ValueLocation::Register:
            return toRegister(value);
        case ValueLocation::Memory:
            return toMemory(value);
        }
    }

    Value getValueImpl(ast::Expression const& expr) { SC_UNREACHABLE(); }
    Value getValueImpl(ast::Identifier const&);
    Value getValueImpl(ast::Literal const&);
    Value getValueImpl(ast::UnaryExpression const&);
    Value getValueImpl(ast::BinaryExpression const&);
    Value getValueImpl(ast::MemberAccess const&);
    Value getValueImpl(ast::DereferenceExpression const&);
    Value getValueImpl(ast::AddressOfExpression const&);
    Value getValueImpl(ast::Conditional const&);
    Value getValueImpl(ast::FunctionCall const&);
    Value getValueImpl(ast::Subscript const&);
    Value getValueImpl(ast::SubscriptSlice const&);
    Value getValueImpl(ast::ListExpression const&);
    Value getValueImpl(ast::Conversion const&);
    Value getValueImpl(ast::ConstructorCall const&);
    Value getValueImpl(ast::TrivialCopyExpr const&);

    /// # Helpers

    void generateArgument(PassingConvention const& PC,
                          Value arg,
                          utl::vector<ir::Value*>& outArgs);

    bool genStaticListData(ast::ListExpression const& list, ir::Alloca* dest);

    void genListDataFallback(ast::ListExpression const& list, ir::Alloca* dest);

    void emitDestructorCalls(sema::DTorStack const& dtorStack);

    /// Creates array size values and stores them in `objectMap` if declared
    /// type is array
    void generateArraySizeImpl(ast::VarDeclBase const* varDecl,
                               uint32_t varID,
                               utl::function_view<ir::Value*()> sizeCallback);

    void generateVarDeclArraySize(ast::VarDeclBase const* varDecl,
                                  uint32_t varID,
                                  uint32_t initID);

    void generateParamArraySize(ast::VarDeclBase const* varDecl,
                                uint32_t varID,
                                ir::Parameter* param);

    /// # Utils

    /// \Returns pointee type as array type, if \p type is a pointer to array,
    /// `nullptr` otherwise
    static sema::ArrayType const* ptrToArray(sema::ObjectType const* type);

    static sema::QualType stripRefOrPtr(sema::QualType);

    /// Allocate a new basic block with name \p name without adding it to the
    /// current function
    ir::BasicBlock* newBlock(std::string name);

    /// Add the basic block \p BB to the current function
    void add(ir::BasicBlock* BB);

    /// Allocate a new basic block with name \p name and add it to the current
    /// function
    ir::BasicBlock* addNewBlock(std::string name);

    /// Add the instruction \p inst to the current basic block
    void add(ir::Instruction* inst);

    /// If the value \p value is already in a register, returns that.
    /// Otherwise loads the value from memory and returns the `load` instruction
    ir::Value* toRegister(Value value);

    /// If the value \p value is in memory, returns the address.
    /// Otherwise allocates stack memory, stores the value and returns the
    /// address
    ir::Value* toMemory(Value value);

    ///
    ir::Value* toValueLocation(ValueLocation location, Value value);

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

    ir::Callable* getFunction(sema::Function const*);

    /// \Returns the value passing convention of the return value and the return
    /// value if the passing convention is `Register` or the address of the
    /// return value if the passing convention is `Stack`
    Value genCall(ast::FunctionCall const*);

    utl::small_vector<ir::Value*> mapArguments(auto&& args);

    ir::Value* intConstant(APInt value);

    ir::Value* intConstant(size_t value, size_t bitwidth);

    ir::Value* floatConstant(APFloat value);

    ir::Value* constant(ssize_t value, ir::Type const* type);

    /// Associate object with program values
    /// Values are stored in `objectMap`
    void memorizeObject(sema::Object const* object, Value value);

    /// Associate array IDs with their size
    void memorizeArraySize(uint32_t ID, Value size);

    /// \overload
    void memorizeArraySize(uint32_t ID, size_t size);

    /// Retrieve stored array size
    Value getArraySize(uint32_t ID) const;

    ///
    std::optional<Value> tryGetArraySize(uint32_t ID) const;
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_LOWERINGCONTEXT_H_
