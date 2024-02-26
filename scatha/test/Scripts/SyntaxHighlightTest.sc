

// This is a comment
fn main(var i: int) -> int {
	var x: X;
	let	b = i >= 0 || i < -10;
	if (b) {
		x.data = 2;		
		x.data *= 3;		
		x.data <<= 2;		
		
	}
	else {
		x = computeX(i);
	}
	let s: string = "if else for while";
	let y: float = 1.0;
	return 0;
}

struct X {
	var data: int;
}
