#ifndef SCATHA_IRGEN_FUNCGENCONTEXTBASE_H_
#define SCATHA_IRGEN_FUNCGENCONTEXTBASE_H_

#include <deque>
#include <string>
#include <string_view>

#include <svm/Builtin.h>
#include <utl/function_view.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "IR/Builder.h"
#include "IR/CFG/BasicBlock.h"
#include "IR/Fwd.h"
#include "IRGen/CallingConvention.h"
#include "IRGen/FunctionGeneration.h"
#include "IRGen/IRGen.h"
#include "IRGen/Maps.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

/// Base class of context objects for function generation of both user defined
/// and compiler generated functions
struct FuncGenContextBase: FuncGenParameters, ir::FunctionBuilder {
    Config config;
    ValueMap valueMap;
    ir::StructType const* arrayPtrType = nullptr;
    ir::StructType const* dynPtrType = nullptr;

    using FuncGenParameters::ctx;

    FuncGenContextBase(Config config, FuncGenParameters params);

    /// Map \p semaFn to the corresponding IR function. If the function is not
    /// declared it will be declared.
    ir::Callable* getFunction(sema::Function const* semaFn);

    /// Get the calling convention of \p function
    CallingConvention getCC(sema::Function const* function);

    ///
    ir::ForeignFunction* getBuiltin(svm::Builtin builtin);

    /// Converts the value \p value to representation \p repr
    Value to(ValueRepresentation repr, Value const& value);

    /// Converts the value \p value to packed representation
    Value pack(Value const& value);

    /// Converts the value \p value to unpacked representation
    Value unpack(Value const& value);

    /// Converts the atom \p atom to \p location
    Atom to(ValueLocation location, Atom atom, ir::Type const* type,
            std::string name);

    /// Stores the atom \p atom to a new local memory allocation if it is not
    /// already in memory
    Atom toMemory(Atom atom);

    /// Loads the atom \p atom to a register if it is not already in a register
    Atom toRegister(Atom atom, ir::Type const* type, std::string name);

    /// Inserts `ExtractValue` instructions for every member in \p atom
    /// \Pre \p atom must be in register
    utl::small_vector<Atom, 2> unpackRegister(Atom atom, std::string name);

    /// Inserts `GetElementPointer` instructions for every member in \p atom
    /// \Pre \p atom must be in memory
    utl::small_vector<Atom, 2> unpackMemory(Atom atom,
                                            ir::RecordType const* type,
                                            std::string name);

    ///
    ir::Value* toPackedRegister(Value const& value);

    ///
    ir::Value* toPackedMemory(Value const& value);

    /// \param semaType The type of the expression that we want to get the array
    /// size of
    /// \Returns the array size of the array or pointer or reference to
    /// array \p value
    /// \Pre \p value must be an array or a pointer or reference
    /// to an array
    /// If \p value is a statically sized array, the static size is returned as
    /// a constant
    Value getArraySize(Value const& value);

    /// Emit a call to `memcpy`
    ir::Call* callMemcpy(ir::Value* dest, ir::Value* source,
                         ir::Value* numBytes);

    /// \overload for `size_t numBytes`
    ir::Call* callMemcpy(ir::Value* dest, ir::Value* source, size_t numBytes);

    /// Emit a call to `memset`
    ir::Call* callMemset(ir::Value* dest, ir::Value* numBytes, int value);

    /// \overload for `size_t numBytes`
    ir::Call* callMemset(ir::Value* dest, size_t numBytes, int value);

    /// Emits a multiply instruction to obtain the byte size of an array
    ir::Value* makeCountToByteSize(ir::Value* count, size_t elemSize);

    /// Emits an `sdiv` instruction to obtain the element count of an array
    ir::Value* makeByteSizeToCount(ir::Value* bytesize, size_t elemSize);

    /// Emits code that makes a copy of \p value
    /// The returned value will be in a register iff its size is not greater
    /// than `PreferredMaxRegisterValueSize`
    /// \Returns the copied value
    /// \Warning Must only be called for values with trivial lifetime
    Value copyValue(Value const& value);

    /// Generates a for loop over the range `[countersBegin[0], counterEnd)`
    /// The first values of the induction variables are the values in \p
    /// countersBegin The next values are the values if the invocation result of
    /// \p inc The argument to \p inc is a view over the values of the induction
    /// variables of the current loop iteration.
    /// The loop runs until the value of the first induction variable compares
    /// equal to \p counterEnd
    void generateForLoop(
        std::string_view name, std::span<ir::Value* const> countersBegin,
        ir::Value* counterEnd,
        utl::function_view<
            utl::small_vector<ir::Value*>(std::span<ir::Value* const> counters)>
            inc,
        utl::function_view<void(std::span<ir::Value* const> counters)> body);

    ///
    Value makeVoidValue(std::string name) const;
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_FUNCGENCONTEXTBASE_H_
