# Scatha compiler

Multi paradigm toy language with a (not yet) optimizing byte code compiler and a virtual machine.

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
    


