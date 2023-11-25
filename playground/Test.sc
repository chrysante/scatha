

export fn test(cond: bool) {
    var j = 1;
    if cond {
        while j < 10 {
            j *= 2;
        }
    }
    return j;
}
