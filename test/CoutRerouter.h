#ifndef TEST_COUTREROUTER_H_
#define TEST_COUTREROUTER_H_

#include <iosfwd>
#include <sstream>
#include <string>

class CoutRerouter {
public:
    CoutRerouter();
    ~CoutRerouter();

    std::string str() const;

private:
    std::ostringstream sstr;
    std::streambuf* saved = nullptr;
};

#endif // TEST_COUTREROUTER_H_
