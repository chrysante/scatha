
fn print(msg: &[byte]) {
    __builtin_putstr(&msg);
    __builtin_putchar(10);
}

fn f(n: u64, m: s32) {
    print("one");
}

fn f(n: u32, m: s64) {
    print("two");
}

fn f(n: u16) {
    print("three");
}

public fn main() {
    
    let u = u16(1);
    
    f(u, 1);
    
    

    
}
