// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_MODULE_H_
#define SCATHA_IR_MODULE_H_

#include <span>

#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/List.h>
#include <scatha/Common/OpaqueRange.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/IR/Fwd.h>

namespace scatha::ir {

class SCATHA_API Module {
public:
    Module()                  = default;
    Module(Module const& rhs) = delete;
    Module(Module&& rhs) noexcept;
    Module& operator=(Module const& rhs) = delete;
    Module& operator=(Module&& rhs) noexcept;
    ~Module();

    auto constantData() const {
        return _constantData |
               ranges::views::transform(
                   [](auto& p) -> auto const* { return p.get(); });
    }

    auto structures() { return makeOpaqueRange(structs); }

    auto structures() const { return makeOpaqueRange(structs); }

    auto& functions() { return funcs; }

    auto const& functions() const { return funcs; }

    void addStructure(UniquePtr<StructureType> structure);

    void addGlobal(UniquePtr<Value> value);

    void addConstantData(UniquePtr<ConstantData> value);

    void addFunction(Function* function);

    void addFunction(UniquePtr<Function> function);

    void eraseFunction(Function* function);

    List<Function>::iterator eraseFunction(List<Function>::const_iterator itr);

    List<Function>::iterator begin();
    List<Function>::const_iterator begin() const;

    List<Function>::iterator end();
    List<Function>::const_iterator end() const;

    bool empty() const;

    Function& front();
    Function const& front() const;

    Function& back();
    Function const& back() const;

private:
    utl::vector<UniquePtr<StructureType>> structs;
    utl::vector<UniquePtr<Value>> _globals;
    utl::vector<UniquePtr<ConstantData>> _constantData;
    List<Function> funcs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_MODULE_H_
