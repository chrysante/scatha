
fn printChar(n: byte) {
    __builtin_putchar(n);
}

fn print(n: int) {
    __builtin_puti64(n);
    printChar('\n');
}

struct Matrix {
    fn new(&mut this) {
        for i = 0; i < 9; ++i {
            this.data[i] = 0;
        }
    }

    fn new(&mut this, data: [int, 9]) {
        this.data = data;
    }

    fn at(&mut this, i: int, j: int) -> &mut int {
        return this.data[i * 3 + j];
    }

    fn at(&this, i: int, j: int) -> &int {
        return this.data[i * 3 + j];
    }

    fn mul(&mut this, B: &Matrix) -> void {
        var C = Matrix();
        for i = 0; i < 3; ++i {
            for k = 0; k < 3; ++k {
                for j = 0; j < 3; ++j {
                    C.at(i, k) += this.at(i, j) * B.at(j, k);
                }
            }    
        }
        for i = 0; i < this.data.count; ++i {
            this.data[i] = C.data[i];
        }
    }

    fn det(&this) -> int {
        return this.at(0, 0) * (this.at(1, 1) * this.at(2, 2) - this.at(2, 1) * this.at(1, 2)) -
               this.at(0, 1) * (this.at(1, 0) * this.at(2, 2) - this.at(1, 2) * this.at(2, 0)) +
               this.at(0, 2) * (this.at(1, 0) * this.at(2, 1) - this.at(1, 1) * this.at(2, 0));
    }

    fn print(&this) {
        for i = 0; i < 3; ++i {
            for j = 0; j < 3; ++j {
                __builtin_puti64(this.at(i, j));
                printChar(' ');
                printChar(' ');
            }
            printChar('\n');
        }
    }

    var data: [int, 9];
}

fn main() -> int {
    var A = Matrix([
        1,  0,  1, 
        0,  1,  0, 
        3,  0,  1 
    ]);

    let B = Matrix([
        1,  0,  0, 
        0,  2,  0, 
        0,  0,  1
    ]);

    A.mul(B);
    A.print();
    print(A.det());
    return 0;
}

