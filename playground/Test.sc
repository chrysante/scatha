
fn print(msg: &[byte]) {
    __builtin_putstr(&msg);
}

fn print(val: byte) {
    __builtin_putchar(val);
}

public fn main() {
    let s = "123ABC";
    var data: [byte, 6];
    
    data[0] = s[0];
    data[1] = s[1];
    data[2] = s[2];
    data[3] = s[3];
    data[4] = s[4];
    data[5] = s[5];
    
    data[1] = '7';
    print(&data);
    print('\n');
}
