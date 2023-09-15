

func i32 @main() {
%entry:
    %local = alloca i32, i32 1
    store ptr %local, i32 1
    branch i1 1, label %then, label %else
    
%then:
    %p = phi ptr [label %entry: %local]
    goto label %cond.end

%else:
    goto label %cond.end

%cond.end:
    %q = phi ptr [label %then: %p], [label %else: %local]
    %res = load i32, ptr %q
    return i32 %res
}
