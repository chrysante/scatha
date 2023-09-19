fn main() -> int {
    var i = 0;
    var j = 0;
    var r = &mut i;
    *r += 1;
    r = &mut j;
    *r += 1;
    return i + j;
}
