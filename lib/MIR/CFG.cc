#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

Function::Function(std::span<Parameter* const> params, std::string name):
    Value(NodeType::Function),
    _name(std::move(name)),
    params(params | ranges::views::transform([](auto* p) {
               return UniquePtr<Parameter>(p);
           }) |
           ranges::to<utl::small_vector<UniquePtr<Parameter>>>) {}
