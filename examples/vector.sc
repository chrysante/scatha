
fn sum(data: &[int]) {
    var acc = 0;
    for i = 0; i < data.count; ++i {
        acc += data[i];
    }
    return acc;
}

struct IntVector {
    fn new(&mut this, cap: int) {
        let data = __builtin_alloc(8 * cap, 8);
        this.numElems = 0;
        this.mData = reinterpret<*mut [int]>(data);
    }

    fn delete(&mut this) {
        let data = reinterpret<*mut [byte]>(this.mData);
        __builtin_dealloc(data, 8);
    }

    fn push_back(&mut this, value: int) {
        this.mData[this.numElems] = value;
        ++this.numElems;
    }

    fn at(&mut this, index: int) -> &mut int {
        return this.mData[index];
    }

    fn at(&this, index: int) -> &int {
        return this.mData[index];
    }

    fn data(&mut this) -> &mut [int] {
        return *this.mData;
    }

    fn data(&this) -> &[int] {
        return *this.mData;
    }

    fn count(&this) {
        return this.numElems;
    }

    fn capacity(&this) {
        return this.mData.count;
    }

    var numElems: int;
    var mData: *mut [int];
}

fn main() {
    var v = IntVector(2);
    v.push_back(1);
    v.at(0) = 15;
    v.at(1) = 7;
    let result = sum(v.data());
    __builtin_puti64(result);
    __builtin_putstr("\n");
}
