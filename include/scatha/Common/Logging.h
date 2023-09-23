#ifndef SCATHA_COMMON_LOGGING_H_
#define SCATHA_COMMON_LOGGING_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Common/Base.h>

namespace scatha::logging {

void SCATHA_API line(std::string_view msg = {});

void SCATHA_API line(std::string_view msg, std::ostream& ostream);

void SCATHA_API header(std::string_view title);

void SCATHA_API header(std::string_view title, std::ostream& ostream);

void SCATHA_API subHeader(std::string_view title = {});

void SCATHA_API subHeader(std::string_view title, std::ostream& ostream);

} // namespace scatha::logging

#endif // SCATHA_COMMON_LOGGING_H_
