

struct @X {
    i64, i64
}

func i64 @main() {
  %entry:
    %a = alloca @X, i32 10
    %p = call ptr @populate, ptr %a
    %q = getelementptr inbounds @X, ptr %p, i32 0, 1
    %res = load i64, ptr %q
    return i64 %res
}

func ptr @populate(ptr %a) {
  %entry:
    %index = add i32 1, i32 2
    %p = getelementptr inbounds @X, ptr %a, i32 %index
    %x.0 = insert_value @X undef, i64 1, 0
    %x.1 = insert_value @X %x.0, i64 2, 1
    store ptr %p, @X %x.1
    return ptr %p
}




# func i64 @main() {
#   %entry:
#     %a = alloca i64, i32 5
#     call void @set, ptr %a, i32 2, i64 3
#     %r = call i64 @get, ptr %a, i32 2
#     return i64 %r
# }
#
# func i64 @get(ptr %data, i32 %index) {
#   %entry:
#     %e = getelementptr inbounds i64, ptr %data, i32 %index
#     %r = load i64, ptr %e
#     return i64 %r
# }
#
# func void @set(ptr %data, i32 %index, i64 %value) {
#   %entry:
#     %e = getelementptr inbounds i64, ptr %data, i32 %index
#     store ptr %e, i64 %value
#     return
# }

