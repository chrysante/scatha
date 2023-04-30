




public fn main() -> int {
    let arr = [1, 2, 3, 4, 5];
    return sum(&arr, 5);
}

fn sum(data: &[int], size: int) -> int {
    var acc = 0;
    for i = 0; i < size; ++i {
        acc += data[i];
    }
    return acc;
}







//struct vector {
//    fn resize(&mut this, newSize: int) {
//        this.mSize = newSize;
//    }
//
//    fn size(&this) -> int { return this.mSize; }
//
//    var mSize: int;
//}
//
//public fn main() -> int {
//    var v: vector;
//
//    v.resize(10);
//
//    return v.size;
//}


