
/// Mutable, owning string class
public struct String {
    /// Lifetime functions
    
    fn new(&mut this) {}

    fn new(&mut this, text: &str) { 
        this.buf = unique str(text); 
        this.sz = text.count;
    }

    fn new(&mut this, rhs: &String) {
        this.buf = unique str(rhs.sz);
        __builtin_memcpy(this.buf, &rhs.buf[0 : rhs.sz]);
        this.sz = rhs.sz;
    }

    fn move(&mut this, rhs: &mut String) {
        this.buf = move rhs.buf;
        this.sz = rhs.sz;
        rhs.sz = 0;
    }
    
    /// Modifiers
    
    fn clear(&mut this) {
        this.sz = 0;
    }

    fn append(&mut this, char: byte) {
        if this.buf.count == this.sz {
            this.grow();
        }
        this.buf[this.sz++] = char;
    }

    /// Queries

    fn count(&this) { return this.sz; }

    fn empty(&this) { return this.sz == 0; }

    fn data(&this) -> *str { return &this.buf[0 : this.sz]; }

    fn data(&mut this) -> *mut str { return &mut this.buf[0 : this.sz]; }

    /// Data

    private fn grow(&mut this) {
        var tmp = unique str(2 * this.buf.count);
        __builtin_memcpy(&mut tmp[0 : this.buf.count], this.buf);
        this.buf = move tmp;
    }

    private var buf: *unique mut str;
    private var sz: int;
}
