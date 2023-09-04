
public fn main() -> int {
    var data = [
        15,   50,   82 //,   57,    7,   42,   86,   23,   60,   51
        //17,   19,   80,   33,   49,   35,   79,   98,   89,   27,
        //92,   45,   43,    5,   88,    2,   58,   75,   22,   18,
        //30,   41,   70,   40,    3,   84,   63,   39,   56,   97,
        //81,    6,   64,   47,   90,   20,   77,   12,   74,   55,
        //78,    4,   28,   52,   61,   85,   32,   37,   95,   83,
        //87,   54,   76,   72,    9,   65,   11,   31,   10,    1,
        //25,   73,   44,   71,   68,    8,   67,   13,   91,   24,
        //62,   21,   66,   48,   99,   94,   69,   46,  100,   38,
        //16,   53,   96,   34,   59,   14,   29,   93,   36,   26
    ];
    sort(&mut data);
    print(&data);
}

fn print(data: &[int]) {
    for i = 0; i < data.count; ++i {
        __builtin_puti64(data[i]);
        __builtin_putstr(", ");
    }
    __builtin_putchar('\n');
}

fn sort(data: &mut [int]) {
    if data.count == 0 {
        return;
    }
    let splitIndex = split(&mut data);
    sort(&mut data[0:splitIndex]);
    sort(&mut data[splitIndex + 1:data.count]);
}

fn split(data: &mut [int]) -> int {
    var i = 0;
    // Starte mit j links vom Pivotelement
    var j = data.count - 2;
    let pivot = data[data.count - 1];
    while i < j {
        // Suche von links ein Element, welches größer als das Pivotelement ist
        while i < j && data[i] <= pivot {
            ++i;
        }

        // Suche von rechts ein Element, welches kleiner oder gleich dem Pivotelement ist
        while j > i && data[j] > pivot {
            --j;
        }

        if data[i] > data[j] {
            swap(&mut data[i], &mut data[j]);
        }
    }

    // Tausche Pivotelement (daten[rechts]) mit neuer endgültiger Position (daten[i])
    // und gib die neue Position des Pivotelements zurück, beende Durchlauf
    if data[i] > pivot {
        swap(&mut data[i], &mut data[data.count - 1]);
    } else {
        i = data.count - 1;
    }
    return i;
}

fn swap(a: &mut int, b: &mut int) {
    let tmp = a;
    a = b;
    b = tmp;
}













/*

fn printVal(x: &X) {
        print("[");
        print(x.value);
        print("]");
}

struct Y { var x: X; }

struct X {
    /// Default constructor
    fn new(&mut this) {
        this.value = 1;
        print("+ ");
        printVal(&this);
        print("\n");
    }

    fn new(&mut this, n: int) {
        this.value = n;
        print("+ ");
        printVal(&this);
        print(" (n: int)\n");
    }

    /// Copy constructor
    fn new(&mut this, rhs: &X) {
        this.value = rhs.value + 1;
        print("+ ");
        printVal(&this);
        print(" (Copy)\n");
    }

    /// Move constructor
    fn move(&mut this, rhs: &mut X) {
        print("+ ");
        printVal(&this);
        print(" (Move)\n");
    }

    fn delete(&mut this) {
        print("- ");
        printVal(&this);
        print("\n");
    }

    var value: int;
}

struct ScopeGuard {
    fn new(&mut this, text: &str) {
        this.text = &text;
    }
    fn delete(&mut this) {
        print(&this.text);
    }
    var text: &str;
}

fn take(x: X) -> X {
    print("took X(");
    print(x.value);
    print(")\n");
    var guard = ScopeGuard("Leaving take()\n");
    return x;
}

public fn main() -> int {
    //var x: X;
    //var y = x;
    //var z = X(X(4).value + 1);

    //return X().value;
    
    //var sum = 0;
    //for x = X(1); x.value <= 3; x.value += 1 {
    //        var y = X(2);
    //    if x.value == 2 {
    //        break;
    //    }
    //    sum += x.value;
    //}
    //return sum;
    
    //var x = X(1);
    //{
    //    var y = x;
    //}
    //var z = X(3);
    
    
    var cond = true;
    
    // let j = cond ? X(1) : X(2);
    var i = 1;
    var j = 2;
    let k = cond ? &mut i : &mut j;
    k = 3;
    return i;
}

fn print(text: &str) {
    __builtin_putstr(&text);
}

fn print(n: int) {
    __builtin_puti64(n);
}

*/
