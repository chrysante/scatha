

struct X {

    public fn print(msg: &[byte]) {
        __builtin_putstr(&msg);
    }
    
    fn print(n: int) {
        __builtin_puti64(n);
    }

}

public fn main(n: int) -> int {
    X.print("Hello world\n");
    let cppVal = cppCallback(7);
    
    X.print("Recieved from C++: ");
    X.print(cppVal);
    X.print("\n");
    
    return 42;
}

public fn fac(n: int) -> int {
    return n <= 1 ? 1 : n * fac(n - 1);
}

public fn callback(n: int) {
    X.print("Callback\n");
    X.print(n);
    X.print(fac(n));
}

public fn allocate(bytes: int) -> &mut [byte] {
    return &mut __builtin_alloc(bytes, 8);
}

public fn deallocate(bytes: &mut [byte]) {
    __builtin_dealloc(&mut bytes, 8);
}
