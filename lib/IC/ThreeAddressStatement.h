#ifndef SCATHA_IC_THREEADDRESSSTATEMENT_H_
#define SCATHA_IC_THREEADDRESSSTATEMENT_H_

#include <algorithm>
#include <iosfwd>
#include <span>
#include <variant>

#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Sema/SymbolID.h"

namespace scatha::ic {

struct EmptyArgument {
    sema::TypeID typeID;
};

struct Variable {
    explicit Variable(sema::SymbolID id): _idChain{ id } {}
    explicit Variable(std::span<sema::SymbolID> ids): _idChain(ids.begin(), ids.end()) {}

    sema::SymbolID id() const { return _idChain.back(); }

    void append(Variable const& rhs) {
        _idChain.reserve(_idChain.size() + rhs._idChain.size());
        std::copy(rhs._idChain.begin(), rhs._idChain.end(), std::back_inserter(_idChain));
    }

    auto begin() const { return _idChain.begin(); }
    auto end() const { return _idChain.end(); }

private:
    utl::small_vector<sema::SymbolID> _idChain;
};

struct Temporary {
    size_t index;
    sema::TypeID type;
};

struct LiteralValue {
    LiteralValue(u64 value, sema::TypeID type): value(value), type(type) {}
    explicit LiteralValue(ast::IntegerLiteral const& lit): LiteralValue(lit.value, lit.typeID) {}
    explicit LiteralValue(ast::BooleanLiteral const& lit): LiteralValue(lit.value, lit.typeID) {}
    explicit LiteralValue(ast::FloatingPointLiteral const& lit):
        LiteralValue(utl::bit_cast<u64>(lit.value), lit.typeID) {}

    u64 value;
    sema::TypeID type;
};

struct Label {
    static constexpr u64 functionBeginIndex = static_cast<u64>(-1);

    Label() = default;
    explicit Label(sema::SymbolID functionID, u64 index = functionBeginIndex): functionID(functionID), index(index) {}

    sema::SymbolID functionID;
    u64 index = functionBeginIndex;
};

struct FunctionLabel {
    struct Parameter {
        sema::SymbolID id;
        sema::TypeID type;
    };

public:
    explicit FunctionLabel(ast::FunctionDefinition const&);

    sema::SymbolID functionID() const { return _functionID; };

    std::span<Parameter const> parameters() const { return _parameters; }

private:
    utl::small_vector<Parameter> _parameters;
    sema::SymbolID _functionID;
};

struct FunctionEndLabel {};

struct If {};

using TasArgumentTypeVariant = std::variant<EmptyArgument, Variable, Temporary, LiteralValue, Label, If>;

struct TasArgument: TasArgumentTypeVariant {
    using TasArgumentTypeVariant::TasArgumentTypeVariant;
    TasArgument(): TasArgumentTypeVariant(EmptyArgument{}) {}

    template <typename... F>
    decltype(auto) visit(utl::visitor<F...> const& visitor) {
        return std::visit(visitor, *this);
    }

    template <typename... F>
    decltype(auto) visit(utl::visitor<F...> const& visitor) const {
        return std::visit(visitor, *this);
    }

    enum Kind { empty, variable, temporary, literalValue, label, conditional };

    bool is(Kind kind) const { return index() == kind; }

    template <typename T>
    T& as() {
        return std::get<T>(*this);
    }
    template <typename T>
    T const& as() const {
        return std::get<T>(*this);
    }
};

enum class Operation : u8 {
    mov, // result <- arg

    /// Function call operations
    param,     // void   <- arg
    getResult, // result <- void
    call,      // void   <- Label
    ret,       // void   <- void

    /// Arithmetic
    add,  // result <- arg1, arg2
    sub,  // result <- arg1, arg2
    mul,  // result <- arg1, arg2
    div,  // result <- arg1, arg2
    idiv, // result <- arg1, arg2
    rem,  // result <- arg1, arg2
    irem, // result <- arg1, arg2
    fadd, // result <- arg1, arg2
    fsub, // result <- arg1, arg2
    fmul, // result <- arg1, arg2
    fdiv, // result <- arg1, arg2
    sl,   // result <- arg1, arg2
    sr,   // result <- arg1, arg2
    And,  // result <- arg1, arg2
    Or,   // result <- arg1, arg2
    XOr,  // result <- arg1, arg2

    /// Relational operations
    eq,
    neq,
    ils,
    ileq,
    ig,
    igeq,
    uls,
    uleq,
    ug,
    ugeq,
    feq,
    fneq,
    fls,
    fleq,
    fg,
    fgeq,

    lnt, // logical not
    bnt, // bitwise not

    jmp, // Here we only have unconditional jumps and use conditional statements
         // to represent conditional jumps

    ifPlaceholder,

    _count
};

std::string_view toString(Operation);

std::ostream& operator<<(std::ostream&, Operation);

int argumentCount(Operation);

bool isJump(Operation);
bool isRelop(Operation);
Operation reverseRelop(Operation);

struct ThreeAddressStatement {
    Operation operation;
    TasArgument result;
    TasArgument arg1;
    TasArgument arg2;

    // For jumps and calls the label is in arg1 by convention
    Label getLabel() const { return arg1.as<Label>(); }
};

} // namespace scatha::ic

#endif // SCATHA_IC_THREEADDRESSSTATEMENT_H_
