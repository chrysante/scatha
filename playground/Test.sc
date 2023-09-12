fn main() -> int {
    var data = allocateInts(10);
    for i = 0; i < (*data).count; ++i {
        (*data)[i] = i;
    }
    var sum = 0;
        for i = 0; i < (*data).count; ++i {
        sum += (*data)[i];
    }
    deallocateInts(data);
    return sum;
}
fn allocateInts(count: int) -> *mut [int] {
    var result = __builtin_alloc(count * 8, 8);
    return reinterpret<*mut [int]>(result);
}
fn deallocateInts(data: *mut [int]) {
    let bytes = reinterpret<*mut [byte]>(data);
    __builtin_dealloc(bytes, 8);
}
