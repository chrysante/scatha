
struct @X {
  i64,
  i64
}

func i64 @main() {
  %entry:
    %q = alloca @X
    %p = getelementptr inbounds @X, ptr %q, i64 0, 0
    store ptr %p, i64 1
    %i = load i64, ptr %p
    goto label %loopheader

  %loopheader:                # preds: entry
    %cond = cmp leq i64 1, i64 undef
    goto label %loopbody

  %loopbody:                  # preds: loopheader, loopbody
    %x = phi ptr [label %loopheader : %p], [label %loopbody : undef]
    branch i1 1, label %exit, label %loopbody

  %exit:                      # preds: loopbody
    %res = call i64 @get_value, ptr undef, ptr undef, i1 1
    %s = select i1 1, f64 undef, f64 undef
    return i64 1
}

func i64 @get_value(ptr %0, ptr %1, i1 %2) {
  %entry:
    branch i1 %2, label %then, label %else

  %then:                      # preds: entry
    %cond = lnt i1 %2
    %res.0 = call i64 @get_value, ptr %1, ptr %0, i1 %cond
    goto label %ret

  %else:                      # preds: entry
    goto label %ret

  %ret:                       # preds: then, else
    %res = phi i64 [label %then : %res.0], [label %else : 1]
    return i64 %res
}
