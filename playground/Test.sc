


func i64 @main() {
  %entry:
    %q = sdiv i32 100, i32 -5
    %r = srem i32 %q, i32 3
    %re = sext i32 %r to i64
    return i64 %re
}
