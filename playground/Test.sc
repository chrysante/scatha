
func void @set_var(ptr) {
  %entry:
    %p = alloca i64
    store ptr %p, i64 1
    %i = load i64, ptr %p
    goto label %loopheader
    
  %loopheader:
    %cond = cmp leq i64 1, i64 undef
    goto label %loopbody
    
  %loopbody:
    %x = phi ptr [label %loopheader: %0], [label %loopbody: undef]
    branch i1 1, label %exit, label %loopbody
    
  %exit:
   %res = call i64 @get_value, ptr undef, ptr undef
   return
}

func i64 @get_value(ptr, ptr) {
  %entry:
    %res = call i64 @get_value, ptr %1, ptr %0
    return i64 %res
}

