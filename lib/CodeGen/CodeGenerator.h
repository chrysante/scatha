#ifndef SCATHA_CODEGEN_CODEGENERATOR_H_
#define SCATHA_CODEGEN_CODEGENERATOR_H_

#include <tuple>
#include <variant>

#include <utl/vector.hpp>

#include "Assembly/AssemblyStream.h"
#include "Basic/Basic.h"
#include "CodeGen/RegisterDescriptor.h"
#include "IC/ThreeAddressCode.h"

namespace scatha::codegen {

struct CurrentFunctionData {
    void addParam(size_t location, u8 registerIndex) {
        ++_paramCount;
        _maxParamCount = std::max(_maxParamCount, _paramCount);
        _parameterRegisterLocations.push_back({ location, registerIndex });
    }

    void resetParams() {
        _paramCount        = 0;
        _calledAnyFunction = true;
        _parameterRegisterLocations.clear();
    }

    size_t paramCount() const { return _paramCount; }
    size_t maxParamCount() const { return _maxParamCount; }

    bool calledAnyFunction() const { return _calledAnyFunction; }

    std::span<std::tuple<size_t, u8> const> parameterRegisterLocations() const { return _parameterRegisterLocations; };

    size_t allocRegArgIndex = static_cast<size_t>(-1);

private:
    bool _calledAnyFunction = false;
    size_t _paramCount      = 0;
    size_t _maxParamCount   = 0;
    utl::vector<std::tuple<size_t, u8>> _parameterRegisterLocations;
};

class SCATHA(API) CodeGenerator {
public:
    explicit CodeGenerator(ic::ThreeAddressCode const&);

    assembly::AssemblyStream run();

private:
    void generateBinaryArithmetic(assembly::AssemblyStream&, ic::ThreeAddressStatement const&);
    void generateComparison(assembly::AssemblyStream&, ic::ThreeAddressStatement const&);
    void generateComparisonStore(assembly::AssemblyStream&, ic::ThreeAddressStatement const&);
    void generateJump(assembly::AssemblyStream&, ic::ThreeAddressStatement const&);
    void generateConditionalJump(assembly::AssemblyStream&,
                                 ic::ThreeAddressStatement const&,
                                 ic::ThreeAddressStatement const&);

    struct ResolvedArg {
        CodeGenerator& self;
        ic::TasArgument const& arg;
        void streamInsert(assembly::AssemblyStream&) const;
    };
    friend struct ResolvedArg;
    friend assembly::AssemblyStream& operator<<(assembly::AssemblyStream&, ResolvedArg);

    ResolvedArg resolve(ic::TasArgument const&);

    struct FunctionRegisterCount {
        size_t local       = 0;
        size_t maxFcParams = 0;

        size_t total() const { return local + 2 + maxFcParams; }
    };

private:
    ic::ThreeAddressCode const& tac;
    RegisterDescriptor rd;

    CurrentFunctionData currentFunction;
};

} // namespace scatha::codegen

#endif // SCATHA_CODEGEN_CODEGENERATOR_H_
