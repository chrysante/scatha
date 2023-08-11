

func f64 @main(f64 %arg) {
%entry:
    %arg.0 = fsub f64 %arg, f64 0.500000
    %arg.1 = fdiv f64 %arg.0, f64 5.0
    %arg.2 = fdiv f64 %arg.1, f64 2.0
    return f64 %arg.2
}


