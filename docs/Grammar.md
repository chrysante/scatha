## Incomplete Grammar in BNF

|                                                                           |                                                                                                                             |
| ---                                                                       | ---                                                                                                                         |
| ``translation-unit``                                                      | = {``external-declaration``}                                                                                                |
| ``external-declaration``<br>&nbsp;                                        | = ``function-definition`` <br> \| ``struct-definition``                                                                     |
| ``function-definition``                                                   |  = "fn" ``identifier`` "(" {``parameter-declaration``} ")" ["->" ``type-expression``] ``compound-statement``                |
| ``parameter-declaration``                                                 |  = ``identifier`` ":" ``type-expression``                                                                                   |
| ``struct-definition``                                                     |  = "struct" ``identifier`` ``compound-statement``                                                                           |
| ``variable-declaration``                                                  |  = "let" ``identifier`` [":" ``type-expression``] ["=" ``assignment-expression``] ";"                                       |
| ``statement`` <br>&nbsp;<br>&nbsp;<br>&nbsp;<br>&nbsp;<br>&nbsp;          |  = ``external-declaration`` <br> \| ``variable-declaration`` <br> \| ``control-flow-statement`` <br> \| ``compound-statement`` <br> \| ``expression-statement`` <br> \| ";" |
| ``expression-statement``                                                  |  = ``commma-expression`` ";"                                                                                                | 
| ``compound-statement``                                                    |  = "{" {``statement``} "}"                                                                                                  |
| ``control-flow-statement`` <br>&nbsp;<br>&nbsp;                           |  = ``return-statement`` <br> \| ``if-statement`` <br> \| ``while-statement``                                                |
| ``return-statement``                                                      |  = "return" \[``comma-expression``\] ";"                                                                                    |
| ``if-statement``                                                          |  = "if" ``comma-expression`` ``compound-statement`` ["else" (``if-statement`` \| ``compound-statement``)]                   |
| ``while-statement``                                                       |  = "while" ``comma-expression`` ``compound-statement``                                                                      |
| ``comma-expression``                                                      |  = ``assignment-expression`` <br> \| ``comma-expression`` "," ``assignment-expression``                                     |
| ``assignment-expression``                                                 |  = ``conditional-expression`` <br> \| ``conditional-expression`` ( "=" \| "*=" \| ... ) ``assignment-expression``           |
| ``type-expression``                                                       |  = ``conditional-expression``                                                                                               |
| ``conditional-expression``                                                |  = ``logical-or-expression`` <br> \| ``logical-or-expression`` "?" ``comma-expression`` ":" ``conditional-expression``      |
| ``logical-or-expression``                                                 |  = ``logical-and-expression`` <br> \| ``logical-or-expression`` "\|\|" ``logical-and-expression``                           |
| ``logical-and-expression``                                                |  = ``inclusive-or-expression`` <br> \| ``logical-and-expression`` "&&" ``inclusive-or-expression``                          |
| ``inclusive-or-expression``                                               |  = ``exclusive-or-expression`` <br> \| ``inclusive-or-expression`` "\|" ``exclusive-or-expression``                         |
| ``exclusive-or-expression``                                               |  = ``and-expression`` <br> \| ``exclusive-or-expression`` "^" ``and-expression``                                            |
| ``and-expression``                                                        |  = ``equality-expression`` <br> \| ``and-expression`` "&" ``equality-expression``                                           |
| ``equality-expression``                                                   |  = ``relational-expression>`` <br> \| ``equality-expression>`` ( "==" \| "!=" ) ``relational-expression>``                  | 
| ``relational-expression``                                                 |  = ``shift-expression`` <br> \| ``relational-expression`` ( "<" \| ">" \| "<=" \| ">=" )  ``shift-expression``              |
| ``shift-expression``                                                      |  = ``additive-expression`` <br> \| ``shift-expression`` ( "<<" \| ">>") ``additive-expression``                             |
| ``additive-expression``                                                   |  = ``multiplicative-expression`` <br> \| ``additive-expression`` ("+" \| "-" ) ``multiplicative-expression``                |
| ``multiplicative-expression``                                             |  = ``unary-expression`` <br> \| ``multiplicative-expression`` ( "*" \| "/" \| "%" ) ``unary-expression``                    |
| ``unary-expression``                                                      |  = ``postfix-expression`` <br> \| ( "&" \| "+" \| "-" \| "~" \| "!" ) ``unary-expression``                                  |
| ``postfix-expression`` <br>&nbsp;<br>&nbsp;<br>&nbsp;                     |  = ``primary-expression`` <br> \| ``postfix-expression`` "(" {``assignment-expression``} ")" <br> \| ``postfix-expression`` "[" {``assignment-expression``} "]" <br> \| ``postfix-expression`` "." ``identifier`` |
| ``primary-expression`` <br>&nbsp;<br>&nbsp;<br>&nbsp;<br>&nbsp;<br>&nbsp; |  = ``identifier`` <br> \| ``integer-literal`` <br> \| ``boolean-literal`` <br> \| ``floating-point-literal`` <br> \| ``string-literal`` <br> \| "(" ``comma-expression`` ")" |
