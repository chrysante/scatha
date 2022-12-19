//#include "IR/Generate.h"
//
//#include <utl/vector.hpp>
//
//#include "Basic/Basic.h"
//#include "AST/AST.h"
//#include "Sema/SymbolTable.h"
//#include "IR/Struct.h"
//
//using namespace scatha;
//using namespace ir;
//
//namespace {
//
//struct Context {
//    explicit Context(sema::SymbolTable const& sym): sourceSymTable(sym) {}
//    
//    void dispatch(ast::AbstractSyntaxTree const& node);
//    
//    void generate(ast::TranslationUnit const&);
//    
//    sema::SymbolTable const& sourceSymTable;
//    ir::SymbolTable targetSymTable;
//    utl::vector<Struct> structs;
//    utl::vector<Function> functions;
//};
//
//} // namespace
//
//GeneratorResult ir::generate(ast::AbstractSyntaxTree const& ast, sema::SymbolTable const& symbolTable) {
//    Context ctx(symbolTable);
//    ctx.dispatch(ast);
//    return {
//        Module(/*std::move(ctx.structs), */std::move(ctx.functions)),
//        std::move(ctx.targetSymTable)
//    };
//}
//
//void Context::dispatch(ast::AbstractSyntaxTree const& node) {
//    SC_DEBUGFAIL();
//}
