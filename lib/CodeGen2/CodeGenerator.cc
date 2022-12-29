#include "CodeGen2/CodeGenerator.h"

#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>

#include "Assembly/AssemblyStream.h"
#include "CodeGen2/RegisterDescriptor.h"
#include "IR/Module.h"
#include "IR/CFG.h"
#include "IR/Context.h"

using namespace scatha;
using namespace assembly;
using namespace cg2;

namespace {

struct Context {
    explicit Context(AssemblyStream& result): result(result) {}
    
    void run(ir::Module const& mod);
    
    void dispatch(ir::Value const& value);
    
    void generate(ir::Value const& value) { SC_UNREACHABLE(); }
    void generate(ir::Function const& function);
    void generate(ir::BasicBlock const& bb);
    void generate(ir::Alloca const&);
    
    size_t labelIndex(ir::Value const* value);
    
    RegisterDescriptor& currentRD() { return *_currentRD; }
    AssemblyStream& result;
    RegisterDescriptor* _currentRD = nullptr;
    size_t labelIndexCounter = 0;
    utl::hashmap<ir::Value const*, size_t> labelIndices;
};

} // namespace

assembly::AssemblyStream cg2::codegen(ir::Module const& mod) {
    assembly::AssemblyStream result;
    Context ctx(result);
    ctx.run(mod);
    return result;
}

void Context::run(ir::Module const& mod) {
    for (auto& function: mod.functions()) {
        dispatch(function);
    }
}

void Context::dispatch(ir::Value const& value) {
    visit(value, [this](auto const& node) { generate(node); });
}

void Context::generate(ir::Function const& function) {
    RegisterDescriptor rd;
    _currentRD = &rd;
    utl::scope_guard clearRD = [&]{ _currentRD = nullptr; };
    /// Declare parameters.
    for (auto& param: function.parameters()) { rd.resolve(param); }
    result << assembly::Label(labelIndex(&function));
    size_t const enterFnArgIndex = result.size() - Value8::size();
    for (auto& bb: function.basicBlocks()) {
        dispatch(bb);
    }
}

void Context::generate(ir::BasicBlock const& bb) {
    result << assembly::Label(labelIndex(&bb));
    for (auto& inst: bb.instructions) {
        dispatch(inst);
    }
}

void Context::generate(ir::Alloca const& allocaInst) {
    /// MARK: Todo list
    /// - Remove necessity for the enterFn instruction: Preallocate e.g. 8mb of registers and never reallocate. This
    ///   way 'all' positive register indices are available for function execution and all we have to do is bump the
    ///   register pointer on function calls. [done]
    /// - Then we still have to solve pointer invalidation on memory reallocation but that should be solvable somehow. [later]
    /// - Once we have both of these we can switch to using real pointers in the VM execution. 
    /// - Then implementation of alloca will be a walk in the park. Just bump the index of the register descriptor by
    ///   ceil_divide(size, reg_size), handle overalignment somehow and return the register pointer plus offset.
    ///   We already have load and store operations (movMR, movRM, etc). 
    allocaInst.allocatedType()->size();
}

size_t Context::labelIndex(ir::Value const* value) {
    auto [itr, success] = labelIndices.insert({ value, labelIndexCounter });
    if (success) { ++labelIndexCounter; }
    return itr->second;
}
