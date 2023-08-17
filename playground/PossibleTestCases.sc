
/// Keep this as a test case for array references but reevaluate the reference syntax before
public fn main() -> int {
    var data = &mut allocateInts(10);
    for i = 0; i < data.count; ++i {
        data[i] = i;
    }
    var sum = 0;
        for i = 0; i < data.count; ++i {
        sum += data[i];
    }
    deallocateInts(&mut data);
    return sum;
}

fn allocateInts(count: int) -> &mut [int] {
    var result = &mut __builtin_alloc(count * 8, 8);
    return reinterpret<&mut [int]>(&mut result);
}

fn deallocateInts(data: &mut [int]) {
    let bytes = reinterpret<&mut [byte]>(&mut data);
    __builtin_dealloc(&mut bytes, 8);
}

