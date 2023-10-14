
fn main() {
    X().print();
}

struct X {
    fn print(&this) {
        print("This is an X\n");
    }
}
