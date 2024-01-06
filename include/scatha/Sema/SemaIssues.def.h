// No include guards
// This file is has .h extension so it will be caught by our formatting script

// ===--------------------------------------------------------------------=== //
// === List of all reasons for a generic bad statement -------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_GENERICBADSTMT_DEF
#define SC_SEMA_GENERICBADSTMT_DEF(reason, severity, text)
#endif

SC_SEMA_GENERICBADSTMT_DEF(ReservedIdentifier, Error, "Reserved identifier")
SC_SEMA_GENERICBADSTMT_DEF(InvalidScope,
                           Error,
                           ::format(statement())
                               << " is invalid in " << ::format(scope()))

#undef SC_SEMA_GENERICBADSTMT_DEF

// ===--------------------------------------------------------------------=== //
// === List of all reasons for a bad variable declaration ----------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_BADVARDECL_DEF
#define SC_SEMA_BADVARDECL_DEF(Reason, Severity, Message)
#endif

SC_SEMA_BADVARDECL_DEF(IncompleteType, Error, {
    header("Cannot declare variable of incomplete type");
    auto* decl = declaration();
    auto range = decl->typeExpr() ? decl->typeExpr()->sourceRange() :
                                    decl->sourceRange();
    primary(range, [=, this](std::ostream& str) {
        str << sema::format(type()) << " is incomplete";
    });
})
SC_SEMA_BADVARDECL_DEF(ExpectedRefInit, Error, {
    header("Reference declaration requires initializer");
    primary(declaration()->sourceRange(), [=, this](std::ostream& str) {
        str << "Reference '" << declaration()->name()
            << "' declared here without initializer";
    });
})
SC_SEMA_BADVARDECL_DEF(CantInferType, Error, {
    header("Cannot infer type");
    primary(declaration()->sourceRange(), [=, this](std::ostream& str) {
        str << "Variable '" << declaration()->name()
            << "' declared without type and initializer";
    });
})
SC_SEMA_BADVARDECL_DEF(RefInStruct, Error, {
    header("Cannot declare variable of reference type in struct");
    auto* var = cast<ast::VariableDeclaration const*>(declaration());
    primary(var->typeExpr()->sourceRange(), [=](std::ostream& str) {
        str << "Variable " << var->name() << " of type "
            << sema::format(var->type()) << " declared here";
    });
})
SC_SEMA_BADVARDECL_DEF(ThisInFreeFunction, Error, {
    header("'this' parameter can only be declared in member functions");
    primary(declaration()->sourceRange(), [=, this](std::ostream& str) {
        auto* function = declaration()->findAncestor<ast::FunctionDefinition>();
        str << "'this' parameter in free function '" << function->name()
            << "' declared here";
    });
})
SC_SEMA_BADVARDECL_DEF(ThisPosition, Error, {
    header("'this' parameter can only be declared as the first parameter");
    auto* param = cast<ast::ParameterDeclaration const*>(declaration());
    primary(param->sourceRange(), [=](std::ostream& str) {
        auto* function = param->findAncestor<ast::FunctionDefinition>();
        str << "'this' parameter in function '" << function->name()
            << "' declared as " << formatOrdinal(param->index())
            << " parameter here";
    });
})

#undef SC_SEMA_BADVARDECL_DEF

// ===--------------------------------------------------------------------=== //
// === List of all reasons for a bad function definition -----------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_BADFUNCDEF_DEF
#define SC_SEMA_BADFUNCDEF_DEF(reason, severity, text)
#endif

SC_SEMA_BADFUNCDEF_DEF(MainMustReturnTrivial,
                       Error,
                       "Function 'main' cannot return non-trivial type "
                           << sema::format(definition()->returnType()))

SC_SEMA_BADFUNCDEF_DEF(MainInvalidArguments,
                       Error,
                       sema::format(definition()->function()->type())
                           << " is not a valid signature for 'main'. "
                           << " Valid signatures are () and (&[*str])")

SC_SEMA_BADFUNCDEF_DEF(FunctionMustHaveBody,
                       Error,
                       "Function '" << definition()->name() << "' has no body")

SC_SEMA_BADFUNCDEF_DEF(UnknownLinkage,
                       Error,
                       "Unknown linkage: \""
                           << definition()->externalLinkage().value_or("")
                           << "\"")

SC_SEMA_BADFUNCDEF_DEF(ExternCNotSupported,
                       Error,
                       "'extern \"C\"' declaration is not supported")

SC_SEMA_BADFUNCDEF_DEF(FunctionDeclarationHasNoReturnType,
                       Error,
                       "Function declaration '" << definition()->name()
                                                << "' has no return type'")

