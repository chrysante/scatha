
 ## Operator precedence

| Precedence  | Operator                                                                                                  | Description                                                      | Associativity         |
| ----------- | --------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- | --------------------- |
| 1           | ``()`` <br>``[]`` <br> ``.``                                                                              | Function call <br>Subscript<br> Member access                    | Left to right &#8594; |
| 2           | ``+``, ``-`` <br>``!``, ``~`` <br>``&``                                                                   | Unary plus and minus <br>Logical and bitwise NOT <br> Address of | Right to left &#8592; |
| 3           | ``*``, ``/``, ``%``                                                                                       | Multiplication, division, and remainder                          | Left to right &#8594; |
| 4           | ``+``, ``-``                                                                                              | Addition and subtraction                                         | Left to right &#8594; |
| 5           | ``<<``, ``>>``                                                                                            | Bitwise left and right shift                                     | Left to right &#8594; |
| 6           | ``<``, ``<=``, ``>``, ``>=``                                                                              | Relational operators                                             | Left to right &#8594; |
| 7           | ``==``, ``!=``                                                                                            | Equality operators                                               | Left to right &#8594; |
| 8           | ``&``                                                                                                     | Bitwise AND                                                      | Left to right &#8594; |
| 9           | ``^``                                                                                                     | Bitwise XOR                                                      | Left to right &#8594; |
| 10          | ``\|``                                                                                                    | Bitwise OR                                                       | Left to right &#8594; |
| 11          | ``&&``                                                                                                    | Logical AND                                                      | Left to right &#8594; |
| 12          | ``\|\|``                                                                                                  | Logical OR                                                       | Left to right &#8594; |
| 13          | ``?:`` <br>``=``, ``+=``, ``-=``, <br> ``*=``, ``/=``, ``%=``, <br> ``<<=``, ``>>=``, <br>``&=``, ``\|=`` | Conditional <br> Assignment <br><br><br><br>                     | Right to left &#8592; |
| 14          | ``,``                                                                                                     | Comma operator                                                   | Left to right &#8594; |

