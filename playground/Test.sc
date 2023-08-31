
struct X {
    /// Default constructor
    fn new(&mut this) {
        this.value = 1;
        print("Created X\n");
    }
    
    fn new(&mut this, n: int) {
        this.value = n;
        print("Created X\n");
    }

    /// Copy constructor
    fn new(&mut this, rhs: &X) {
        this.value = rhs.value + 1;
        print("Copied X\n");
    }

    /// Move constructor
    fn move(&mut this, rhs: &mut X) {
        print("Moved X\n");
    }

    fn delete(&mut this) {
        print("Deleted X [value = ");
        print(this.value);
        print("]\n");
    }

    var value: int;
}

fn take(x: X) {

}

public fn main() -> int {
    //var x: X;
    //var y = x;
    //var z = X(3);
    
    return X().value;
}


fn print(text: &str) {
    __builtin_putstr(&text);
}

fn print(n: int) {
    __builtin_puti64(n);
}
