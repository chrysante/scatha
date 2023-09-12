
fn sum(data: &[int]) -> int {
    var acc = 0;
    for i = 0; i < data.count; ++i {
        acc += data[i];
    }
    return acc;
}

struct IntVector {
    fn init(&mut this, cap: int) {
        let data = &mut __builtin_alloc(8 * cap, 8);
        this.numElems = 0;
        this.mData = reinterpret<&mut [int]>(&mut data);
    }

    fn deinit(&mut this) {
        let data = &mut reinterpret<&mut [byte]>(&this.mData);
        __builtin_dealloc(&mut data, 8);
    }

    fn push_back(&mut this, value: int) {
        this.mData[this.numElems] = value;
        ++this.numElems;
    }

    fn at(&mut this, index: int) -> &mut int {
        return &mut this.mData[index];
    }

    fn at(&this, index: int) -> &int {
        return &this.mData[index];
    }

    fn data(&mut this) -> &mut [int] {
        return &mut this.mData;
    }

    fn data(&this) -> &[int] {
        return &this.mData;
    }

    fn count(&this) -> int {
        return this.numElems;
    }

    fn capacity(&this) -> int {
        return this.mData.count;
    }

    var numElems: int;
    var mData: &mut [int];
}

fn main() -> int {
    var v: IntVector;
    v.init(2);
    v.push_back(1);
    v.at(0) = 15;
    v.at(1) = 7;
    let result = v.data.sum();
    v.deinit();
    return result;
}
