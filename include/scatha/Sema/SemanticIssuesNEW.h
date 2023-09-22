#ifndef SCATHA_SEMA_SEMANTICISSUES_H_
#define SCATHA_SEMA_SEMANTICISSUES_H_

#include <iosfwd>
#include <span>
#include <vector>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue.h>
#include <scatha/Sema/Fwd.h>

/// # Hierarchy of semantic issue classes
/// This closely reflects the hierarchy of AST nodes
///
/// ```
/// SemanticIssue
/// ├─ BadStmt
/// │  ├─ GenericBadStmt
/// │  ├─ BadDecl
/// │  │  ├─ Redefinition
/// │  │  ├─ BadVarDecl
/// │  │  ├─ BadParamDecl
/// │  │  ├─ BadSMF
/// │  │  └─ StructDefCycle
/// │  └─ BadReturnStatement
/// └─ BadExpression
///    ├─ BadIdentifier
///    ├─ BadLiteral ??
///    ├─ BadUnaryExpr
///    ├─ BadBinaryExpr
///    ├─ BadMemAcc
///    ├─ BadCondExpr
///    ├─ BadFunctionCall
///    ├─ BadConstructorCall
///    ├─ BadSubscript
///    ├─ BadPointerReference
///    ├─ BadPointerDereference
///    └─ BadTypeConversion
/// ```

namespace scatha::sema {

#define SC_SEMA_ISSUE_REASON()                                                 \
private:                                                                       \
    Reason _reason{};                                                          \
                                                                               \
public:                                                                        \
    Reason reason() const { return _reason; }

#define SC_SEMA_DERIVED_STMT(Type, Name)                                       \
    template <typename T = ast::Type>                                          \
    T const* Name() const {                                                    \
        return cast_or_null<T const*>(BadStmt::statement());                   \
    }

/// Base class of all semantic issues
class SCATHA_API SemaIssue: public Issue {
public:
    SemaIssue(Scope const* scope,
              SourceRange sourceRange,
              IssueSeverity severity):
        Issue(sourceRange, severity), _scope(scope) {}

    /// \Returns the scope in which the issue occured
    Scope const* scope() const { return _scope; }

private:
    Scope const* _scope;
};

/// Base class of all statement related issues
class SCATHA_API BadStmt: public SemaIssue {
public:
    /// \Returns the erroneous statement
    ast::Statement const* statement() const { return stmt; }

protected:
    explicit BadStmt(Scope const* scope,
                     ast::Statement const* statement,
                     IssueSeverity severity);

private:
    ast::Statement const* stmt;
};

///
class SCATHA_API GenericBadStmt: public BadStmt {
public:
    enum Reason {
#define SC_SEMA_GENERICBADSTMT_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemanticIssuesNEW.def>
    };
    SC_SEMA_ISSUE_REASON()

    GenericBadStmt(Scope const* scope,
                   ast::Statement const* statement,
                   Reason reason);

private:
    void format(std::ostream& str) const override;
};

/// Base class of all declaration related issues
class SCATHA_API BadDecl: public BadStmt {
protected:
    explicit BadDecl(Scope const* scope,
                     ast::Declaration const* declaration,
                     IssueSeverity severity);

    SC_SEMA_DERIVED_STMT(Declaration, declaration)
};

/// Declaration of a name that is already defined in the same scope
class SCATHA_API Redefinition: public BadDecl {
public:
    Redefinition(Scope const* scope,
                 ast::Declaration const* declaration,
                 Entity const* existing):
        BadDecl(scope, declaration, IssueSeverity::Error),
        _existing(existing) {}

    /// \Returns the previous declaration of the same name
    Entity const* existing() const { return _existing; }

private:
    void format(std::ostream& str) const override;

    Entity const* _existing;
};

/// Bad declaration of variable or function parameter
class SCATHA_API BadVarDecl: public BadDecl {
public:
    enum Reason {
#define SC_SEMA_BADVARDECL_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemanticIssuesNEW.def>
    };
    SC_SEMA_ISSUE_REASON()

    BadVarDecl(Scope const* scope,
               ast::VarDeclBase const* vardecl,
               Reason reason,
               Type const* type = nullptr,
               ast::Expression const* initExpr = nullptr);

    SC_SEMA_DERIVED_STMT(VarDeclBase, declaration)

    ///
    Type const* type() const { return _type; }

    ///
    ast::Expression const* initExpr() const { return _initExpr; }

private:
    void format(std::ostream& str) const override;

    Type const* _type;
    ast::Expression const* _initExpr;
};

class SCATHA_API BadSMF: public BadDecl {
public:
    enum Reason {
#define SC_SEMA_BADSMF_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemanticIssuesNEW.def>
    };
    SC_SEMA_ISSUE_REASON()

    BadSMF(Scope const* scope,
           ast::FunctionDefinition const* funcdef,
           Reason reason,
           SpecialMemberFunction SMF,
           StructType const* parent);

    SC_SEMA_DERIVED_STMT(FunctionDefinition, definition)

    SpecialMemberFunction SMF() const { return smf; }

    StructType const* parent() const { return _parent; }

private:
    void format(std::ostream& str) const override;

    SpecialMemberFunction smf;
    StructType const* _parent;
};

///
class SCATHA_API BadReturnStmt: public BadStmt {
public:
    enum Reason {
#define SC_SEMA_BADRETURN_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemanticIssuesNEW.def>
    };
    SC_SEMA_ISSUE_REASON()

    SC_SEMA_DERIVED_STMT(ReturnStatement, statement)

    BadReturnStmt(Scope const* scope,
                  ast::ReturnStatement const* statement,
                  Reason reason);

private:
    void format(std::ostream& str) const override;
};

/// Error due to cyclic struct definition
class SCATHA_API StructDefCycle: public BadDecl {
public:
    StructDefCycle(Scope const* scope, std::vector<Entity const*> cycle);

    /// The definition cycle
    std::span<Entity const* const> cycle() const { return _cycle; }

private:
    void format(std::ostream& str) const override;

    std::vector<Entity const*> _cycle;
};

/// Base class of all statement related issues
class SCATHA_API BadExpr: public SemaIssue {
public:
    enum Reason {
#define SC_SEMA_BADEXPR_DEF(Type, Reason, Severity, Message) Reason,
#include <scatha/Sema/SemanticIssuesNEW.def>
    };
    SC_SEMA_ISSUE_REASON()

    explicit BadExpr(Scope const* scope,
                     ast::Expression const* expr,
                     Reason reason);

    /// \Returns the erroneous expression
    ast::Expression const* expr() const { return _expr; }

private:
    void format(std::ostream&) const override;

    ast::Expression const* _expr;
};

} // namespace scatha::sema

#undef SC_SEMA_ISSUE_REASON
#undef SC_SEMA_DERIVED_STMT

#endif // SCATHA_SEMA_SEMANTICISSUES_H_
