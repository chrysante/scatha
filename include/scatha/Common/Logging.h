#ifndef SCATHA_COMMON_LOGGING_H_
#define SCATHA_COMMON_LOGGING_H_

#include <iosfwd>
#include <string_view>

namespace scatha::logging {

void line(std::string_view msg = {});

void line(std::string_view msg, std::ostream& ostream);

void header(std::string_view title);

void header(std::string_view title, std::ostream& ostream);

void subHeader(std::string_view title = {});

void subHeader(std::string_view title, std::ostream& ostream);

} // namespace scatha::logging

#endif // SCATHA_COMMON_LOGGING_H_
