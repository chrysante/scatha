// clang-format off

// ===----------------------------------------------------------------------===
// === List of all builtin functions ---------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SVM_BUILTIN_DEF
#   define SVM_BUILTIN_DEF(name, ...)
#endif

/// ## Common Math Functions
SVM_BUILTIN_DEF(abs_f64,   Const, { F64() }, F64())
SVM_BUILTIN_DEF(exp_f64,   Const, { F64() }, F64())
SVM_BUILTIN_DEF(exp2_f64,  Const, { F64() }, F64())
SVM_BUILTIN_DEF(exp10_f64, Const, { F64() }, F64())
SVM_BUILTIN_DEF(log_f64,   Const, { F64() }, F64())
SVM_BUILTIN_DEF(log2_f64,  Const, { F64() }, F64())
SVM_BUILTIN_DEF(log10_f64, Const, { F64() }, F64())
SVM_BUILTIN_DEF(pow_f64,   Const, { F64(), F64() }, F64())
SVM_BUILTIN_DEF(sqrt_f64,  Const, { F64() }, F64())
SVM_BUILTIN_DEF(cbrt_f64,  Const, { F64() }, F64())
SVM_BUILTIN_DEF(hypot_f64, Const, { F64(), F64() }, F64())
SVM_BUILTIN_DEF(sin_f64,   Const, { F64() }, F64())
SVM_BUILTIN_DEF(cos_f64,   Const, { F64() }, F64())
SVM_BUILTIN_DEF(tan_f64,   Const, { F64() }, F64())
SVM_BUILTIN_DEF(asin_f64,  Const, { F64() }, F64())
SVM_BUILTIN_DEF(acos_f64,  Const, { F64() }, F64())
SVM_BUILTIN_DEF(atan_f64,  Const, { F64() }, F64())
SVM_BUILTIN_DEF(fract_f64, Const, { F64() }, F64())
SVM_BUILTIN_DEF(floor_f64, Const, { F64() }, F64())
SVM_BUILTIN_DEF(ceil_f64,  Const, { F64() }, F64())

SVM_BUILTIN_DEF(abs_f32,   Const, { F32() }, F32())
SVM_BUILTIN_DEF(exp_f32,   Const, { F32() }, F32())
SVM_BUILTIN_DEF(exp2_f32,  Const, { F32() }, F32())
SVM_BUILTIN_DEF(exp10_f32, Const, { F32() }, F32())
SVM_BUILTIN_DEF(log_f32,   Const, { F32() }, F32())
SVM_BUILTIN_DEF(log2_f32,  Const, { F32() }, F32())
SVM_BUILTIN_DEF(log10_f32, Const, { F32() }, F32())
SVM_BUILTIN_DEF(pow_f32,   Const, { F32(), F32() }, F32())
SVM_BUILTIN_DEF(sqrt_f32,  Const, { F32() }, F32())
SVM_BUILTIN_DEF(cbrt_f32,  Const, { F32() }, F32())
SVM_BUILTIN_DEF(hypot_f32, Const, { F32(), F32() }, F32())
SVM_BUILTIN_DEF(sin_f32,   Const, { F32() }, F32())
SVM_BUILTIN_DEF(cos_f32,   Const, { F32() }, F32())
SVM_BUILTIN_DEF(tan_f32,   Const, { F32() }, F32())
SVM_BUILTIN_DEF(asin_f32,  Const, { F32() }, F32())
SVM_BUILTIN_DEF(acos_f32,  Const, { F32() }, F32())
SVM_BUILTIN_DEF(atan_f32,  Const, { F32() }, F32())
SVM_BUILTIN_DEF(fract_f32, Const, { F32() }, F32())
SVM_BUILTIN_DEF(floor_f32, Const, { F32() }, F32())
SVM_BUILTIN_DEF(ceil_f32,  Const, { F32() }, F32())

/// ## Memory Management
SVM_BUILTIN_DEF(memcpy,  None,  {
    pointer(QualType::Mut(arrayType(Byte()))),
    pointer(QualType::Const(arrayType(Byte())))
}, Void())
SVM_BUILTIN_DEF(memmove,  None,  {
    pointer(QualType::Mut(arrayType(Byte()))),
    pointer(QualType::Const(arrayType(Byte())))
}, Void())
SVM_BUILTIN_DEF(memset,  None,  {
    pointer(QualType::Mut(arrayType(Byte()))),
    S64()
}, Void())
SVM_BUILTIN_DEF(alloc, None, { S64(), S64() },
                pointer(QualType::Mut(arrayType(Byte()))))
SVM_BUILTIN_DEF(dealloc, None, {
    pointer(QualType::Mut(arrayType(Byte()))),
    S64()
}, Void())

/// ## Console Output
SVM_BUILTIN_DEF(putchar,   None,  { Byte() }, Void())
SVM_BUILTIN_DEF(puti64,    None,  { S64() }, Void())
SVM_BUILTIN_DEF(putf64,    None,  { F64() }, Void())
SVM_BUILTIN_DEF(putstr,    None,
                { strPointer() }, Void())
SVM_BUILTIN_DEF(putln,    None,
                { strPointer() }, Void())
SVM_BUILTIN_DEF(putptr,    None,  { pointer(Byte()) }, Void())

/// ## Console Input
/// Allocates memory using `__builtin_alloc()` and thus requires the caller to
/// free the memory using `__builtin_dealloc()`
/// Signature: `() -> *mut str`
SVM_BUILTIN_DEF(readline,   None, {},
                strPointer(Mutability::Mutable))

/// ## String conversion

/// Returns true on successful conversion
/// Signature: `(result: &mut int, text: *str, base: int) -> bool`
SVM_BUILTIN_DEF(strtos64,    None, {
    reference(QualType::Mut(S64())),
    strPointer(),
    S64()
}, Bool())

/// Returns true on successful conversion
/// Signature: `(result: &mut double, text: *str) -> bool`
SVM_BUILTIN_DEF(strtof64,    None, {
    reference(QualType::Mut(F64())),
    strPointer()
}, Bool())

/// # FString runtime support

/// Expects a dynamically allocated buffer as the first parameter. Writes the `text` parameters into the buffer at specified offset.
/// Reallocates the buffer if it's too small to write `text`.
/// \Returns the potentially reallocated buffer
/// Signature: `(buffer: *mut str, offset: int, text: *str) -> *mut str*`
SVM_BUILTIN_DEF(fstring_writestr, None, {
    pointer(Str(), Mutability::Mutable),
    reference(S64(), Mutability::Mutable),
    strPointer()
}, pointer(Str(), Mutability::Mutable))

///
SVM_BUILTIN_DEF(fstring_writes64, None, {
    pointer(Str(), Mutability::Mutable),
    reference(S64(), Mutability::Mutable),
    S64()
}, pointer(Str(), Mutability::Mutable))

///
SVM_BUILTIN_DEF(fstring_writef64, None, {
    pointer(Str(), Mutability::Mutable),
    reference(S64(), Mutability::Mutable),
    F64()
}, pointer(Str(), Mutability::Mutable))

///
SVM_BUILTIN_DEF(fstring_trim, None, {
    pointer(Str(), Mutability::Mutable),
    S64()
}, pointer(Str(), Mutability::Mutable))

/// ## Debug trap
SVM_BUILTIN_DEF(trap,    None,  {  }, Void())

/// Quick and dirty randon number generation.
SVM_BUILTIN_DEF(rand_i64, None,  {  }, S64())

#undef SVM_BUILTIN_DEF