#undef SC_SEMA_BADFUNCDEF_DEF

// ===--------------------------------------------------------------------=== //
// === List of all reasons for a bad special member function -------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_BADSMF_DEF
#define SC_SEMA_BADSMF_DEF(reason, severity, text)
#endif

SC_SEMA_BADSMF_DEF(HasReturnType,
                   Error,
                   "Function '" << SMF() << "' must not have a return type")
SC_SEMA_BADSMF_DEF(NotInStruct,
                   Error,
                   "Function '" << SMF() << "' must be a member function")
SC_SEMA_BADSMF_DEF(NoParams,
                   Error,
                   "Function '" << SMF()
                                << "' must have at least one parameter of type "
                                << "&mut " << parent()->name())
SC_SEMA_BADSMF_DEF(BadFirstParam,
                   Error,
                   "The first parameter to function '"
                       << SMF() << "' must be of type "
                       << "&mut " << parent()->name())
SC_SEMA_BADSMF_DEF(MoveSignature,
                   Error,
                   "The parameters types of function '"
                       << SMF() << "' must be "
                       << "&mut " << parent()->name() << ", "
                       << "&mut " << parent()->name())
SC_SEMA_BADSMF_DEF(DeleteSignature,
                   Error,
                   "Function '" << SMF() << "' must have exactly one parameter")

#undef SC_SEMA_BADSMF_DEF

// ===--------------------------------------------------------------------=== //
// === List of all reasons for a bad return statement --------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_BADRETURN_DEF
#define SC_SEMA_BADRETURN_DEF(reason, severity, text)
#endif

SC_SEMA_BADRETURN_DEF(NonVoidMustReturnValue,
                      Error,
                      "Non-void function must return a value")
SC_SEMA_BADRETURN_DEF(VoidMustNotReturnValue,
                      Error,
                      "Void function must not return a value")

#undef SC_SEMA_BADRETURN_DEF

// ===--------------------------------------------------------------------=== //
// === List of all reasons for a bad expression --------------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_BADEXPR_DEF
#define SC_SEMA_BADEXPR_DEF(ExprType, Reason, Severity, Message)
#endif

SC_SEMA_BADEXPR_DEF(Expression, BadExprNone, Error, "No issue")

/// This should only be used for temporary language constructs for which it is
/// not worthwhile to make special errors
SC_SEMA_BADEXPR_DEF(Expression, GenericBadExpr, Error, "Bad expression")

SC_SEMA_BADEXPR_DEF(Identifier,
                    UndeclaredID,
                    Error,
                    "Use of undeclared identifier '" << expr->value() << "'")

SC_SEMA_BADEXPR_DEF(Identifier,
                    AccessedMemberWithoutObject,
                    Error,
                    "Cannot access member '" << expr->value()
                                             << "' without an object argument")

SC_SEMA_BADEXPR_DEF(UnaryExpression,
                    UnaryExprBadType,
                    Error,
                    "Operand type " << expr->operand()->type()->name()
                                    << " is invalid for unary operator "
                                    << expr->operation())

SC_SEMA_BADEXPR_DEF(UnaryExpression,
                    UnaryExprValueCat,
                    Error,
                    expr->operand()->valueCategory()
                        << " operand is invalid for unary operator "
                        << expr->operation())

SC_SEMA_BADEXPR_DEF(UnaryExpression,
                    UnaryExprImmutable,
                    Error,
                    "Immutable operand is invalid for unary operator "
                        << expr->operation())

SC_SEMA_BADEXPR_DEF(BinaryExpression,
                    BinaryExprNoCommonType,
                    Error,
                    "Operand types " << expr->lhs()->type()->name() << " and "
                                     << expr->rhs()->type()->name()
                                     << " have no common type")

SC_SEMA_BADEXPR_DEF(BinaryExpression,
                    BinaryExprBadType,
                    Error,
                    ::format(expr->operation())
                        << " is not supported for operand types "
                        << expr->lhs()->type()->name() << " and "
                        << expr->rhs()->type()->name())

SC_SEMA_BADEXPR_DEF(BinaryExpression,
                    BinaryExprValueCatLHS,
                    Error,
                    expr->lhs()->valueCategory()
                        << " operand is invalid in assignment expression")

SC_SEMA_BADEXPR_DEF(BinaryExpression,
                    BinaryExprImmutableLHS,
                    Error,
                    "Immutable operand is invalid in assignment expression")

