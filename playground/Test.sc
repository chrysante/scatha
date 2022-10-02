
struct Y { var i: int; }

fn f(x: int, y: Y) -> int {
	return 2 * x * y.i;
}
