

public struct String {
    fn new(&mut this) {}

    fn new(&mut this, text: &str) { 
        this.buf = unique str(text); 
        this.sz = text.count;
    }
    
    fn clear(&mut this) {
        this.buf = null;
        this.sz = 0;
    }

    fn count(&this) { return this.sz; }

    fn data(&this) -> &str { return this.buf[0 : this.sz]; }
    fn data(&mut this) -> &mut str { return this.buf[0 : this.sz]; }

    private var buf: *unique str;
    private var sz: int;
}