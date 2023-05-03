
func i32 @main({ i32, i32 } %0) {
  %entry:
    %1 = extract_value { i32, i32 } %0, 1
    return i32 %1
}
