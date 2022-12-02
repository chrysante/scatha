#ifndef COMMON_STRUCTUREDSOURCE_H_
#define COMMON_STRUCTUREDSOURCE_H_

#include <string>
#include <string_view>

#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha {
	
class SCATHA(API) StructuredSource {
public:
    explicit StructuredSource(std::string_view source);
    
    std::string_view getLine(std::size_t index) const {
        SC_ASSERT(index < lines.size(), "Index out of bounds.");
        return lines[index];
    }
    
private:
    utl::vector<std::string> lines;
};
	
}

#endif // COMMON_STRUCTUREDSOURCE_H_

