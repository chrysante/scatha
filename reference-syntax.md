`Y`

    fn main() -> int {
        var x: X = { /**/ };
     
        // Explicitly pass `x` by reference
        f(&x);
        
        // Should member function calls be implicitly by reference?
        // -> Yes!
        x.f();
    }

    struct X {
        // Pass `this` by reference
        fn f(&this) -> int { return self.i; }
        
        // Pass `this` by mutable reference
        fn f(&mut self) -> int {
            self.i += 1;
            return self.i; 
        }
        
        var i: int;
    }

    fn f(x: &mut X) -> &mut X {
        var y: X = { /**/ };
        
        // Assign `y` to `x`
        x = y;
        
        // Well formed, `x`s lifetime is independent of `f`
        return x; 
    }
    
    fn g() -> X mut& {
        var x: X = { /**/ };
                
        // Undefined behaviour, returning reference to local stack memory
        return x;  
    }
    
    fn h() -> X mut& {
        var x: X = { /**/ };
        let y = &x; // `y` is a reference to `x`
        
        // Undefined behaviour
        return &y;   
    }

    fn a() -> int {
        var x = 0;
        var r = &x;
        r = 1;
        var s = &r;
        s = 2;
        return r; // Returns 2
    }
    
    struct Y {
        var i: int;
    }
    
    fn b(n: int) -> &mut unique Y {
        return unique X(i: n);
    }
    
    fn x() {
        /// `u` is of type `&mut unique X`
        let u = b(1); 
        return u.
    }   
    
## Different kinds of references
|||
|-|-|
| 'Raw' reference    | `& [mut] <object-type>`        |
| 'Unique' reference | `& [mut] unique <object-type>` <br> `& unique [mut] <object-type>` |

                        
## Binding references
Let `x`, `y` and `z` be objects of type `X`.

| |  <div style="width:200pt"></div> ||
|-|------|-|
| | `let r = &x;`                   | `r` is an immutable reference to an immutable object of type `X` |
| | `var r = &x;`<br>`r = &y;`      | `r` is a mutable reference to an immutable object of type `X` and thus can be reassigned. <br> After the line `r = &y;`, `r` refers to `y`. |
| | `let r = &mut x;` <br> `r = y;` | `r` is an immutable reference to a mutable object of type `X`. The line `r = y` assigns the referred value to `y`, so after this line `x == y` should hold. |
| | `let a = unique mut X(/*...*/);` <br> `let r = &a` <br> `a = x;` <br> `// r = x;` | `a` is of type `&unique mut X`. <br> `r` is of type `&X`, as it is assigned without the `mut` keyword. <br> Thus `a = x` is valid (assigns the value of `x` the the object referred to by `a`), but `r = x` is invalid, because `r` is a reference to immutable `X`). |
| | `var a = unique X(/*...*/);` <br> `r = &a;` <br> `a = unique X(/**/);` <br> `// r is now dangling ...` | |
| | `var a = unique mut X();` <br> `var b = unique mut X()` <br> `b = a;` <br> `b = &unique a;` <br> &nbsp; <br> &nbsp; | Allocate object `a`. <br> Allocate object `b`. <br> Assign value of `a` to `b`. <br> Assign the `unique` reference `a` to `b`. After this line `a` is `null`, the object previously referred to by `b` is deallocated and `b` now refers to the object previously referred to by `a`.|
| |  `var a = unique X();` <br> `// a = &x;` <br> `// a = &unique x;` | <br> Invalid. Can't assign raw reference to `unique` reference. <br> Invalid. Can't `unique`-reference non-`unique` object. |
 
| | <div style="width:200pt"></div> ||
|-|-|-|
| | `var a = [X, 10];` | `a` is a stack allocated array of 10 objects of type `X` |
| | `var a = unique [X];` <br> `a.push(x)` <br> &nbsp; | `a` is a heap allocated array of type `X` <br> `x` is copied into the back of the array. <br> The array is (potentially) reallocated. |
| | `var a = unique [X](10);` <br> `var b = &a;` <br> `var c = &unique a;` <br> &nbsp; | `a` is a heap allocated array of 10 elements. <br> `b` is an array-reference to `a`. <br> `c` steals the content of `a`; `a` is now empty. <br> `b` is still valid, now referring to contents of `c`. | 
|   | `var a = [x, y, z];` | Invalid (similar to `std::initializer_list`) |
|(1)| `var a: [X, 3] = [x, y, z];` | Stack allocated array of `x`, `y`, `z` |
|(2)| `var a = unique [x, y, z];` | Heap allocated array of `x`, `y`, `z` |
|   | `[1 : x, 2 : y, 3 : z]` | List of pairs / 2-tuples |
|   | `var d: std.dict<int, X> = [1: x, 2: y]` | List of pairs / 2-tuples converted to `std.dict` |

## Types
||||
|-|-|-|
||`X`| Object type |
||`&X`| Raw reference to `X`. References must refer to object types. |
||`&mut X`| Raw mutable reference to `X` |
||`unique X`| Unique reference to heap allocated object of type `X` |
||`[X, N]`| Stack allocated array of `N` objects of type `X`. `N` must be known at compile time. |
||`unique [X]`| Heap allocated array of type `X` |
||`&[X]`| Reference to array of type `X` |
|| `(X, int)` | Tuple of types `X` and `int` |
|| `[1:x, 2:y]` | Deduces as  |