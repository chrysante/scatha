


struct @X {
    i64, i64, i64
}

func i64 @main(i1 %cond) {
  %entry:
    %array = alloca @X, i32 3
    %x.1 = getelementptr inbounds @X, ptr %array, i32 1
    %x.1.value = load @X, ptr %x.1
    call void @takeX, @X %x.1.value
    %x.1.1 = getelementptr inbounds @X, ptr %x.1, i32 0, 1
    store ptr %x.1.1, i64 5
    %x.0.2 = getelementptr inbounds @X, ptr %array, i32 0, 2
    store ptr %x.0.2, i64 3
    goto label %end
    
  %end:
    %y.1.1 = getelementptr inbounds @X, ptr %x.1, i32 0, 1
    %a = load i64, ptr %y.1.1
    %y.0.2 = getelementptr inbounds @X, ptr %array, i32 0, 2
    %b = load i64, ptr %y.0.2
    %res = add i64 %a, i64 %b
    return i64 %b
}

func void @takeX(@X) {
  %entry:
    return
}
