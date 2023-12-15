#include <scatha/Runtime/LibSupport.h>

int myFunction() {
    return 42;
}

SC_EXPORT_FUNCTION(myFunction, "myFunction");
