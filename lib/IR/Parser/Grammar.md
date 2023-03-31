## IR Grammar

`<non-terminals>` are written in angle brackets, `TERMINALS` or "terminals" are either written in all-caps or quoted. 

### List of terminals

`INT_LIT`, `FLOAT_LIT` `UNDEF`

```
<module>          ::= {<struct-def>}* {<func-decl>}* {<func-def>}*

<func-decl>       ::= "func" <type-id> <global-id> "(" [<param-list>] ")" 
<param-list>      ::= <type-id> ["," <type-id>]

<func-def>        ::= <func-decl> "{" {<basic-block>}+ "}"

<basic-block>     ::= <local-id> ":" {<statement>}+
<instruction>     ::= [<local-id> "="] <inst-body>
<inst-body>       ::= "alloca" <type-id>
                    | "load" <type-id> "," "ptr" <id>
                    | "store" "ptr" <id> "," <type-id> <value>
                    | "goto" "label" <local-id>
                    | "branch" "i1" <value> ","
                               "label" <local-id> ","
                               "label" <local-id>
                    | "return" [<type-id> <value>]
                    | "call" <type-id> <global-id> {"," <type-id> <value>}*
                    | "phi" <type-id> {<phi-arg>}+
                    | "cmp" <cmp-op> <type-id> <value> "," <type-id> <value>
                    | <un-op> <type-id> <value>
                    | <bin-op> <type-id> <value> "," <type-id> <value>
                    | "gep" "inbounds" <type-id> ","
                                       "ptr" <value> ","   
                                       <type-id> <value>
                                       <lit-idx-list>
                    | "insert_value" <type-id> <value> ","
                                     <type-id> <value> <lit-idx-list>
                    | "extract_value" <type-id> <value> <lit-idx-list>
                    | "select" "i1" <value> "," 
                               <type-id> <value> "," 
                               <type-id> <value> 
<call-arg>        ::= 
<phi-arg>         ::= "[" "label" <local-id> ":" <id> "]"
<cmp-op>          ::= "le", "leq",
<un-op>           ::= "neg" | ...
<bin-op>          ::= "add" | "sub" | "mul" | "div" | "rem" | ...
<lit-idx-list>    ::= { "," <int-lit> }+

<struct-def>      ::= "struct" <identifier> "{" {<type-id>}* "}"

<value>           ::= <id> | <literal>
<id>              ::= <local-id> | <global-id>
<literal>         ::= INT_LIT | FLOAT_LIT | "undef"
<type-id>         ::= "void" | "ptr" | INT_TYPE | FLOAT_TYPE | <global-id>

```
