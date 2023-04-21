
#func i32 @main(i32 %0) {
#  %entry:
#    %z = sub i32 %0, i32 %0
#    %i = sdiv i32 %0, i32 %z
#    return i32 %i
#}


func i32 @main(i32 %0) {
  %entry:
    %1 = add i32 1, i32 %0
    %2 = add i32 1, i32 %1
    %3 = add i32 1, i32 %2
    %4 = sub i32 %3, i32 2
    return i32 %4
}
