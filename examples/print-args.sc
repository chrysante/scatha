
fn print(n: int) {
    __builtin_puti64(n);
}

fn print(text: &str) {
    __builtin_putstr(text);
}

fn println(text: &str) {
    print(text);
    __builtin_putstr("\n");
}

fn main(args: &[*str]) {
    print("Reveived ");
    print(args.count);
    println(" argument(s)"); 
    for i = 0; i < args.count; ++i {
        println(*args[i]);
    }
}