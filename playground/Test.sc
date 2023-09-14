

#//struct X {
#//    var y: Y;
#//    var i: int;
#//}
#//
#//struct Y {
#//    var j: s32;
#//    var k: s32;
#//}
#//
#//fn main(b: bool) -> int {
#//    var x: X;
#//    x.y.j = 1;
#//    x.y.k = 2;
#//    x.i = 3;
#//    var y: X;
#//    y.y.j = 4;
#//    y.y.k = 5;
#//    y.i = 6;
#//    var z: &X = b ? x : y;
#//    let w = x.y;
#//    let q = x;
#//    return z.y.j + w.k;
#//}

# phi pointers in a loop

#func i32 @main(i1 %0, i1 %1) {
#%entry:
#    %data = alloca i32, i32 3
#    %at0 = getelementptr inbounds i32, ptr %data, i32 0
#    store ptr %at0, i32 1
#    %at1 = getelementptr inbounds i32, ptr %data, i32 1
#    store ptr %at1, i32 2
#    %at2 = getelementptr inbounds i32, ptr %data, i32 2
#    store ptr %at2, i32 3
#    goto label %header
#
#%header:
#    %elem.ptr = phi ptr [label %entry: %data], [label %body: %next]
#    %sum = phi i32 [label %entry: 0], [label %body: %added]
#    %i = phi i32 [label %entry: 0], [label %body: %i.0]
#    %end.ptr = getelementptr inbounds i32, ptr %data, i32 3
#    %cond = ucmp neq i32 %i, i32 3
#    branch i1 %cond, label %body, label %end
#
#%body:
#    %elem = load i32, ptr %elem.ptr
#    %added = add i32 %sum, i32 %elem
#    %next = getelementptr inbounds i32, ptr %elem.ptr, i32 1
#    %i.0 = add i32 %i, i32 1
#    goto label %header
#
#%end:
#    return i32 %sum
#}

# phi pointers transitively

#func i32 @main(i1 %0, i1 %1) {
#%entry:
#    %data1 = alloca i32
#    %data2 = alloca i32
#    %array = alloca i32, i32 2
#    store ptr %data1, i32 1
#    store ptr %data2, i32 2
#    %data3 = getelementptr inbounds i32, ptr %array, i32 1
#    store ptr %data3, i32 3
#    branch i1 %0, label %then, label %end
#
#%then:
#    goto label %end
#
#%end:
#    %data.phi = phi ptr [label %entry: %data2], [label %then: %data1]
#    branch i1 %1, label %then.1, label %end.1
#
#%then.1:
#    goto label %end.1
#
#%end.1:
#    %data.phi.1 = phi ptr [label %end: %data3], [label %then.1: %data.phi]
#    %a = load i32, ptr %data.phi.1
#    store ptr %data.phi.1, i32 3
#    %gep = getelementptr inbounds i16, ptr %data.phi.1, i32 1
#    %b = load i16, ptr %gep
#    %c = zext i16 %b to i32
#    %ret = add i32 %a, i32 %c
#    return i32 %ret
#}



func i32 @main() {
%entry:
    %data = alloca i64, i32 2
    store ptr %data, i64 0
    %data.1 = getelementptr inbounds i64, ptr %data, i32 1
    store ptr %data.1, i64 1
    %struct = load { i64, i32, i32 }, ptr %data
    %ret = extract_value { i64, i32, i32 } %struct, 1
    return i32 %ret
}

