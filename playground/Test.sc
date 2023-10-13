
// Names from the library can be accessed as lib.name
import lib;

// Names from the library can be accessed as lib_name.name
import lib as lib_name;

// Names from the library are imported into the global scope
use lib;

fn main() {
    return lib.f();
}
