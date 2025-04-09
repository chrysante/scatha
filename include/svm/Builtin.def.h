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
/// The `fstring_write*` functions expect a dynamically allocated buffer with an
/// alignment of 1 as the first parameter.
/// The third parameter is written into the buffer at the specified offset.
/// The offset is updated to the end of the written argument.
/// The buffer is reallocated if it's too small to write the argument.
/// The potentially reallocated buffer is returned.

/// Signature: `(buffer: *mut str, offset: int, text: *str) -> *mut str`
SVM_BUILTIN_DEF(fstring_writestr, None, {
    pointer(Str(), Mutability::Mutable),
    reference(S64(), Mutability::Mutable),
    strPointer()
}, pointer(Str(), Mutability::Mutable))

/// Signature: `(buffer: *mut str, offset: int, arg: s64) -> *mut str`
SVM_BUILTIN_DEF(fstring_writes64, None, {
    pointer(Str(), Mutability::Mutable),
    reference(S64(), Mutability::Mutable),
    S64()
}, pointer(Str(), Mutability::Mutable))

/// Signature: `(buffer: *mut str, offset: int, arg: u64) -> *mut str`
SVM_BUILTIN_DEF(fstring_writeu64, None, {
    pointer(Str(), Mutability::Mutable),
    reference(S64(), Mutability::Mutable),
    U64()
}, pointer(Str(), Mutability::Mutable))

/// Signature: `(buffer: *mut str, offset: int, arg: f64) -> *mut str`
SVM_BUILTIN_DEF(fstring_writef64, None, {
    pointer(Str(), Mutability::Mutable),
    reference(S64(), Mutability::Mutable),
    F64()
}, pointer(Str(), Mutability::Mutable))

/// Signature: `(buffer: *mut str, offset: int, arg: byte) -> *mut str`
SVM_BUILTIN_DEF(fstring_writechar, None, {
    pointer(Str(), Mutability::Mutable),
    reference(S64(), Mutability::Mutable),
    Byte()
}, pointer(Str(), Mutability::Mutable))

/// Signature: `(buffer: *mut str, offset: int, arg: bool) -> *mut str`
SVM_BUILTIN_DEF(fstring_writebool, None, {
    pointer(Str(), Mutability::Mutable),
    reference(S64(), Mutability::Mutable),
    Bool()
}, pointer(Str(), Mutability::Mutable))

/// Signature: `(buffer: *mut str, offset: int, arg: *byte) -> *mut str`
SVM_BUILTIN_DEF(fstring_writeptr, None, {
    pointer(Str(), Mutability::Mutable),
    reference(S64(), Mutability::Mutable),
    pointer(arrayType(Byte()), Mutability::Const)
}, pointer(Str(), Mutability::Mutable))

/// Signature: `(buffer: *mut str, targetSize: int) -> *mut str`
/// Reallocates the buffer if its size differs from `targetSize`
SVM_BUILTIN_DEF(fstring_trim, None, {
    pointer(Str(), Mutability::Mutable),
    S64()
}, pointer(Str(), Mutability::Mutable))

/// Signature: `(path: *str, openMode: int) -> int`
/// Opens the file specified by \p path
/// \p openMode specifies the following flags
///
///     0x01 (app)    seek to the end of stream before each write
///     0x02 (binary) open in binary mode
///     0x04 (in)     open for reading
///     0x08 (out)    open for writing
///     0x10 (trunc)  discard the contents of the stream when opening
///     0x20 (ate)    seek to the end of stream immediately after open
///
/// \Returns an opaque file handle
SVM_BUILTIN_DEF(fileopen, None, {
    pointer(Str(), Mutability::Const),
    S64()
}, S64())

/// Signature: `(filehandle: int) -> bool`
/// Closes the file handle
/// \Returns `true` on success, `false` otherwise
SVM_BUILTIN_DEF(fileclose, None, {
    S64()
}, Bool())

/// Signature: `(value: byte, filehandle: int) -> bool`
/// Writes \p value into the file \p filehandle
/// \Returns `true` on success, `false` otherwise
SVM_BUILTIN_DEF(fileputc, None, {
    S64(),
    Byte(),
}, Bool())

/// Signature: `(buffer: *[byte], objSize: int, filehandle: int) -> bool`
/// Writes up to `buffer.count / objSize` objects into \p filehandle
/// \Returns the number of objects written
SVM_BUILTIN_DEF(filewrite, None, {
    pointer(QualType::Const(arrayType(Byte()))),
    S64(),
    S64()
}, S64())

/// ## Debug trap
SVM_BUILTIN_DEF(trap,    None,  {  }, Void())

/// ## Exit
SVM_BUILTIN_DEF(exit,    None,  { S64() }, Void())

/// Quick and dirty randon number generation.
SVM_BUILTIN_DEF(rand_i64, None,  {  }, S64())

#undef SVM_BUILTIN_DEF
