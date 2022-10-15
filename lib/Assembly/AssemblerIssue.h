#ifndef SCATHA_CODEGEN_ASSEMBLERERROR_H_
#define SCATHA_CODEGEN_ASSEMBLERERROR_H_

#include <stdexcept>

#include "Assembly/Assembly.h"
#include "Basic/Basic.h"

namespace scatha::assembly {

class SCATHA(API) AssemblerIssue: public std::runtime_error {
public:
    explicit AssemblerIssue(std::string const&, size_t line);

    size_t line() const { return _line; }

private:
    size_t _line;
};

class SCATHA(API) UnexpectedElement: public AssemblerIssue {
public:
    using AssemblerIssue::AssemblerIssue;
    explicit UnexpectedElement(Element const&, size_t line);
};

class SCATHA(API) InvalidArguments: public UnexpectedElement {
public:
    explicit InvalidArguments(Instruction, Element const& a, Element const& b, size_t line);
};

class SCATHA(API) InvalidMarker: public AssemblerIssue {
public:
    explicit InvalidMarker(Marker, size_t line);
};

class SCATHA(API) UseOfUndeclaredLabel: public AssemblerIssue {
public:
    explicit UseOfUndeclaredLabel(Label, size_t line);

    Label label() const { return _label; }

private:
    Label _label;
};

} // namespace scatha::assembly

#endif // SCATHA_CODEGEN_ASSEMBLERERROR_H_
