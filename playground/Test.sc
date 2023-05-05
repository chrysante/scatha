


func i1 @main-bool(i1 %0) {
  %entry:
    %1 = or i1 %0, i1 1
    goto label %header

  %header:
    %i.1 = phi i64 [label %entry : 0], [label %body : %inc]
    %result = phi i64 [label %entry : 42], [label %body : %result]
    %cond = scmp ls i64 %i.1, i64 10
    branch i1 %cond, label %body, label %end

  %body:
    %inc = add i64 %i.1, i64 1
    goto label %header

  %end:
    %2 = scmp eq i64 %result, i64 42
    %3 = and i1 %1, i1 %2
    return i1 %3
}


