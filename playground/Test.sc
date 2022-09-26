
fn greaterZero(a: int) -> bool {
	return a > 0;
	
}

fn main() -> int {
	let x = 0;
	let y = 1;
	if greaterZero(x) {
		return 2;
	}
	else if greaterZero(y) {
		return 1;
	}
	else {
		return 0;
	}
}
