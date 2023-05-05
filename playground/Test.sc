

ext func void @__builtin_memcpy(ptr, ptr, i64)

@data = [i32, 3] [i32 1, i32 2, i32 3]

func i32 @main() {
  %entry:
    %data = alloca i32, i32 3
    call void @__builtin_memcpy, ptr %data, ptr @data, i64 12
    goto label %header
    
  %header:
    %i = phi i32 [label %entry : 0], [label %body : %i.1]
    %s = phi i32 [label %entry : 0], [label %body : %s.1]
    %c = ucmp ls i32 %i, i32 3
    branch i1 %c, label %body, label %end
    
  %body:
    %i.1 = add i32 %i, i32 1
    %p = getelementptr inbounds i32, ptr %data, i32 %i
    %elem = load i32, ptr %p
    %s.1 = add i32 %s, i32 %elem
    goto label %header
  
  %end:
    return i32 %s
}








# @const_data = [i32, 3] [i32 1, i32 2, i32 3]
#
# @other_data = i32 1
#
# func i32 @main() {
#   %entry:
#     %1 = load i32, ptr @other_data
#
#     %p = getelementptr inbounds i32, ptr @const_data, i32 0
#     %t0 = load i32, ptr %p
#
#     %q = getelementptr inbounds i32, ptr @const_data, i32 1
#     %t1 = load i32, ptr %q
#
#     %r = getelementptr inbounds i32, ptr @const_data, i32 2
#     %t2 = load i32, ptr %r
#
#     %s0 = add i32 %t0, i32 %t1
#     %s1 = add i32 %s0, i32 %t2
#
#     %s2 = add i32 %s1, i32 %1
#     return i32 %s2
# }
