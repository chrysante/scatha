func void @f(i1 %0) {
  %entry:
    branch i1 %0, label %then, label %else

  %then:
    goto label %header

  %else:
    goto label %header

  %header:
    %i.2 = phi i64 [label %then : 0], [label %else : 0], [label %body : %++.result]
    %cmp.result = scmp ls i64 %i.2, i64 10
    branch i1 %cmp.result, label %body, label %end

  %body:
    %++.result = add i64 %i.2, i64 1
    goto label %header

  %end:
    return
}
