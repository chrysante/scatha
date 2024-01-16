#include "IRGen/Value.h"

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
