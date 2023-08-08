fn gcd(a: int, b: int) -> int {
    while b != 0 && true {
        let t = b + 0;
        b = a % b;
        a = t;
    }
    return a;
}
public fn main() -> int {
    let a = 756476;
    let b = 1253;
    return gcd(a, b);
}


/*

fn print(n: int) {
    __builtin_puti64(n);
    __builtin_putchar('\n');
}

fn print(n: double) {
    __builtin_putf64(n);
    __builtin_putchar('\n');
}

fn print(s: &str) {
    __builtin_putstr(&s);
}

public fn test(n: int) {
    
    for i = 0; i < 10; ++i {
        print(i);
    }
}

public fn test(n: int) {
    
    var i = 0;
    do {
        print(i);
        ++i;
    } while i < 10;
}

public fn main(cond: bool) {
    var i = 0;
    if cond {
        print("cond\n");
    }
    else {
        print("not cond\n");
    }
    while i < 10  {
        for j = 0; j < 5; ++j {
            __builtin_putchar(' ');
        }
        ++i;
        if i % 3 != 0 {
            continue;
        }
        print(i);
    }
}
*/
