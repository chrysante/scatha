# Scatha compiler

Multi paradigm toy language with an optimizing byte code compiler and a virtual machine.
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
    
Take a look at the [Grammar](docs/Grammar.md).

## Feature roadmap

### Framework

- Find a good solution for implementing references/pointers in the frontend that is powerful but not harmful 

- Arrays (probably depend on references)

- Strings (depend on arrays)

- Member functions

    - Uniform function call syntax

- Dynamic dispatch
    
    - Interfaces/Protocols

- Well defined bytecode format for serialization  

- Runtime integration mechanisms

- Optimizations

    - Register promotion (almost complete)
    
    - Constant folding
    
    - Inlining

### Compiler

- Better issue formatting

## Issues

- Parser crashes on many invalid inputs

