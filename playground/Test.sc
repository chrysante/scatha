
@const_data = [i32, 3] [i32 1, i32 2, i32 3]

func i32 @main() {
  %entry:
    %p = getelementptr inbounds i32, ptr @const_data, i32 1
    %r = load i32, ptr %p
    return i32 %r
}
