# Scatha compiler

Multi paradigm toy language with an optimizing byte code compiler and a virtual register machine.
The language is compiled and strongly and statically typed. 
Made for easy integration as an embedded scripting language into other applications. 

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

Take a look at the [examples](examples/).
    
Take a look at the [grammar](docs/Grammar.md).

## Feature roadmap

### Language
- Arithemtic types with explicitly specified width
    - Signed and unsigned integral types: 
        
        `s8, s16, s32, s64, s128`

        `u8, u16, u32, u64, u128`

    - Floating point types: 

        `f16, f32, f64, f128`

    - Arithmetic vector types?

- `int` defaults to `s64`
- `float` defaults to `f64`
- Explicit (and maybe some implicit) casts
    - Explicit casts: 
        - Integral to floating point and vive versa.
        - Narrowing integral and floating point conversion.
    - Implicit casts:
        - Widening integral and floating point casts.
- Find a good solution for implementing references/pointers in the language that is powerful but not harmful 
- Member functions (also depend on references in a way)
    - Uniform function call syntax
    - Constructors/destructors
- Dynamic dispatch
    - Interfaces/Protocols
    - Multiple dispatch

- Generics 
- Standard library
    - `string`, `array<T>` / `vector<T>`, `list<T>`, `dict<K, V>` etc. classes/generics

### Optimizer
- Textual IR representation / IR parser
- Passes
    - <font color="green">Register promotion</font> 
    - <font color="green">Constant folding</font>
    - Inlining

### Compiler

- Better issue formatting
- Parallel compilation
- Runtime integration mechanisms
