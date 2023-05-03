
fn print(n: int) {
    __builtin_puti64(n);
    newLine();
}

fn newLine() {
    __builtin_putchar(10);
}

fn hash(n: int) -> int {
    return n;
}

/// -1 means empty and -2 means tombstone

fn init(table: &mut [int]) {
    for i = 0; i < table.count; ++i {
        table[i] = -1;
    }
}

fn max(a: int, b: int) -> int {
    return a > b ? a : b;
}

fn store(table: &mut [int], n: int) -> bool {
    if n < 0 {
        return false;
    }
    let h = hash(n);
    let end = max(table.count, 256);
    for i = 0; i < end; ++i, ++h {
        h  %= table.count;
        let entry = table[h];
        /// We store
        if entry < 0 {
            table[h] = n;
            return true;
        }
        /// n is already in the table
        if entry == n {
            return false;
        }
    }
    return false;
}

fn erase(table: &mut [int], n: int) -> bool {
    if n < 0 {
        return false;
    }
    let h = hash(n);
    let end = max(table.count, 256);
    for i = 0; i < end; ++i, ++h {
        h  %= table.count;
        let entry = &table[h];
        if entry == n {
            entry = -1;
            return true;
        }
        if entry == -1 {
            return false;
        }
    }
    return false;
}

fn contains(table: &[int], n: int) -> bool {
    if n < 0 {
        return false;
    }
    let h = hash(n);
    for i = 0; true; ++i {
        let entry = table[h];
        if entry == -1 {
            return false;
        }
        if entry == n {
            return true;
        }
        if i == 256 {
            return false;
        }
        h = (h + 1) % table.count;
    }
}

fn print(table: &[int]) {
    newLine();
    for i = 0; i < table.count; ++i {
        if table[i] >= 0 {
            print(table[i]);
        }
    }
    newLine();
}

fn printRaw(table: &[int]) {
    for i = 0; i < table.count; ++i {
        print(table[i]);
    }
}

public fn main() -> bool {
    var table: [int, 10];
    
    table.init();
    
    var result = true;
    
    result &= table.store(5);
    result &= table.store(15);
    result &= table.store(7);
    result &= table.store(0);
    
    table.print();
    
    table.erase(15);
    
    table.print();
    
    table.store(15);
    
    table.print();
    
    result &= table.store(13);
    result &= table.store(31);
    result &= table.store(11);

    table.print();
    
    result &= table.contains(5);
    result &= table.contains(7);
    result &= table.contains(0);
    result &= !table.contains(9);
    result &= !table.contains(8);
    result &= !table.contains(100);

    return result;
}
