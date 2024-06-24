
/// Mutable, owning string class
public struct String {
    /// Lifetime functions

    fn new(&mut this) {}

    fn new(&mut this, text: *str) { 
        this.buf = unique str(text.count); 
        __builtin_memcpy(this.buf as *mut, text);
        this.sz = text.count;
    }

    fn new(&mut this, text: mut *unique mut str) { 
        this.buf = move text;
        this.sz = this.buf.count;
    }

    fn new(&mut this, rhs: &String) {
        this.buf = unique str(rhs.sz);
        __builtin_memcpy(this.buf as *mut, &rhs.buf[0 : rhs.sz]);
        this.sz = rhs.sz;
    }

    fn move(&mut this, rhs: &mut String) {
        this.buf = move rhs.buf;
        this.sz = rhs.sz;
        rhs.sz = 0;
    }

    fn delete(&mut this) {} 

    /// # Modifiers

    /// ## Clear

    /// Sets this string to the empty string
    fn clear(&mut this) {
        this.sz = 0;
    }

    /// ## Append

    /// Appends a single character to the back 
    fn append(&mut this, char: byte) {
        if this.buf.count == this.sz {
            this.grow();
        }
        this.buf[this.sz++] = char;
    }

    /// Appends the string \p text to the back of this string
    fn append(&mut this, text: *str) {
        if this.buf.count <= this.sz + text.count {
            this.growLeast(this.sz + text.count);
        }
        for i = 0; i < text.count; ++i {
            this.buf[this.sz++] = text[i];
        }
    }

    /// \overload
    fn append(&mut this, text: *unique str) {
        this.append(text as *);
    }

    /// \overload
    fn append(&mut this, text: &String) {
        this.append(text.data());
    }

    /// # Insert

    fn insert(&mut this, atIndex: int, text: *str) {
        let newSize = this.sz + text.count;
        this.growLeast(newSize);
        this.sz = newSize;
        __builtin_memmove(&mut this.buf[atIndex + text.count : newSize],
                          &this.buf[atIndex : this.sz]);
        __builtin_memcpy(&mut this.buf[atIndex : atIndex + text.count],     
                         text);
    }

    /// # Queries

    /// \Returns the number of characters 
    fn count(&this) { return this.sz; }

    /// \Returns true if this string is empty
    fn empty(&this) { return this.sz == 0; }

    /// \Returns a raw pointer to the contained string
    fn data(&this) -> *str { return &this.buf[0 : this.sz]; }

    /// \overload 
    fn data(&mut this) -> *mut str { return &mut this.buf[0 : this.sz]; }

    /// # Internals

    /// Grows the maintained buffer by a factor of two
    internal fn grow(&mut this) {
        this.growLeast(2 * this.buf.count);
    }

    /// Grows the maintained buffer to at least \p leastSize 
    internal fn growLeast(&mut this, leastSize: mut int) {
        leastSize = max(leastSize, 2 * this.buf.count);
        if leastSize <= this.buf.count {
            return;
        }
        var tmp = unique str(leastSize);
        __builtin_memcpy(&mut tmp[0 : this.buf.count], this.buf as *);
        this.buf = move tmp;
    }

    internal fn max(n: int, m: int) -> int {
        return n < m ? m : n;
    }

    internal var buf: *unique mut str;
    internal var sz: int;
}

private fn printVar(name: *str, value: int) {
    __builtin_putstr(name);
    __builtin_putstr(": ");
    __builtin_puti64(value);
    __builtin_putstr("\n");
}

/// Debug helper
private fn printVar(name: *str, value: *str) {
    __builtin_putstr(name);
    __builtin_putstr(": ");
    __builtin_putstr(value);
    __builtin_putstr("\n");
}
