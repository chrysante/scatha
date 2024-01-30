#include <iostream>

#include <termfmt/termfmt.h>

#include "IR/Print.h"
#include "IRGen/Value.h"
#include "Sema/Entity.h"
#include "Sema/Format.h"

using namespace scatha;
using namespace irgen;

std::string_view irgen::toString(ValueLocation VL) {
    using enum ValueLocation;
    switch (VL) {
    case Register:
        return "Register";
    case Memory:
        return "Memory";
    }
}

std::ostream& irgen::operator<<(std::ostream& ostream, ValueLocation VL) {
    return ostream << toString(VL);
}

std::string_view irgen::toString(ValueRepresentation VR) {
    using enum ValueRepresentation;
    switch (VR) {
    case Packed:
        return "Packed";
    case Unpacked:
        return "Unpacked";
    }
}

std::ostream& irgen::operator<<(std::ostream& ostream, ValueRepresentation VR) {
    return ostream << toString(VR);
}

void irgen::print(Atom atom) { print(atom, std::cout); }

void irgen::print(Atom atom, std::ostream& str) { str << atom << "\n"; }

std::ostream& irgen::operator<<(std::ostream& str, Atom atom) {
    if (atom.get()) {
        ir::printDecl(*atom, str);
    }
    else {
        str << "NULL";
    }
    str << " " << tfmt::format(tfmt::BrightGrey, "[", atom.location(), "]");
    return str;
}

void irgen::print(Value const& value) { print(value, std::cout); }

void irgen::print(Value const& value, std::ostream& str) {
    str << value << "\n";
}

std::ostream& irgen::operator<<(std::ostream& str, Value const& value) {
    str << sema::format(value.type()) << ": (";
    for (bool first = true; auto atom: value.elements()) {
        if (!first) {
            str << ", ";
        }
        first = false;
        str << atom;
    }
    str << ") "
        << tfmt::format(tfmt::BrightGrey, "[", value.representation(), "]");
    return str;
}
