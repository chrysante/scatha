
# SROA2

# Test case that fails for reverse BFS phi rewrite
func i64 @test(i1 %0, ptr %1) {
%entry:
    %local = alloca i64, i32 1
    branch i1 %0, label %then, label %else
    
%then:
    goto label %if.end

%else:
    goto label %if.end

%if.end:
    %p = phi ptr [label %then: %1], [label %else: %local]
    store ptr %p, i64 1
    goto label %end

%end:
    %ret = load i64, ptr %p
    return i64 %ret
}


# phi pointers in a loop
func i32 @main(i1 %0, i1 %1) {
%entry:
    %data = alloca i32, i32 3
    %at0 = getelementptr inbounds i32, ptr %data, i32 0
    store ptr %at0, i32 1
    %at1 = getelementptr inbounds i32, ptr %data, i32 1
    store ptr %at1, i32 2
    %at2 = getelementptr inbounds i32, ptr %data, i32 2
    store ptr %at2, i32 3
    goto label %header

%header:
    %elem.ptr = phi ptr [label %entry: %data], [label %body: %next]
    %sum = phi i32 [label %entry: 0], [label %body: %added]
    %i = phi i32 [label %entry: 0], [label %body: %i.0]
    %end.ptr = getelementptr inbounds i32, ptr %data, i32 3
    %cond = ucmp neq i32 %i, i32 3
    branch i1 %cond, label %body, label %end

%body:
    %elem = load i32, ptr %elem.ptr
    %added = add i32 %sum, i32 %elem
    %next = getelementptr inbounds i32, ptr %elem.ptr, i32 1
    %i.0 = add i32 %i, i32 1
    goto label %header

%end:
    return i32 %sum
}

# phi pointers transitively
func i32 @main(i1 %0, i1 %1) {
%entry:
    %data1 = alloca i32
    %data2 = alloca i32
    %array = alloca i32, i32 2
    store ptr %data1, i32 1
    store ptr %data2, i32 2
    %data3 = getelementptr inbounds i32, ptr %array, i32 1
    store ptr %data3, i32 3
    branch i1 %0, label %then, label %end

%then:
    goto label %end

%end:
    %data.phi = phi ptr [label %entry: %data2], [label %then: %data1]
    branch i1 %1, label %then.1, label %end.1

%then.1:
    goto label %end.1

%end.1:
    %data.phi.1 = phi ptr [label %end: %data3], [label %then.1: %data.phi]
    %a = load i32, ptr %data.phi.1
    store ptr %data.phi.1, i32 3
    %gep = getelementptr inbounds i16, ptr %data.phi.1, i32 1
    %b = load i16, ptr %gep
    %c = zext i16 %b to i32
    %ret = add i32 %a, i32 %c
    return i32 %ret
}
