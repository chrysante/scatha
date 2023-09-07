
fn print(text: &str) {
	__builtin_putstr(text);
}

struct X {
	fn new(&mut this, text: *str)	{
		this.text = text;
		"sfnjkdsad";
	}

	fn print(&this) {
		print(*this.text);
	}

	/// Mutable pointer to constant str
	text: mut *str;
	
	/// Mutable pointer to constant Y
	ptr: mut *Y;

	/// Mutable pointer to mutable Y
	mutPtr: mut *mut Y; 

	/// Error. Struct member cannot be of reference type
	text2: &str;
}

struct Y {}

fn use(x: &X) {
	x.print();
}

fn modify(x: &mut X) {
	x.text = &"Modified text\n";
}

fn modify2(x: &mut X, y: *Y) {
	x.ptr = y;
}


fn modifyMut(x: &mut X, y: *mut Y) {
	x.ptr = y;
}

fn use2(x: &Y) {
	var x = X();
	/// This fails because we cannot convert a reference to a pointer
	x.ptr = &y;
}

export fn main() {
	/// Need to explicitly take address here to create a pointer
	var x = X(&"Hello World!\n");
	
	/// Fine, local variables implicitly bind to references
	use(x);
	
	/// Modify via a reference. Lifetime is fine because the reference is 
	/// guaranteed to not outlive the lifetime of x
	modify(x);

	var y = Y();
	/// We pass in y as a pointer because it is stored beyond the scope of the
	/// called function
	/// When using pointers it is the responsibility of the user that the 
	/// pointer will not be used after the lifetime of the pointee has ended
	modify2(x, &y);

	/// To create a mutable pointer to y we use the &mut operator
	modifyMut(x, &mut y);

	/// Fine, because temporaries bind to const references
	use(X(&"Bye!\n"));
}