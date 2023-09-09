

struct X {
    fn new(&mut this, n: int) {
        this.value = n;
    }
    
    fn get(&mut this) -> &mut int {
        return this.value;
    }
    
    var value: int;
}

public fn main() -> int {
    var x = X(2);
    
    x.get() = 1;
    
    return x.value;
}























/*

struct X {
    fn new(&mut this) {
        this.value = 0;
        print("+");
        print(this.value);
    }

    fn new(&mut this, n: int) {
        this.value = n;
        print("+");
        print(this.value);
    }

    fn new(&mut this, rhs: &X) {
        this.value = rhs.value + 1;
        print("+");
        print(this.value);
    }

    fn delete(&mut this) {
        print("-");
        print(this.value);
    }

    var value: int;
}

fn copy(x: &X) -> X { return x; }
fn copy(x: X) -> X { return x; }

fn f(x: &X) {}

public fn main() -> int {

    var x = X();
    f(x);
    
    //var cond = false;
    //let x = X(1);
    //let y = X(2);
    //let z = cond ? x : y;
    //return z.value;
}

fn print(text: &str) {
    __builtin_putstr(&text);
}

fn print(n: int) {
    __builtin_puti64(n);
}

struct ScopeGuard {
    fn new(&mut this, text: &str) {
        this.text = &text;
    }
    fn delete(&mut this) {
        print(&this.text);
    }
    var text: &str;
}

fn take(x: X) -> X {
    print("took X(");
    print(x.value);
    print(")\n");
    var guard = ScopeGuard("Leaving take()\n");
    return x;
}

fn make() -> X {
    let r = X();
    return r;
}

*/

