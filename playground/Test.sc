


func i64 @main() {
  %entry:
    %a = alloca i64, i32 5
    call void @set, ptr %a, i32 2, i64 3
    %r = call i64 @get, ptr %a, i32 2
    return i64 %r
}

func i64 @get(ptr %data, i32 %index) {
  %entry:
    %e = getelementptr inbounds i64, ptr %data, i32 %index
    %r = load i64, ptr %e
    return i64 %r
}

func void @set(ptr %data, i32 %index, i64 %value) {
  %entry:
    %e = getelementptr inbounds i64, ptr %data, i32 %index
    store ptr %e, i64 %value
    return
}
