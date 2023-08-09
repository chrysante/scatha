
struct @X {
    i32, i32
}

func i32 @main(i1 %cond, @X %x) {
  %entry:
    %A.0 = insert_value @X %x, i32 1, 0
    #%A.1 = insert_value @X %A.0, i32 2, 1
    %B.0 = insert_value @X undef, i32 3, 0
    %B.1 = insert_value @X %B.0, i32 4, 1
    branch i1 %cond, label %then, label %else

  %then:
    goto label %then.continue

  %then.continue:
      goto label %end

  %else:
    goto label %end

  %end:
    %C = phi @X [label %then.continue: %A.0], [label %else: %B.1]
    branch i1 %cond, label %then.1, label %else.1

  %then.1:
    goto label %end.1

  %else.1:
    branch i1 %cond, label %then.2, label %else.2

  %then.2:
    goto label %end.1

  %else.2:
    goto label %end.1

  %end.1:
    %result = extract_value @X %C, 1
    return i32 %result
}
