

fn main() -> int {
    var conds = [true, false, false];
    return test(conds);
}

fn test(conds: &[bool]) -> int {
    var result = 5;
    if conds[0] {
        result = 1;
    }
    else if conds[1] {
        result = 2;
    }
    else if conds[2] {
        result = 3;
    }
    else if conds[3] {
        result = 4;
    }
    else if conds[4] {
        result = 5;
    }
    else if conds[5] {
        result = 6;
    }
    return result;
}
    

        
    
    
        
    





//func i64 @main() {
//%entry:
//    %0 = scmp eq i32 0, i32 1
//    %1 = lnt i1 %0
//
//    %s = select i1 %0, i64 1, i64 2
//    %r = select i1 %1, i64 %s, i64 3
//    return i64 %r
//}

