struct Expr {
    var id: int;
    var lhs: int;
    var rhs: int;
}
fn eval(expr: Expr) -> int {
    // 0 == Literal
    if expr.id == 0 {
        return expr.lhs;
    }
    // 1 == Add
    if expr.id == 1 {
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
