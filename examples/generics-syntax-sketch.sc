
fn foo[T: regular type](t: T) {
    let u = T();
    if t == u {} // Fine because regular is comparable
    t.baz(); // Error, concept regular does not provide baz() method
}

// Same as foo
fn bar(t: any regular) { 

}

fn foo(t: regular type) {

}

concept comparable[T: type] = trait { 
    fn ==(&T, &T) -> bool;
    fn !=(&T, &T) -> bool;
};

concept constructible[T: type, Args: type...] = trait {
    fn new(&mut T, Args...);
};

concept destructible[T: type] = trait { 
    fn delete(&mut T);
};

concept regular[T: type] = 
    comparable(T) &
    constructible(T) &
    constructible(T, &T) &
    destructible(T);

// Satisfied if T satisfies t.foo() or t.bar() but not both
concept weird[T: type] = trait { fn foo(&T); } ^ trait { fn bar(&T); };

struct S {}

concept can_add_to_s[T: type] = trait(S) { fn +=(&mut S, &T); }

// Regular
struct X {}

// Not regular
struct Y {
    fn new(&mut this, n: int) {}
}

fn test() {
    foo(0); // Ok
    foo(X()); // Ok
    foo(Y()); // Error, Y does not satisfy concept regular because it does not satisfy constructible(Y)
}