#include "IR/BinSerialize.h"

#include "Common/Base.h"

using namespace scatha;
using namespace ir;

void ir::serializeBinary(ir::Module const&, std::ostream&) {
    SC_UNIMPLEMENTED();
}

bool ir::deserializeBinary(ir::Context&, ir::Module&, std::istream&) {
    SC_UNIMPLEMENTED();
}
