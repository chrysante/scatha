
fn add(f: float) -> int { return 1; }

fn add(x: int) -> int {
	return x;
}

fn add(x: int, y: int) -> int {
	return x + y;
}

fn add(x: int, y: int, z: int) -> int {
	return x + y + z;
}

fn main() -> int {
	return add(1.0) * add(1) + add(2, 3) + add(4, 5, 6);
}







/*

fn main() -> Y {
	let x: int = "hello";
	let x: int = "hello";
	let y: X = 0;
	return "1";
}

struct Y{
	
}

*/

/*

struct X {
	fn f(x: int) -> int { return x; }
}


fn main(x: X) -> int {
	let x = 0;
	let y: int = "hello world";
	return x.f(o);
}

*/

/*

struct Y { var data: int; }

fn f() -> Y {
	let y: Y;
	return y;
}

fn main() -> int {
	return f().data;
}

*/
