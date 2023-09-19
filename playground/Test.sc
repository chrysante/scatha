export fn foo(x: int) -> int {
    var y: int;
    if (x > 42) {
        y = 5;
    }
    return y;
}


export fn bar(x: int) -> int {
    var y: int;
    if (x < 100) {
        if (x < 50) {
            y = 5;
        }
    }
    else {
        if (x < 150) {
            y = 10;
        }
    }
    return y;
}
