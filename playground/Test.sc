


fn toInline() -> int {
    var data: [int, (1 << 13) - 1];
    var data2 = [1];
    __builtin_memcpy(reinterpret<*mut [byte]>(&mut data[0 : 1]),
                     reinterpret<*mut [byte]>(&mut data2));
    return data2[0];
}


fn main() -> int {
    var result = 0;
    for i = 0; i < 10; ++i {
        result += toInline();
    }
    return result;
}
