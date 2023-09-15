
struct X {
    
    fn new(&mut this) {}
    
    fn new(&mut this, n: int) {}
    
    fn new(&mut this, rhs: &X) {}
    
    fn delete(&mut this) {}
    
}

fn main() {
    // var x = X();
    
    var x = X(2);
    var y = x;
    
    // x = X();
}
