
struct X {
    export fn print(msg: &[byte]) {
        __builtin_putstr(msg);
    }
    
    fn print(n: int) {
        __builtin_puti64(n);
    }
}

fn main(n: int) -> int {
    X.print("Hello world\n");
    let cppVal = cppCallback(7);
    
    X.print("Recieved from C++: ");
    X.print(cppVal);
    X.print("\n");
    
    return 42;
}

export fn fac(n: int) -> int {
    return n <= 1 ? 1 : n * fac(n - 1);
}

export fn callback(n: int) {
    X.print("Callback\n");
    X.print(n);
    X.print(fac(n));
}

export fn allocate(bytes: int) -> *mut [byte] {
    return __builtin_alloc(bytes, 8);
}

export fn deallocate(bytes: *mut [byte]) {
    __builtin_dealloc(bytes, 8);
}
