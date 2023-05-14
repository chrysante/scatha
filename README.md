# Scatha compiler

Multi paradigm toy language with an optimizing byte code compiler and a virtual register machine.
The language is compiled and strongly and statically typed. 
Made for easy integration as an embedded scripting language into other applications. 

Read the [language reference](docs/LanguageRef.md) for details.

## Examples

### Simple program to calculate the GCD of 27 and 9 using the euclidean algorithm. 
    fn main() -> int {
        return gcd(27, 9);
    }
    
    fn gcd(a: int, b: int) -> int {
        if b == 0 {
            return a;        
        }
        return gcd(b, a % b);
    }

Take a look at some more sophisticated [examples](examples/).

## Feature roadmap

### Language
- Arithemtic types with explicitly specified width
    - Signed and unsigned integral types: ✅
        
        `s8`, `s16`, `s32`, `s64`

        `u8`, `u16`, `u32`, `u64`

    - Floating point types: ✅ 

        `f32`, `f64` 

    - Arithmetic vector types?

- `int` defaults to `s64` ✅
- `float` defaults to `f32` ✅
- `double` defaults to `f64` ✅
- Explicit (and maybe some implicit) casts
    - Explicit casts: 
        - Integral to floating point and vice versa. ✅
        - Narrowing integral and floating point conversion. ✅
    - Implicit casts:
        - Widening integral and floating point casts. ✅
- Find a good solution for implementing references/pointers in the language that is powerful but not harmful ✅
- Member functions (also depend on references in a way) ✅
    - Uniform function call syntax ✅
    - Constructors/destructors
    
- `unique`/`shared` references and expression, similar to `std::unique_ptr<>`/`std::make_unique<>` res. `std::shared_ptr<>`/`std::make_shared<>` for simple dynamic memory allocation. This depends on having well defined object lifetimes (constructors/destructors).
    - Syntax shall look something like this:
        ```
        {
            let i = unique int(1);
        } // i is deallocatd here
        {
            let j: unique int = nil;
            {
                let i = unique int(1);
                j = move i; // Now i == nil
            }
        } // j is deallocatd here
        ```
    
- Dynamic dispatch
    - Interfaces/Protocols
    - Multiple dispatch

- Generics 
- Standard library
    - `string`, `array<T>` / `vector<T>`, `list<T>`, `dict<K, V>` etc. classes/generics

### Optimizer
- Textual IR representation / IR parser. ✅
- Passes
    - Register promotion (SSA construction) ✅
    - Constant folding ✅
    - Dead code elimination ✅
    - Tail recursion elimination ✅
    - Inlining ✅
    - and more... ✅

### Compiler

- Better issue formatting ✅
- Parallel compilation
- Runtime integration mechanisms ✅
