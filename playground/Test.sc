
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}

fn makeX() -> X {
    var result: X;
    result.a = 1;
    result.b = true;
    result.c = false;
    result.d = true;
    return result;
}

fn main() -> int {
    var x = makeX();
    if x.c {
        return 2;
    }
    return 1;
}
 
