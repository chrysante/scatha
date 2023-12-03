



export fn test(a: int, data:  *mut [byte], ints: *[int]) {
    __builtin_putstr("Hello ");
    let localData = [data[4], '2', '3'];
    __builtin_putstr("World ");
    let n = 2 * a;
    __builtin_memcpy(data, &localData);
    let index = ints[a];
    if (a + ints[a]) / index + ints[1] == 0 {
        return n / 2;
    }
    return n;
}
