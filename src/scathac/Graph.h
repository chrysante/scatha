#ifndef SCATHAC_GRAPH_H_
#define SCATHAC_GRAPH_H_

#include "Options.h"

namespace scatha {

/// Command line options for the `graph` debug tool
struct GraphOptions: BaseOptions {
    std::filesystem::path dest;
    bool generatesvg;
    bool open;
    bool cfg;
    bool calls;
    bool interference;
    bool selectiondag;
};

/// Main function of the `graph` tool
int graphMain(GraphOptions options);

} // namespace scatha

#endif // SCATHAC_GRAPH_H_
