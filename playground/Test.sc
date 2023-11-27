



export fn test(a: int, data:  *mut [byte]) {
    __builtin_putstr("Hello ");
    let localData = ['1', '2', '3'];
    __builtin_putstr("World ");
    let n = 2 * a;
    __builtin_memcpy(data, &localData);
    if n == 0 {
        return n / 2;
    }
    return n;
}
