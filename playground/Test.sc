
struct @X {
    i64, i64
}

func i64 @main(@X %a, @X %b, i1 %cond) {
  %entry:
    branch i1 %cond, label %if, label %then

  %if:
    goto label %end

  %then:
     %b.1 = insert_value @X %b, i64 3, 1
     goto label %end

  %end:
    %0 = phi @X [label %if: %a], [label %then: %b.1]
    %1 = extract_value @X %0, 0
    %2 = extract_value @X %0, 1
    %3 = add i64 %1, i64 %2
    return i64 %3
}

