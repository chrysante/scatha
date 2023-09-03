
fn printVal(x: &X) {
        print("[");
        print(x.value);
        print("]");
}

struct Y { var x: X; }

struct X {
    /// Default constructor
    fn new(&mut this) {
        this.value = 1;
        print("+ ");
        printVal(&this);
        print("\n");
    }

    fn new(&mut this, n: int) {
        this.value = n;
        print("+ ");
        printVal(&this);
        print(" (n: int)\n");
    }

    /// Copy constructor
    fn new(&mut this, rhs: &X) {
        this.value = rhs.value + 1;
        print("+ ");
        printVal(&this);
        print(" (Copy)\n");
    }

    /// Move constructor
    fn move(&mut this, rhs: &mut X) {
        print("+ ");
        printVal(&this);
        print(" (Move)\n");
    }

    fn delete(&mut this) {
        print("- ");
        printVal(&this);
        print("\n");
    }

    var value: int;
}

fn take(x: X) {

}

public fn main() -> int {
    //var x: X;
    //var y = x;
    //var z = X(X(4).value + 1);

    //return X().value;
    
    //var sum = 0;
    //for x = X(1); x.value <= 3; x.value += 1 {
    //        var y = X(2);
    //    if x.value == 2 {
    //        break;
    //    }
    //    sum += x.value;
    //}
    //return sum;
    
    //var x = X(1);
    //{
    //    var y = x;
    //}
    //var z = X(3);
    
    let x = X(1);
    take(x);
}


fn print(text: &str) {
    __builtin_putstr(&text);
}

fn print(n: int) {
    __builtin_puti64(n);
}
