## Language

### Array pointers should be automatically dereferenced when accessing them, i.e.
```
let ptr = &mut myArray;
ptr[0] = 42;          // Now (*ptr)[42] is required which is unnecessary 
let size = ptr.count; // Now (*ptr).count or ptr->count is required
```
Then we can also remove the `->` member access operator

### Add move semantics

### Add dynamic memory allocation in the language 

## Frontend

### Restructure `FunctionBodyAnalysis` so it just takes a single function at a time

### Fix parser crashes

## IR

### Make IR verification pass better  
### Improve inlining decisions
### Fix TRE for void calls
### Create annotations for functions such as no side effects etc.
### Clean up `AccessTree` class

## Backend

### Write selection DAG based code generation algorithm
