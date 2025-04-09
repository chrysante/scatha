#include "VirtualPointer.h"

#include <ostream>

using namespace svm;

std::ostream& svm::operator<<(std::ostream& ostream, VirtualPointer ptr) {
    return ostream << "{ " << ptr.slotIndex << " : " << ptr.offset << " }";
}
