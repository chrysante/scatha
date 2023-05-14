# The Language

The Language (which is provisionally called "Scatha" in lack of a better name) is reminiscent of C, but also borrows (no pun intended) features and design decisions from Rust and other languages. 
Specifically the primary building block of programs are functions which are comprised of statements. It has a similar notion of expressions and operators as C or C++, but tries to be a bit higher level and more user friendly. 
Variables and references are immutable be default, mutability needs to be explicitly declared by the `mut` and `var` keywords. 

## Functions

`[<access-spec>] fn <function-name> ( [<param-decl> { , <param-decl> }* ] ) [ -> <return-type> ] { <statements> }`  

Functions can be declared with the `fn` keyword, followed by the name of the function.
Then follows a mandatory but possibly empty comma separated list of parameter declarations, enclosed by parantheses. 
An arrow `->` and the return type follow optionally. No specified return type is interpreted as `void` return type, but may in the future change to automatically deduced return type.

**Example**
```
fn foo(value: int, message: &str) -> double { 
    print(&message); 
    return double(value) / 3.0;
}
export fn main() {
    let bar = bar(5, "Hello World!\n");
}
``` 

*Note that this example will not compile because the `print()` function is not defined*

*Also note that right now for binary visibility the `public` keyword is used, but this will be changed to the more fitting `export`*

## Statements

Functions are made up of a sequence of statements. A statement can be any of the following:

### Function definition

See above, may not appear at function scope however

### Variable declaration

`let / var <variable-name> [ : <type> ] [ = <init-expression> ] ;` 

Variables are declared with the `let` keyword (for immutable) or `var` keyword (for mutable) variables, followed by the name of the variable. 
Types can be specified explicitly with a colon `:` after the variable name followed by an expression specifying the type (usually the name of the type).
The initial value can be specified by an initializing expression following the optional type specifier and an equal sign `=`.
While both the type specifier and the initializing expression are optional, one of them is always required, for the compiler to know the type of the variable.
In `struct` definitions explicit type specifiers are mandatory.      

**Example**
```
let foo = 42;
let baz: Bar;
var qux = Quux(a, b, c);
let bar: double = 1.0;
```

### Expression statements

Any expression followed by a semicolon `;` is an expression-statement. The expression will be evaluated when encountered by control flow and the value will be discarded.  

**Example**
```
1 + 2 * 3;             // Evaluates the arithmetic expression and discards the result
foo(bar, baz, 42);     // Calls the function `foo` with arguments `bar`, `baz` and 42 and discards the result (if any)
1 + 2 > 3 ? 4 : 5;     // Conditional operator
print("Hello world!"); // Calls function `print` with argument "Hello world!" 
```

### Conditional statements

`if <bool-expr> { <statements> } [ else { <statements> } ]`

Just like in C, except no parantheses around `<bool-expr>` are required.

### Loop statements

`for <var-init> ; <bool-expr> ; <inc-expr> { <statements> }`

Just like in C, except no parantheses around `<var-init> ; <bool-expr> ; <inc-expr>` are required. 

`while <bool-expr> { <statements> }`

`do { <statements> } while <bool-expr> ;`

Same story here. 

### Jump statements

`break;`
`continue;`

Must appear in loops, `break;` cancels execution of the loop, `continue;` skips executions of the current loop iteration. 

## References

`& <lvalue-or-reference-expression>`

`& mut <lvalue-or-reference-expression>`

Any object that has a logical address can be referenced with a reference expression.

**Example**
```
var baz = 0;
let ref = &baz; // `ref` now refers to `baz`
print(ref);     // Prints '0'
baz = 42;
print(ref);     // Prints '42'
```
 
References can be mutable, which allows reassignments of the original value.:
 
```
var baz = 0;
let ref = &mut baz; // `ref` now refers to `baz`
print(baz);         // Prints '0'
ref = 42;
print(baz);         // Prints '42'
```

For references to be mutable, the referred-to value must also be mutable:

```
let bar = 0;
let ref = &mut bar; // Error. Cannot take mutable reference to immutable variable `bar`
```

Reference-variables themselves can also be mutable, which allows for reassignment of the reference:

```
let bar = 0;
let baz = 1;
var ref = &bar; // `ref` refers to `bar`
ref = &baz;     // `ref` now refers to `baz`
```

