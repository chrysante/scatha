#include "Assembly/AssemblerIssue.h"

#include <utl/strcat.hpp>

namespace scatha::assembly {

AssemblerIssue::AssemblerIssue(std::string const &msg, size_t line):
    std::runtime_error(utl::strcat(msg, " Line: ", line)), _line(line) {}

UnexpectedElement::UnexpectedElement(Element const &elem, size_t line):
    AssemblerIssue(utl::strcat("Unexpected element: ", elem.marker(), "."), line) {}

InvalidArguments::InvalidArguments(Instruction i, Element const &a, Element const &b, size_t line):
    UnexpectedElement(utl::strcat("Invalid arguments for ", i, ": ", a.marker(), ", ", b.marker(), "."), line) {}

InvalidMarker::InvalidMarker(Marker m, size_t line): AssemblerIssue(utl::strcat("Invalid Marker: ", m, "."), line) {}

UseOfUndeclaredLabel::UseOfUndeclaredLabel(Label label, size_t line):
    AssemblerIssue(utl::strcat("Use of undeclared label: ", label, "."), line), _label(label) {}

} // namespace scatha::assembly
