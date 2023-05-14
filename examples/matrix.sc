
fn print(n: byte) {
    __builtin_putchar(n);
}

fn print(n: int) {
    __builtin_puti64(n);
    print('\n');
}

fn at(A: &mut [int], i: int, j: int) -> &mut int {
    return &mut A[i * 3 + j];
}

fn mul(A: &mut [int], B: &[int]) -> void {
    var C: [int, 9];
    for i = 0; i < 9; ++i {
        C[i] = 0;
    }
    for i = 0; i < 3; ++i {
        for k = 0; k < 3; ++k {
            for j = 0; j < 3; ++j {
                C.at(i, k) += A.at(i, j) * B.at(j, k);
            }
        }    
    }
    for i = 0; i < A.count; ++i {
        A[i] = C[i];
    }
}

fn det2(A: &[int]) -> int {
    return A[0] * A[3] - A[2] * A[1];
}           


fn det3(A: &[int]) -> int {
    return A.at(0, 0) * (A.at(1, 1) * A.at(2, 2) - A.at(2, 1) * A.at(1, 2)) -
           A.at(0, 1) * (A.at(1, 0) * A.at(2, 2) - A.at(1, 2) * A.at(2, 0)) +
           A.at(0, 2) * (A.at(1, 0) * A.at(2, 1) - A.at(1, 1) * A.at(2, 0));
}

fn print(A: &[int]) {
    for i = 0; i < 3; ++i {
        for j = 0; j < 3; ++j {
            __builtin_puti64(A.at(i, j));
            print(' ');
            print(' ');
        }
        print('\n');
    }
}

public fn main() -> int {
    var A = [
        1,  0,  1, 
        0,  1,  0, 
        3,  0,  1 
    ];

    let B = [
        1,  0,  0, 
        0,  2,  0, 
        0,  0,  1
    ];

    A.mul(&B);

    A.print();

    print(det3(&A));

    return 0;
}

