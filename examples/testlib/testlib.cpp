#include <scatha/Runtime/LibSupport.h>

int myFunction(int a, int b) {
    return a + b;
}

SC_EXPORT_FUNCTION(myFunction, "myFunction");
