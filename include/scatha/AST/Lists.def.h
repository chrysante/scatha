// clang-format off

// ===----------------------------------------------------------------------===
// === List of all AST Nodes -----------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_ASTNODE_DEF
#   define SC_ASTNODE_DEF(Name, Parent, Corporeality)
#endif

SC_ASTNODE_DEF(ASTNode,                    VoidParent,           Abstract)
SC_ASTNODE_DEF(TranslationUnit,            ASTNode,              Concrete)
SC_ASTNODE_DEF(SourceFile,                 ASTNode,              Concrete)
SC_ASTNODE_DEF(Statement,                  ASTNode,              Abstract)
SC_ASTNODE_DEF(ImportStatement,            Statement,            Concrete)
SC_ASTNODE_DEF(CompoundStatement,          Statement,            Concrete)
SC_ASTNODE_DEF(Declaration,                Statement,            Abstract)
SC_ASTNODE_DEF(FunctionDefinition,         Declaration,          Concrete)
SC_ASTNODE_DEF(BaseClassDeclaration,       Declaration,          Concrete)
SC_ASTNODE_DEF(RecordDefinition,           Declaration,          Abstract)
SC_ASTNODE_DEF(StructDefinition,           RecordDefinition,     Concrete)
SC_ASTNODE_DEF(ProtocolDefinition,         RecordDefinition,     Concrete)
SC_ASTNODE_DEF(VarDeclBase,                Declaration,          Abstract)
SC_ASTNODE_DEF(VariableDeclaration,        VarDeclBase,          Concrete)
SC_ASTNODE_DEF(ParameterDeclaration,       VarDeclBase,          Concrete)
SC_ASTNODE_DEF(ThisParameter,              ParameterDeclaration, Concrete)
SC_ASTNODE_DEF(ExpressionStatement,        Statement,            Concrete)
SC_ASTNODE_DEF(EmptyStatement,             Statement,            Concrete)
SC_ASTNODE_DEF(ControlFlowStatement,       Statement,            Abstract)
SC_ASTNODE_DEF(ReturnStatement,            ControlFlowStatement, Concrete)
SC_ASTNODE_DEF(IfStatement,                ControlFlowStatement, Concrete)
SC_ASTNODE_DEF(LoopStatement,              ControlFlowStatement, Concrete)
SC_ASTNODE_DEF(JumpStatement,              ControlFlowStatement, Concrete)
SC_ASTNODE_DEF(Expression,                 ASTNode,              Abstract)
SC_ASTNODE_DEF(Literal,                    Expression,           Concrete)
SC_ASTNODE_DEF(FStringExpr,                Expression,           Concrete)
SC_ASTNODE_DEF(Identifier,                 Expression,           Concrete)
SC_ASTNODE_DEF(UnaryExpression,            Expression,           Concrete)
SC_ASTNODE_DEF(BinaryExpression,           Expression,           Concrete)
SC_ASTNODE_DEF(CastExpr,                   Expression,           Concrete)
SC_ASTNODE_DEF(MemberAccess,               Expression,           Concrete)
SC_ASTNODE_DEF(MoveExpr,                   Expression,           Concrete)
SC_ASTNODE_DEF(UniqueExpr,                 Expression,           Concrete)
SC_ASTNODE_DEF(AddressOfExpression,        Expression,           Concrete)
SC_ASTNODE_DEF(DereferenceExpression,      Expression,           Concrete)
SC_ASTNODE_DEF(Conditional,                Expression,           Concrete)
SC_ASTNODE_DEF(UninitTemporary,            Expression,           Concrete)
SC_ASTNODE_DEF(NontrivAssignExpr,          Expression,           Concrete)
SC_ASTNODE_DEF(CallLike,                   Expression,           Abstract)
SC_ASTNODE_DEF(FunctionCall,               CallLike,             Concrete)
SC_ASTNODE_DEF(ConstructExpr,              CallLike,             Concrete) // Deprecated
SC_ASTNODE_DEF(ConstructBase,              CallLike,             Abstract)

/// AST construction nodes
/// These are defined with a seperate macro because these are also part of
/// `sema::ObjectTypeConversion`

