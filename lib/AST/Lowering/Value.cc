#include "AST/Lowering/Value.h"

using namespace scatha;
using namespace ast;

std::string_view ast::toString(ValueLocation VL) {
    using enum ValueLocation;
    switch (VL) {
    case Register:
        return "Register";
    case Memory:
        return "Memory";
    }
}

std::ostream& ast::operator<<(std::ostream& ostream, ValueLocation VL) {
    return ostream << toString(VL);
}
