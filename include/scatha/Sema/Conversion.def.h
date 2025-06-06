// No include guards

// ===----------------------------------------------------------------------===
// === List of conversions betweens lvalues and rvalues --------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_VALUECATCONV_DEF
#define SC_VALUECATCONV_DEF(Name, Rank)
#endif

SC_VALUECATCONV_DEF(LValueToRValue, 0)
SC_VALUECATCONV_DEF(MaterializeTemporary, 1)

#undef SC_VALUECATCONV_DEF

// ===----------------------------------------------------------------------===
// === List of mutability conversions --------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_QUALCONV_DEF
#define SC_QUALCONV_DEF(Name, Rank)
#endif

SC_QUALCONV_DEF(MutToConst, 1)
SC_QUALCONV_DEF(StaticToDyn, 1)
SC_QUALCONV_DEF(DynToStatic, 1)

#undef SC_QUALCONV_DEF

// ===----------------------------------------------------------------------===
// === List of conversions between object types ----------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_OBJTYPECONV_DEF
#define SC_OBJTYPECONV_DEF(Name, Rank)
#endif

/// # Constructing conversions
/// These cases directly reflect the AST construct expressions
#define SC_ASTNODE_CONSTR_DEF(Name, ...) SC_OBJTYPECONV_DEF(Name, 2)
#include <scatha/AST/Lists.def.h>

SC_OBJTYPECONV_DEF(NullptrToRawPtr, 0)
SC_OBJTYPECONV_DEF(NullptrToUniquePtr, 0)
SC_OBJTYPECONV_DEF(UniqueToRawPtr, 1)

/// # Only valid for pointer target types

SC_OBJTYPECONV_DEF(ArrayPtr_FixedToDynamic, 1)
SC_OBJTYPECONV_DEF(Reinterpret_ValuePtr, 2)
SC_OBJTYPECONV_DEF(Reinterpret_ValuePtr_ToByteArray, 2)
SC_OBJTYPECONV_DEF(Reinterpret_ValuePtr_FromByteArray, 2)
SC_OBJTYPECONV_DEF(Reinterpret_DynArrayPtr_ToByte, 2)
SC_OBJTYPECONV_DEF(Reinterpret_DynArrayPtr_FromByte, 2)
SC_OBJTYPECONV_DEF(Ptr_DerivedToParent, 1)

/// # Only valid for lvalue target types

SC_OBJTYPECONV_DEF(ArrayRef_FixedToDynamic, 1)
SC_OBJTYPECONV_DEF(Reinterpret_ValueRef, 2)
SC_OBJTYPECONV_DEF(Reinterpret_ValueRef_ToByteArray, 2)
SC_OBJTYPECONV_DEF(Reinterpret_ValueRef_FromByteArray, 2)
SC_OBJTYPECONV_DEF(Reinterpret_DynArrayRef_ToByte, 2)
SC_OBJTYPECONV_DEF(Reinterpret_DynArrayRef_FromByte, 2)
SC_OBJTYPECONV_DEF(Ref_DerivedToParent, 1)

/// # Only valid for value target types

SC_OBJTYPECONV_DEF(Reinterpret_Value, 2)

/// ## Arithmetic conversions

#ifndef SC_ARITHMETIC_CONV_DEF
#define SC_ARITHMETIC_CONV_DEF SC_OBJTYPECONV_DEF
#endif

SC_ARITHMETIC_CONV_DEF(IntTruncTo8, 2)
SC_ARITHMETIC_CONV_DEF(IntTruncTo16, 2)
SC_ARITHMETIC_CONV_DEF(IntTruncTo32, 2)
SC_ARITHMETIC_CONV_DEF(SignedWidenTo16, 1)
SC_ARITHMETIC_CONV_DEF(SignedWidenTo32, 1)
SC_ARITHMETIC_CONV_DEF(SignedWidenTo64, 1)
SC_ARITHMETIC_CONV_DEF(UnsignedWidenTo16, 1)
SC_ARITHMETIC_CONV_DEF(UnsignedWidenTo32, 1)
SC_ARITHMETIC_CONV_DEF(UnsignedWidenTo64, 1)
SC_ARITHMETIC_CONV_DEF(FloatTruncTo32, 2)
SC_ARITHMETIC_CONV_DEF(FloatWidenTo64, 1)

SC_ARITHMETIC_CONV_DEF(SignedToUnsigned, 1)
SC_ARITHMETIC_CONV_DEF(UnsignedToSigned, 1)
SC_ARITHMETIC_CONV_DEF(SignedToFloat32, 2)
SC_ARITHMETIC_CONV_DEF(SignedToFloat64, 2)
SC_ARITHMETIC_CONV_DEF(UnsignedToFloat32, 2)
SC_ARITHMETIC_CONV_DEF(UnsignedToFloat64, 2)
SC_ARITHMETIC_CONV_DEF(FloatToSigned8, 2)
SC_ARITHMETIC_CONV_DEF(FloatToSigned16, 2)
SC_ARITHMETIC_CONV_DEF(FloatToSigned32, 2)
SC_ARITHMETIC_CONV_DEF(FloatToSigned64, 2)
SC_ARITHMETIC_CONV_DEF(FloatToUnsigned8, 2)
SC_ARITHMETIC_CONV_DEF(FloatToUnsigned16, 2)
SC_ARITHMETIC_CONV_DEF(FloatToUnsigned32, 2)
SC_ARITHMETIC_CONV_DEF(FloatToUnsigned64, 2)

SC_ARITHMETIC_CONV_DEF(IntToByte, 2)
SC_ARITHMETIC_CONV_DEF(ByteToSigned, 2)
SC_ARITHMETIC_CONV_DEF(ByteToUnsigned, 2)

#undef SC_ARITHMETIC_CONV_DEF

#undef SC_OBJTYPECONV_DEF
