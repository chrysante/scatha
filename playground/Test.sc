


fn gcd(a: int, b: int) -> int {
	if (b == 0) {
		return a;
	}
	return gcd(b, a % b);
}

fn main() {
	let a = 39;
	let b = 13;
	
	let result = gcd(a, b);
	
}
