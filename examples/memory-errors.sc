
fn print(text: &str) {
    __builtin_putstr(text);
}

fn main(args: &[*str]) {
    if args.count < 1 {
        print("Expecting at least one argument\n");
        return 1;  
    }
    var caseID: int;
    if !__builtin_strtos64(caseID, *args.front, /* base = */ 10) {
        print("Expecting an integer as first argument\n");
        return 2;
    }
    if caseID == 0 {
        print("Testing bad memory access\n");
        let count = 10000;
        let data = __builtin_alloc(count, 1);
        data[2 * count] = 0;
        return 0;
    }
    if caseID == 1 {
        print("Testing bad memory allocation\n");
        let data = __builtin_alloc(12, 3);
        return 0;
    }
    if caseID == 2 {
        print("Testing bad memory deallocation\n");
        let count = 16;
        let data = __builtin_alloc(count, 1);
        let ptr = &mut data[count / 2 : count];
        __builtin_dealloc(ptr, 1);
        return 0;
    }
    if caseID == 3 {
        print("Testing trap instruction\n");
        __builtin_trap();
        return 0;
    }
    print("Unexpected case ID\n");
    return 3;
}
