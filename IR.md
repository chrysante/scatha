
# Source Code:

    fn fac(n: int) -> int {
        var result = 1;
        var i = 0;
        while i < n {
            i += 1;
            result *= i;
        }
        return result;
    }

# IR: 

    define @fac(%n: i64) -> i64 {
      begin:
        %result.0 = i64 1;
        %i.0 = i64 0;
        branch label %loop_header;

      loop_header: 
        %i.1 = phi i64 [%begin, %i.0], [%loop_body, %i.2];
        %result.1 = phi i64 [%begin, %result.0], [%loop_body, %result.2];
        %0 = icmp ls i64 %i.1, %n;
        branch i1 %0 label %loop_body, label %loop_end;
        
      loop_body:
        %i.2 = iadd i64 %i.1, 1;
        %result.2: i64 = imul i64 %result.1, %i.2;
        branch label %loop_header;

      loop_end:
        ret i64 %result.1;
    }