#ifndef SC_ASTNODE_CONSTR_DEF
#define SC_ASTNODE_CONSTR_DEF(Name, Parent, Corporeality)                      \
    SC_ASTNODE_DEF(Name##Expr, Parent, Corporeality)
#endif

SC_ASTNODE_CONSTR_DEF(TrivDefConstruct,       ConstructBase,        Concrete)
SC_ASTNODE_CONSTR_DEF(TrivCopyConstruct,      ConstructBase,        Concrete)
SC_ASTNODE_CONSTR_DEF(TrivAggrConstruct,      ConstructBase,        Concrete)
SC_ASTNODE_CONSTR_DEF(NontrivAggrConstruct,   ConstructBase,        Concrete)
SC_ASTNODE_CONSTR_DEF(NontrivConstruct,       ConstructBase,        Concrete)
SC_ASTNODE_CONSTR_DEF(NontrivInlineConstruct, ConstructBase,        Concrete)
SC_ASTNODE_CONSTR_DEF(DynArrayConstruct,      ConstructBase,        Concrete)

#undef SC_ASTNODE_CONSTR_DEF

SC_ASTNODE_DEF(Subscript,                  CallLike,             Concrete)
SC_ASTNODE_DEF(SubscriptSlice,             CallLike,             Concrete)
SC_ASTNODE_DEF(GenericExpression,          CallLike,             Concrete)
SC_ASTNODE_DEF(ListExpression,             Expression,           Concrete)
SC_ASTNODE_DEF(ConvExprBase,               Expression,           Abstract)
SC_ASTNODE_DEF(ValueCatConvExpr,           ConvExprBase,         Concrete)
SC_ASTNODE_DEF(QualConvExpr,               ConvExprBase,         Concrete)
SC_ASTNODE_DEF(ObjTypeConvExpr,            ConvExprBase,         Concrete) // Update NodeType::LAST in Fwd.h if this changes!

#undef SC_ASTNODE_DEF

// ===----------------------------------------------------------------------===
// === List of literal kinds -----------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_LITERAL_KIND_DEF
#   define SC_LITERAL_KIND_DEF(Kind, Str)
#endif

SC_LITERAL_KIND_DEF(Integer,         "integer")
SC_LITERAL_KIND_DEF(Boolean,         "boolean")
SC_LITERAL_KIND_DEF(FloatingPoint,   "floating point")
SC_LITERAL_KIND_DEF(Null,            "null")
SC_LITERAL_KIND_DEF(This,            "this")
SC_LITERAL_KIND_DEF(String,          "string")
SC_LITERAL_KIND_DEF(FStringBegin,    "fstring begin")
SC_LITERAL_KIND_DEF(FStringContinue, "fstring continue")
SC_LITERAL_KIND_DEF(FStringEnd,      "fstring end")
SC_LITERAL_KIND_DEF(Char,            "char")

#undef SC_LITERAL_KIND_DEF

// ===----------------------------------------------------------------------===
// === List of unary operators ---------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_UNARY_OPERATOR_DEF
#   define SC_UNARY_OPERATOR_DEF(name, opStr)
#endif

SC_UNARY_OPERATOR_DEF(Promotion,  "+")
SC_UNARY_OPERATOR_DEF(Negation,   "-")
SC_UNARY_OPERATOR_DEF(BitwiseNot, "~")
SC_UNARY_OPERATOR_DEF(LogicalNot, "!")
SC_UNARY_OPERATOR_DEF(Increment,  "++")
SC_UNARY_OPERATOR_DEF(Decrement,  "--")

#undef SC_UNARY_OPERATOR_DEF

// ===----------------------------------------------------------------------===
// === List of unary operator notation -------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_UNARY_OPERATOR_NOTATION_DEF
#   define SC_UNARY_OPERATOR_NOTATION_DEF(name, opStr)
#endif

SC_UNARY_OPERATOR_NOTATION_DEF(Prefix,  "prefix")
SC_UNARY_OPERATOR_NOTATION_DEF(Postfix, "postfix")

#undef SC_UNARY_OPERATOR_NOTATION_DEF

// ===----------------------------------------------------------------------===
// === List of binary operators --------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_BINARY_OPERATOR_DEF
#   define SC_BINARY_OPERATOR_DEF(node, opStr)
#endif

SC_BINARY_OPERATOR_DEF(Multiplication, "*"  )
SC_BINARY_OPERATOR_DEF(Division,       "/"  )
SC_BINARY_OPERATOR_DEF(Remainder,      "%"  )
SC_BINARY_OPERATOR_DEF(Addition,       "+"  )
SC_BINARY_OPERATOR_DEF(Subtraction,    "-"  )
SC_BINARY_OPERATOR_DEF(LeftShift,      "<<" )
SC_BINARY_OPERATOR_DEF(RightShift,     ">>" )
SC_BINARY_OPERATOR_DEF(Less,           "<"  )
SC_BINARY_OPERATOR_DEF(LessEq,         "<=" )
SC_BINARY_OPERATOR_DEF(Greater,        ">"  )
SC_BINARY_OPERATOR_DEF(GreaterEq,      ">=" )
SC_BINARY_OPERATOR_DEF(Equals,         "==" )
SC_BINARY_OPERATOR_DEF(NotEquals,      "!=" )
SC_BINARY_OPERATOR_DEF(BitwiseAnd,     "&"  )
SC_BINARY_OPERATOR_DEF(BitwiseXOr,     "^"  )
SC_BINARY_OPERATOR_DEF(BitwiseOr,      "|"  )
SC_BINARY_OPERATOR_DEF(LogicalAnd,     "&&" )
SC_BINARY_OPERATOR_DEF(LogicalOr,      "||" )
SC_BINARY_OPERATOR_DEF(Assignment,     "="  )
SC_BINARY_OPERATOR_DEF(AddAssignment,  "+=" )
SC_BINARY_OPERATOR_DEF(SubAssignment,  "-=" )
SC_BINARY_OPERATOR_DEF(MulAssignment,  "*=" )
SC_BINARY_OPERATOR_DEF(DivAssignment,  "/=" )
SC_BINARY_OPERATOR_DEF(RemAssignment,  "%=" )
SC_BINARY_OPERATOR_DEF(LSAssignment,   "<<=")
SC_BINARY_OPERATOR_DEF(RSAssignment,   ">>=")
SC_BINARY_OPERATOR_DEF(AndAssignment,  "&=" )
SC_BINARY_OPERATOR_DEF(OrAssignment,   "|=" )
SC_BINARY_OPERATOR_DEF(XOrAssignment,  "^=" )
SC_BINARY_OPERATOR_DEF(Comma,          ","  )

#undef SC_BINARY_OPERATOR_DEF