SC_SEMA_BADEXPR_DEF(MemberAccess,
                    MemAccNonStaticThroughType,
                    Error,
                    "Cannot access non-static member "
                        << expr->member()->value() << " without an object")

SC_SEMA_BADEXPR_DEF(MemberAccess,
                    MemAccTypeThroughValue,
                    Error,
                    "Cannot access type " << expr->member()->value()
                                          << " through an object")

SC_SEMA_BADEXPR_DEF(
    Identifier,
    AccessDenied,
    Error,
    expr->value() << " is "
                  << formatWithIndefArticle(expr->entity()->accessControl())
                  << " member")

SC_SEMA_BADEXPR_DEF(Conditional,
                    ConditionalNoCommonType,
                    Error,
                    "Operand types " << expr->thenExpr()->type()->name()
                                     << " and "
                                     << expr->elseExpr()->type()->name()
                                     << " have no common type")

SC_SEMA_BADEXPR_DEF(DereferenceExpression,
                    DerefNoPtr,
                    Error,
                    "Cannot dereference non-pointer type "
                        << expr->referred()->type()->name())

SC_SEMA_BADEXPR_DEF(AddressOfExpression,
                    AddrOfNoLValue,
                    Error,
                    "Cannot take the address of an rvalue of type "
                        << expr->referred()->type()->name())

SC_SEMA_BADEXPR_DEF(AddressOfExpression,
                    MutAddrOfImmutable,
                    Error,
                    "Cannot take mutable address of an immutable value of type "
                        << expr->referred()->type()->name())

SC_SEMA_BADEXPR_DEF(CallLike,
                    SubscriptNoArray,
                    Error,
                    "Subscript expression on non-array type "
                        << expr->callee()->type()->name())

SC_SEMA_BADEXPR_DEF(Subscript,
                    SubscriptArgCount,
                    Error,
                    "Subscript expression requires exactly one argument")

SC_SEMA_BADEXPR_DEF(FunctionCall,
                    ExplicitSMFCall,
                    Error,
                    "Cannot explicitly call special member function '"
                        << expr->function()->SMFKind() << "'")

SC_SEMA_BADEXPR_DEF(FunctionCall,
                    ObjectNotCallable,
                    Error,
                    "Object not callable")

SC_SEMA_BADEXPR_DEF(FunctionCall,
                    CantDeduceReturnType,
                    Error,
                    "Cannot deduce return type on recursive function")

SC_SEMA_BADEXPR_DEF(ConstructExpr,
                    CannotConstructType,
                    Error,
                    "Cannot construct type")

SC_SEMA_BADEXPR_DEF(NonTrivAssignExpr,
                    CannotAssignUncopyableType,
                    Error,
                    "Cannot assign uncopyable type")

SC_SEMA_BADEXPR_DEF(ListExpression,
                    ListExprNoCommonType,
                    Error,
                    "No common type in array expression")

SC_SEMA_BADEXPR_DEF(ListExpression,
                    ListExprVoid,
                    Error,
                    "Invalid array of void elements")

SC_SEMA_BADEXPR_DEF(ListExpression,
                    ListExprTypeExcessElements,
                    Error,
                    "Too many arguments in array type expression")

SC_SEMA_BADEXPR_DEF(Expression,
                    ListExprNoIntSize,
                    Error,
                    "Array type expression requires integral size")

SC_SEMA_BADEXPR_DEF(Expression,
                    ListExprNoConstSize,
                    Error,
                    "Array type expression requires constant size")

SC_SEMA_BADEXPR_DEF(Expression,
                    ListExprNegativeSize,
                    Error,
                    "Array type expression requires non-negative size")

SC_SEMA_BADEXPR_DEF(ListExpression,
                    ListExprBadEntity,
                    Error,
                    "Invalid entity for array expression")

SC_SEMA_BADEXPR_DEF(MoveExpr,
                    MoveExprConst,
                    Error,
                    "Cannot move immutable object")

SC_SEMA_BADEXPR_DEF(MoveExpr,
                    MoveExprImmovable,
                    Error,
                    "Cannot move immovable object")

SC_SEMA_BADEXPR_DEF(MoveExpr,
                    MoveExprRValue,
                    Warning,
                    "Moving rvalue object has no effect")

SC_SEMA_BADEXPR_DEF(MoveExpr,
                    MoveExprCopies,
                    Warning,
                    "Moving object without move constructor results in a copy")

SC_SEMA_BADEXPR_DEF(UniqueExpr,
                    UniqueExprNoRValue,
                    Error,
                    "Unique expression must be an rvalue")

#undef SC_SEMA_BADEXPR_DEF
