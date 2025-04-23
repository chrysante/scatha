import std;

fn hash(n: int) -> int {
    return n;
}

fn max(a: int, b: int) -> int {
    return a > b ? a : b;
}

fn assert(cond: bool, message: *str) {
    if (!cond) { std.print("Assertion failed: \(message)"); __builtin_trap(); }
}

let Empty: int = -1;
let Tombstone: int = -2;

public struct Hashtable {
    fn store(&mut this, value: int) -> bool {
        if this.load_factor() >= 80 { 
            this.grow();
        }
        let result = store_impl(*this._data, value);
        if (result) { ++this._size; }
        return result;
    }

    private fn store_impl(table: &mut [int], value: int) -> bool {
        if value < 0 { 
            return false; 
        }
        var h = hash(value);
        for i = 0; i < table.count; ++i, ++h {
            h  %= table.count;
            let entry = table[h];
            if entry < 0 {
                // Found an empty slot
                table[h] = value;
                return true;
            }
            if entry == value {
                // value is already in the table
                return false;
            }
        }
        return false;
    }

    fn erase(&mut this, n: int) -> bool {
        if n < 0 {
            return false;
        }
        var h = hash(n);
        let table: &mut = *this._data;
        for i = 0; i < table.count; ++i, ++h {
            h %= table.count;
            let entry: &mut int = table[h];
            if entry == n {
                entry = Empty;
                --this._size;
                return true;
            }
            if entry == Empty {
                return false;
            }
        }
        return false;
    }

    fn contains(&this, n: int) -> bool {
        if n < 0 {
            return false;
        }
        let table: & = *this._data;
        var h = hash(n);
        for i = 0; i < table.count; ++i, ++h {
            h %= table.count;
            let entry = table[h];
            if entry == -1 {
                return false;
            }
            if entry == n {
                return true;
            }
            
        }
        return false;
    }

    fn size(&this) -> int { return this._size; }

    fn empty(&this) -> bool { return this.size() == 0; }

    fn print(&this) {
        let table: & = *this._data;
        __builtin_putstr("[");
        var first = true;
        for i = 0; i < table.count; ++i {
            if table[i] >= 0 {
                if !first { __builtin_putstr(", "); }
                first = false;
                __builtin_puti64(table[i]);
            }
        }
        __builtin_putln("]");
    }

    fn print_raw(&this) {
        let table: & = *this._data;
        for i = 0; i < table.count; ++i {
            std.print("\(table[i])");
        }
    }

    // Have to comment private here due to compiler bug
    /* private */ fn grow(&mut this) {
        let new_cap = max(2, 2 * this._data.count);
        var new_data = unique [int](new_cap);
        for i = 0; i < new_data.count; ++i {
            new_data[i] = Empty;
        }
        for i = 0; i < this._data.count; ++i {
            if this._data[i] >= 0 {
                let success = store_impl(*new_data, this._data[i]);
                assert(success, "Insertion must succeed while rehashing");
            }
        }
        this._data = move new_data;
    }

    fn load_factor(&this) -> int {
        if this._data.count == 0 { return 100; }
        return this._size * 100 / this._data.count;
    }

    private var _data: *unique mut [int];
    private var _size: int; 
}
