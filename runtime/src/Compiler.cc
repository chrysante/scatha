#include "Runtime/Compiler.h"

#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/IRGen/IRGen.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Opt/Optimizer.h>
#include <scatha/Parser/Parser.h>
#include <scatha/Sema/Analyze.h>

#include "Runtime/Program.h"

using namespace scatha;

Compiler::Compiler() {
    mapType(typeid(void), sym.Void());
    mapType(typeid(bool), sym.Bool());
    mapType(typeid(int8_t), sym.S8());
    mapType(typeid(int16_t), sym.S16());
    mapType(typeid(int32_t), sym.S32());
    mapType(typeid(int64_t), sym.S64());
    mapType(typeid(uint8_t), sym.U8());
    mapType(typeid(uint16_t), sym.U16());
    mapType(typeid(uint32_t), sym.U32());
    mapType(typeid(uint64_t), sym.U64());
    mapType(typeid(float), sym.F32());
    mapType(typeid(double), sym.F64());
}

sema::StructType const* Compiler::declareType(StructDesc desc) {
    auto* type = sym.declareStructureType(desc.name);
    assert(false && "Unimplemented");
    return type;
}

static size_t const FunctionSlot = 16;

FuncDecl Compiler::declareFunction(std::string name,
                                   sema::FunctionSignature signature) {
    size_t slot = FunctionSlot;
    size_t index = functionIndex++;
    auto* function = sym.declareForeignFunction(name,
                                                slot,
                                                index,
                                                std::move(signature),
                                                sema::FunctionAttribute::None);
    return { .function = function, .slot = slot, .index = index };
}

sema::Type const* Compiler::mapType(std::type_info const& key,
                                    sema::Type const* value) {
    auto [itr, success] = typeMap.insert({ key, value });
    assert(success);
    return itr->second;
}

sema::Type const* Compiler::mapType(std::type_info const& key,
                                    StructDesc valueDesc) {
    return mapType(key, declareType(std::move(valueDesc)));
}

sema::Type const* Compiler::getType(std::type_info const& key) const {
    auto itr = typeMap.find(key);
    if (itr != typeMap.end()) {
        return itr->second;
    }
    return nullptr;
}

void Compiler::addSourceText(std::string text, std::filesystem::path path) {
    sourceFiles.push_back(SourceFile::make(std::move(text), std::move(path)));
}

void Compiler::addSourceFile(std::filesystem::path path) {
    sourceFiles.push_back(SourceFile::load(std::move(path)));
}

Program Compiler::compile() {
    IssueHandler issueHandler;
    auto ast = parser::parse(sourceFiles, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(sourceFiles);
    }
    if (!ast) {
        throw std::runtime_error("Compilation failed");
    }
    auto analysisResult = sema::analyze(*ast, sym, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(sourceFiles);
    }
    if (issueHandler.haveErrors()) {
        throw std::runtime_error("Compilation failed");
    }
    auto [context, mod] = irgen::generateIR(*ast, sym, analysisResult, {});
    opt::optimize(context, mod, 1);
    auto asmStream = cg::codegen(mod);
    auto [bindata, binsym] = Asm::assemble(asmStream);
    Program prog;
    std::tie(prog._data, prog._sym, prog._binsym) =
        std::tuple{ std::move(bindata), std::move(sym), std::move(binsym) };
    return prog;
}
