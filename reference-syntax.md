    fn main() -> int {
        var x: X = { /\*\*/ };
        // Explicitly pass x by reference
        f(&x);
        // Should member function calls be implicitly by reference?
        x.f();
    }

    struct X {
        // Pass self by reference
        fn f(self&) -> int { return self.i; }
        
        // Pass self by mutable reference
        fn f(self mut&) -> int {
            self.i += 1;
            return self.i; 
        }
        
        var i: int;
    }

    fn f(x: X mut&) -> X mut& {
        var y: X = { /\*\*/ };
        x = y;    // Assign y to x.
        return x; // Well formed, x's lifetime is independent of f.
    }
    
    fn g() -> X mut& {
        var x: X = { /\*\*/ };
        return x; // Ill formed, returning reference to local stack memory. 
    }
    
    fn h() -> X mut& {
        var x: X = { /\*\*/ };
        let y = &x; // y is a reference to x.
        return &y;  // Ill formed; How to detect this though? Implement borrow checking like rust? 
    }
