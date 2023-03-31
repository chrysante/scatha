
struct @X {
    i64, i64
}

func i64 @set_var(ptr) {
  %entry:
    %q = alloca @X
    %p = getelementptr inbounds @X, ptr %q, i64 0, 0
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
   %s = select i1 1, f64 undef, f64 undef
   return i64 1
}

func i64 @get_value(ptr, ptr) {
  %entry:
    %res = call i64 @get_value, ptr %1, ptr %0
    return i64 %res
}

