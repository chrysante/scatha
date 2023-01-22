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

Take a look at [examples](examples/).
    
Take a look at the [grammar](docs/Grammar.md).

## Feature roadmap

### Framework

- Explicit (and maybe some implicit) casts

- Find a good solution for implementing references/pointers in the language that is powerful but not harmful 

- Arrays (probably depend on references)

- Strings (depend on arrays)

- Member functions (also depend on references in a way)

    - Uniform function call syntax
    
    - Constructors/destructors

- Dynamic dispatch
    
    - Interfaces/Protocols
    
    - Multiple dispatch

- Runtime integration mechanisms

- Textual IR representation / IR parser

- Optimizations

    - <s> Register promotion </s> done
    
    - <s> Constant folding </s> done
    
    - Inlining

### Compiler

- Better issue formatting


