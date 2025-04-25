#ifndef SCATHA_UTIL_COUTREROUTER_H_
#define SCATHA_UTIL_COUTREROUTER_H_

#include <iosfwd>
#include <sstream>
#include <string>

namespace scatha::test {

class CoutRerouter {
public:
    CoutRerouter();
    ~CoutRerouter();

    void reset();

    std::string str() const;

private:
    std::ostringstream sstr;
    std::streambuf* saved = nullptr;
};

} // namespace scatha::test

#endif // SCATHA_UTIL_COUTREROUTER_H_
