#ifndef PLAYGROUND_UTIL_H_
#define PLAYGROUND_UTIL_H_

#include <string_view>

namespace playground {

void line(std::string_view msg = {});

void header(std::string_view title);

void subHeader(std::string_view title = {});

} // namespace playground

#endif // PLAYGROUND_UTIL_H_
