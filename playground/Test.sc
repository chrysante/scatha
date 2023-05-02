

// func [i32, 6] @main() {
//   %entry:
//     %a = alloca [i32, 6]
//
//     %p = getelementptr inbounds i32, ptr %a, i32 0
//     store ptr %p, i32 0
//     %q = getelementptr inbounds i32, ptr %a, i32 1
//     store ptr %q, i32 1
//
//
//     %r = load [i32, 6], ptr %a
//     return [i32, 6] %r
// }


struct vector {
    fn size(&this) -> int { return this.__size; }
    
    fn resize(&mut this, new_size: int) { this.__size = new_size; }
    
    var __size: int;
}

fn test() {
    var i = 10 - 20;
    while true {
        ++i;
        print(i);
        if i == 5 {
            break;
        }
    }
}

fn print(n: int) {
    __builtin_puti64(n);
    __builtin_putchar(10);
}

fn makeInt() -> int {
    return 5;
}

public fn main() -> int {
    var v: vector;
    v.resize(10);
    print(v.size());

    test();
    let arr = [1, 2, 3, 4, makeInt()];
    return sum(&arr);
}

fn sum(data: &[int]) -> int {
    var acc = 0;
    for i = 0; i < data.count; ++i {
        acc += data[i];
    }
    return acc;
}

