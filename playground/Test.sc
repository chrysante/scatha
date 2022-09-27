fn fact(n: int) -> int {
	var i = 0;
	var result = 1;
	while i < n {
		i += 1;
		result *= i;
	}
	return result;
}

fn main() -> int {
	return fact(4);
}
