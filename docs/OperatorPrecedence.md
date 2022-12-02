
 ## Operator precedence

| Precedence  | Operator        | Description                             | Associativity     |
| ----------- | --------------- | --------------------------------------- | ----------------- |
| 1           | () <br>\[\] <br> . | Function call <br>Subscript<br> Member access | Left to right  -> |
|             |                 |                                         |                   |
| 2           | +, -            | Unary plus and minus                    | Right to left <-  |
|             | !, ~            | Logical and bitwise NOT                 |                   |
|             | &               | Address of                              |                   |
|             |                 |                                         |                   |
| 3           | \*, /, %        | Multiplication, division, and remainder | Left to right ->  |
|             |                 |                                         |                   |
| 4           | +, -            | Addition and subtraction                |                   |
| 5           | \<\<, \>\>      | Bitwise left and right shift            |                   |
| 6           | <, <=, >, >=    | Relational operators                    |                   |
| 7           | ==, !=          | Equality operators                      |                   |
| 8           | &               | Bitwise AND                             |                   |
| 9           | ^               | Bitwise XOR                             |                   |
| 10          | \|              | Bitwise OR                              |                   |
| 11          | &&              | Logical AND                             |                   |
| 12          | ||              | Logical OR                              |                   |
|             |                 |                                         |                   |
|13           | ?:              | Conditional                             | Right to left <-  |
|             | =, +=, -=       | Assignment                              |                   |
|             | *=, /=, %=      |                                         |                   |
|             | <<=, >>=,       |                                         |                   |
|             | &=, \|=,        |                                         |                   |
|             |                 |                                         |                   |
| 14          | ,               | Comma operator                          | Left to right ->  |

