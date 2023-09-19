
fn green() {
	print("\e[1;32m");
}

fn reset() {
	print("\e[0m");	
}

fn print(n: int) {
	__builtin_puti64(n);
}

fn print(text: &str) {
	__builtin_putstr(text);
}

fn sqrt(value: double) -> double {
	return __builtin_sqrt_f64(value);
}

fn set(data: &mut [bool], value: bool) {
	for i = 0; i < data.count; ++i {
		data[i] = value;
	}
}

struct Sieve {
	fn new(&mut this, size: int) {
		this.flags = reinterpret<*mut [bool]>(__builtin_alloc(size, 1));
	}
	
	fn delete(&mut this) {
		__builtin_dealloc(reinterpret<*mut [byte]>(this.flags), 1);
	}

	fn setFalse(&mut this, index: int, stride: int) {
		for i = index; i < this.size; i += stride {
			(*this.flags)[i] = false;
		}
	}

	fn isSet(&this, index: int) -> bool {
		return (*this.flags)[index];
	}

	fn size(&this) -> int {
		return this.flags->count;
	}

	fn run(&mut this) {
		set(*this.flags, true);
		var factor = 3;
        var q = int(sqrt(double(this.size())));	
        while factor <= q {
            for num = factor; num < this.size; num += 2 {
                if this.isSet(num) {
                    factor = num;
                    break;
                }
            }
            this.setFalse(factor * factor, 2 * factor);
            factor += 2;
        }
	}

	fn printPrime(p: int) {
		return;
		green();
		print(p);
		print(" is prime!\n");
		reset();
	}

	fn report(&this) {
		print("Sieve of size: ");
		print(this.size());
		print("\n");
		printPrime(2);
		var numPrimes = 1;
		for i = 3; i < this.size(); i+=2 {
			if this.isSet(i) {
				++numPrimes;
				printPrime(i);
			}
		}
		print("Found ");
		print(numPrimes);
		print(" prime numbers below ");
		print(this.size());
		print("\n");
	}

	var flags: *mut [bool];
}

fn main() {
	let N = 10000000;
	var sieve = Sieve(N);
	sieve.run();
  	sieve.report();
}