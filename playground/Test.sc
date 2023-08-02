/*
struct Expr {
    var id: int;
    var lhs: int;
    var rhs: int;
}

fn eval(expr: Expr) -> int {
    if expr.id == 0 { // 0 == Literal
        return expr.lhs;
    }
    if expr.id == 1 { // 1 == Add
        return expr.lhs + expr.rhs;
    }
    // 2 == Sub
    expr.id = 1; // Add
    expr.rhs = -expr.rhs;
    return eval(expr);
}

public fn main(n: int) -> int {
    var expr: Expr;
    expr.id = 2;
    expr.lhs = 5;
    expr.rhs = 2;
    return eval(expr);
}
*/

fn ack(n: int, m: int) -> int {
    if n == 0 {
        return m + 1;
    }
    if m == 0 {
        return ack(n - 1, 1);
    }
    return ack(n - 1, ack(n, m - 1));
}

public fn main(n: int) -> int {
    return ack(3, 4);
}
