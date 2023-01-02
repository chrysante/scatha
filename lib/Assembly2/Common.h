#ifndef SCATHA_ASSEMBLY2_COMMON_H_
#define SCATHA_ASSEMBLY2_COMMON_H_

#include <iosfwd>
#include <string_view>

#include "Basic/Basic.h"
#include "Common/Dyncast.h"

namespace scatha::asm2 {

/// Value types in asm. There are exactly 3 types: signed, unsigned and float
enum class Type {
    Signed, Unsigned, Float, _count
};
	
#define SC_ASM_ELEMENT_DEF(elem) class elem;
#include "Assembly2/Elements.def"

enum class ElemType {
#define SC_ASM_ELEMENT_DEF(elem) elem,
#include "Assembly2/Elements.def"
    _count
};

/// For \p dyncast compatibility.
inline ElemType dyncast_get_type(std::derived_from<Element> auto const& elem) {
    return elem.elementType();
}

} // namespace scatha::asm2

#define SC_ASM_ELEMENT_DEF(elem) \
    SC_DYNCAST_MAP(::scatha::asm2::elem, ::scatha::asm2::ElemType::elem);
#include "Assembly2/Elements.def"

namespace scatha::asm2 {

template <typename To, typename From>
std::unique_ptr<To> cast(std::unique_ptr<From>&& from) {
    auto ptr = from.get();
    from.release();
    return std::unique_ptr<To>(scatha::cast<To*>(ptr));
}

enum class CompareOperation {
#define SC_ASM_COMPARE_DEF(jmpcnd, ...) jmpcnd,
#include "Assembly2/Elements.def"
    _count
};

SCATHA(API) std::string_view toJumpInstName(CompareOperation condition);
SCATHA(API) std::string_view toSetInstName(CompareOperation condition);

enum class ArithmeticOperation {
#define SC_ASM_ARITHMETIC_DEF(op, ...) op,
#include "Assembly2/Elements.def"
    _count
};

SCATHA(API) std::string_view toString(ArithmeticOperation operation);

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, ArithmeticOperation operation);

} // namespace scatha::asm2

#endif // SCATHA_ASSEMBLY2_COMMON_H_