Any of the four combinations of 
- 'const-reference-to-const'
- 'mutable-reference-to-const'
- 'const-reference-to-mutable'
- 'mutable-reference-to-mutable'

is allowed. 

To take a reference to a value referred to by another reference, the reference must again be taken explicitly:

```
let bar = 0;
let ref = &bar;
let qux = ref;   // `qux` is deduced to be of type int, and is assigned a copy of the value of `bar`
let quux = &ref; // `quux` is deduced to be of type &int (reference to int), and refers to the value of `bar`
```

In other words, references are deferenced implicitly. When copying references, their reference status must always explicitly be upheld if thats the desired behaviour.

Care must be taken with reassigning and especially storing references. The use of references that outlive their referred-to object has undefined behaviour. 

## Expressions

Pretty much like in C, differences will be explained in more detail in the future. 

## Arrays

Detailed explanation will follow, but in short: Arrays can be declared by square brackets around a type name, and an optional compile time constant integral count argument.

**Example**
```
let array = [1, 2, 3];  // `array` is a stack allocated array of three objects of type int
let bar: [int, 3];      // Same as above
let baz = &bar;         // `baz` is a reference to (fixed size) array.  
let qux: &[int] = &bar; // `qux` is a reference to dynamically sized array
print(qux.count);       // Prints '3' 
```

References to fixed size arrays are essentially pointers, since the size is known at compile time and carried by the type system.
References to dynamically sized arrays are pairs of a pointer and the size of the array (represented as 64 bit integer). The size of an array can be accessed via the `count` member.  

Elements of arrays can be accessed with the subscript operator `[]` like this:
```
let array = [1, 2, 3, 4];
print(array[1]); // Prints '2'
```
Indices into arrays are 0 based.

String literals have the type `&[byte]`, i.e. immutable reference to dynamically sized array of bytes. There is no dedicated `char` type. The type alias `str` is provided by the compiler for `[byte]` (array of bytes), like in Rust. An owning string class will be added through a standard library in the future. 
**Note:** String literals (and strings in general) are not null-terminated. References to dynamically sized arrays carry the runtime size, so there is no need for such shenanigans. 

## Constant expressions

Many integral and floating point valued expressions can be evaluated at compile time, detailed explanation follows.

## Structs

Users can define their own data types with the `struct` keyword, followed by a brace-enclosing list of variable declarations. 
Structs are aggregate/product types like in C and other C derived languages. 

**Example**
```
struct Baz {
    var bar:  s64; // Signed 64 bit integer. (int defaults to s64) 
    var qux:  u32; // Unsigned 32 bit integer
    var quux: s32;
}
```  

Memory layout of `struct`s is guaranteed to be what is generally expected, assumed and implemented by popular C and C++ compilers. 
Specifically that means the following:
- No padding at the beginning of a `struct`
- Every data member is placed at a memory location divisible by its own alignment. Padding bits are inserted as necessary to uphold this guarantee 
- The data members are layed out in the order of declaration
- The alignment of the struct is the maximal alignment of its data members
- The size of the struct is the sum of the sizes its data members plus padding bits

**Example**
```
struct S {
    var x: int;   // 64 bit signed integer
    var y: float; // 32 bit floating point value 
    var z: u32;   // 32 bit unsigned integer
}
```
This `struct S` has a size of 16 bytes and an alignment of 8 bytes. The member `x` dictates the alignment of 8 bytes, the members `y` and `z` share the 8 bytes after `x`.
 
This makes sharing data types between guest and host application code very straight-forward.

Function definitions may appear at struct scope. Unless they have an explicitly specified `this` argument as the first parameter, 
they are (in C++ terms) 'static' function, i.e. they don't operate on the member variables of the structure but only on their arguments.
For member functions, an explicit `this`, `&this` or `&mut this` argument can be specified as the first parameter. The member variables of the 
object can then be accessed through the `this` object or reference in the function.     

**Example**
```
struct S {
    fn sum(&this) -> int { // `this` is a reference-to-const
        var result = 0;
        for i = 0; i < this.data.count; ++i {
            result += this.data[i];
        }
        return result;
    }
    var baz: int;
    var data: &mut [int];
}
```
