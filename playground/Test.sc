

function i64 @f(i64) {
  %entry:
    %cond = cmp leq i64 %0, i64 $1
    branch i1 %cond, label %then, label %else
  
  %then:
    return i64 $1

  %else:
    %sub.res = sub i64 %0, $1
    %call.res = call i64 @f, i64 %sub.res
    %mul.res = mul i64 %0, %call.res
    return i64 %mul.res
}

function i64 @main() {
  %entry:
    %call.res = call i64 @f, i64 $5
    return i64 %call.res
}
