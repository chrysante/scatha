import testlib;

extern "C" fn myFunction(n: int, m: int) -> int;

fn main() {
    __builtin_puti64(myFunction(22, 20));
    __builtin_putstr("\n");
}