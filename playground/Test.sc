

struct X {
    fn setValue(&mut this, value: int) {
        //this.value = value;
    }
    
    fn getValue(&this) {
        //return this.value;
        return 1;
    }
    
    var value: int;
}

public fn main() -> int {
    var x: X;
    x.setValue(1);
    
    return x.getValue();
}

