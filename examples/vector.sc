
fn sum(data: &[int]) -> int {
    var acc = 0;
    for i = 0; i < data.count; ++i {
        acc += data[i];
    }
    return acc;
}

struct IntVector {
    fn init(&mut this, cap: int) {
        let data = &__builtin_alloc(8 * cap, 8);
        this.numElems = 0;
        this.data = reinterpret<&[int]>(&data);
    }

    fn deinit(&mut this) {
        let data = &reinterpret<&[byte]>(&this.data);
        __builtin_dealloc(&data, 8);
    }

    fn push_back(&mut this, value: int) {
        this.data[this.numElems] = value;
        ++this.numElems;
    }

    fn at(&mut this, index: int) -> &mut int {
        return &this.data[index];
    }

    fn count(&this) -> int {
        return this.numElems;
    }

    fn capacity(&this) -> int {
        return this.data.count;
    }

    var numElems: int;
    var data: &[int];
}

public fn main() -> int {
    let v: IntVector;
    v.init(100);
    v.push_back(1);
    let result = v.at(0);
    v.deinit();
    return result;
}
