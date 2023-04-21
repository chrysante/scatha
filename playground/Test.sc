
struct @Y {
    i64,
    f64,
    i32
}

struct @X {
    i64,
    f64,
    @Y
}

func @X @main(@X %xbase, @Y %ybase) {
  %entry:
    %x.0 = extract_value @X %xbase, 0
    %x.1 = extract_value @X %xbase, 1
    %y.1 = extract_value @Y %ybase, 1
  
    %r.0 = insert_value @X undef, i64 %x.0, 0
    %r.1 = insert_value @X %r.0,  f64 %x.1, 1
    %r.2 = insert_value @X %r.1,  i64 1,    2, 0
    %r.3 = insert_value @X %r.2,  f64 %y.1, 2, 1
    
    return @X %r.3
}
