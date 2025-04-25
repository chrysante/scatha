#ifndef SCATHA_IR_EQUALITYTESTHELPER_H_
#define SCATHA_IR_EQUALITYTESTHELPER_H_

#include <string>

#include <utl/vector.hpp>

#include "IR/Fwd.h"

namespace scatha::test::ir {

struct StructureEqTester {
    explicit StructureEqTester(std::string name): name(std::move(name)) {}

    StructureEqTester members(utl::vector<std::string> typenames) {
        memberTypenames = std::move(typenames);
        return std::move(*this);
    }

    void test(scatha::ir::StructType const&) const;

private:
    std::string name;
    utl::vector<std::string> memberTypenames;
};

inline StructureEqTester testStructure(std::string name) {
    return StructureEqTester(std::move(name));
}

struct InstructionEqTester {
    explicit InstructionEqTester(std::string name): name(std::move(name)) {}

    InstructionEqTester instType(scatha::ir::NodeType type) {
        nodeType = type;
        return std::move(*this);
    }

    InstructionEqTester references(utl::vector<std::string> names) {
        referedNames = std::move(names);
        return std::move(*this);
    }

    void test(scatha::ir::Instruction const& inst) const;

private:
    std::string name;
    scatha::ir::NodeType nodeType;
    utl::vector<std::string> referedNames;
};

inline InstructionEqTester testInstruction(std::string name) {
    return InstructionEqTester(std::move(name));
}

struct BasicBlockEqTester {
    explicit BasicBlockEqTester(std::string name): name(std::move(name)) {}

    BasicBlockEqTester instructions(utl::vector<InstructionEqTester> insts) {
        instTesters = std::move(insts);
        return std::move(*this);
    }

    void test(scatha::ir::BasicBlock const& basicBlock) const;

private:
    std::string name;
    utl::vector<InstructionEqTester> instTesters;
};

inline BasicBlockEqTester testBasicBlock(std::string name) {
    return BasicBlockEqTester(std::move(name));
}

struct FunctionEqTester {
    explicit FunctionEqTester(std::string name): name(std::move(name)) {}

    FunctionEqTester parameters(utl::vector<std::string> typenames) {
        paramTypenames = std::move(typenames);
        return std::move(*this);
    }

    FunctionEqTester basicBlocks(utl::vector<BasicBlockEqTester> testers) {
        bbTesters = std::move(testers);
        return std::move(*this);
    }

    void test(scatha::ir::Function const& function) const;

private:
    std::string name;
    utl::vector<std::string> paramTypenames;
    utl::vector<BasicBlockEqTester> bbTesters;
};

inline FunctionEqTester testFunction(std::string name) {
    return FunctionEqTester(std::move(name));
}

struct ModuleEqTester {
    explicit ModuleEqTester(scatha::ir::Module const* mod = nullptr):
        mod(mod) {}

    ModuleEqTester structures(utl::vector<StructureEqTester> structs) {
        this->structs = std::move(structs);
        if (mod) {
            testStructures(*mod);
        }
        return std::move(*this);
    }

    ModuleEqTester functions(utl::vector<FunctionEqTester> funcs) {
        this->funcs = std::move(funcs);
        if (mod) {
            testFunctions(*mod);
        }
        return std::move(*this);
    }

    void testStructures(scatha::ir::Module const& mod) const;

    void testFunctions(scatha::ir::Module const& mod) const;

private:
    scatha::ir::Module const* mod;
    utl::vector<FunctionEqTester> funcs;
    utl::vector<StructureEqTester> structs;
};

inline ModuleEqTester testModule(scatha::ir::Module const* mod = nullptr) {
    return ModuleEqTester(mod);
}

} // namespace scatha::test::ir

#endif // SCATHA_IR_EQUALITYTESTHELPER_H_
