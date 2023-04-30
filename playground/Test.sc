

struct X {
    fn setValue(&mut this, value: int) {
        this.value = value;
    }
    // fn getValue(&this) -> int {
    //     return this.value;
    // }
    var value: int;
}

fn getValue(x: &X) -> int {
    return x.value;
}

public fn main() -> int {
    var x: X;
    x.setValue(1);
    return x.getValue();
}

