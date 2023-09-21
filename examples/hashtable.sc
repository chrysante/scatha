
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
    var h = hash(n);
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
    var h = hash(n);
    let end = max(table.count, 256);
    for i = 0; i < end; ++i, ++h {
        h  %= table.count;
        let entry: &mut int = table[h];
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
    var h = hash(n);
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
    __builtin_putstr("Table contains:");
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

fn main() -> bool {
    var table: [int, 10];
    
    init(table);
    
    var result = true;
    
    result &= store(table, 5);
    result &= store(table, 15);
    result &= store(table, 7);
    result &= store(table, 0);
    
    print(table);
    erase(table, 15);
    print(table);
    store(table, 15);
    print(table);
    
    result &= store(table, 13);
    result &= store(table, 31);
    result &= store(table, 11);

    print(table);
    
    result &= contains(table, 5);
    result &= contains(table, 7);
    result &= contains(table, 0);
    result &= !contains(table, 9);
    result &= !contains(table, 8);
    result &= !contains(table, 100);

    return result;
}
