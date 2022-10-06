

fn main() -> int {
	var x: X;
	let result = f(x.y);
	return result;
}

fn f(y: Y) -> int {
	return y.x;
}

struct X {
	var i: int;
	var j: int;
	var y: Y;
}

struct Y {
	var x: int;
	var y: int;
}


