
struct X {
    /// Default constructor
    fn new(&mut this) {
        print("Created X\n");
    }
    
    /// Copy constructor
    fn new(&mut this, rhs: &X) {
        print("Copied X\n");
    }
    
    /// Move constructor
    fn move(&mut this, rhs: &mut X) {
        print("Moved X\n");
    }
    
    fn delete(&mut this) {
        print("Deleted X\n");
    }
}

fn take(x: X) {

}

public fn main() {
    var x: X;
    var y = x;
    take(x);
}


fn print(text: &str) {
    __builtin_putstr(&text);
}

