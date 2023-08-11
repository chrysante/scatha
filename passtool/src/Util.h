#ifndef SCATHA_PASSTOOL_UTIL_H_
#define SCATHA_PASSTOOL_UTIL_H_

#include <string_view>

#include <utl/streammanip.hpp>

namespace scatha::passtool {

void line(std::string_view m);

void header(std::string_view title);

void subHeader(std::string_view title);

extern utl::vstreammanip<> const Warning;

extern utl::vstreammanip<> const Error;

} // namespace scatha::passtool

#endif // SCATHA_PASSTOOL_UTIL_H_
