fn main() -> int {
    var x: X
    // Explicitly pass x by reference
    f(&x);
    // Should member function calls be implicitly by reference?
    x.f();
}

struct X {
    // Pass self by reference
    fn f(self&) -> int { return i; }
    
    // Pass self by mutable reference
    fn f(self mut&) -> int {
        self.i += 1;
        return self.i; 
    }
    
    var i: int;
}

fn f(x: X mut&) -> X mut& {
    var y: X;
}
