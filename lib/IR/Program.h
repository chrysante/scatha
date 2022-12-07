#ifndef SCATHA_IR_IR_H_
#define SCATHA_IR_IR_H_

#include <span>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/Label.h"
#include "IR/VariableDeclaration.h"
#include "IR/Function.h"
#include "IR/Struct.h"
#include "IR/BranchStatement.h"

namespace scatha::ir {

//class ThreeAddressStatement: public StatementBase {
//public:
//    size_t index() const { return _index; }
//
//private:
//    size_t _index;
//};

class Program {
public:
    explicit Program(utl::vector<Struct> structs, utl::vector<Function> funcs):
        structs(std::move(structs)), funcs(std::move(funcs))
    {}
    
    std::span<Struct const> structures() const { return structs; }
    std::span<Function const> functions() const { return funcs; }
    
private:
    utl::vector<Struct> structs;
    utl::vector<Function> funcs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_IR_H_

