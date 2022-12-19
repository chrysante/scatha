#include <Catch/Catch2.hpp>

#include "Common/APFloat.h"

#include <sstream>

using namespace scatha;

TEST_CASE("APFloat comparison", "[common][big-num]") {
    APFloat n = 300;
    CHECK(n == APFloat(300));
    CHECK(n > APFloat(0));
    CHECK(n < APFloat(1000));
    CHECK(n == 300);
    CHECK(n > 0);
    CHECK(n < 1000);
    CHECK(300 == n);
    CHECK(0 < n);
    CHECK(1000 > n);
}

TEST_CASE("APFloat floating point representable - 1", "[common][big-num]") {
    double const base = 1.3;
    APFloat const f = base;
    CHECK(static_cast<float>(f)       != base);
    CHECK(static_cast<double>(f)      == base);
    CHECK(static_cast<long double>(f) == base);
}

TEST_CASE("APFloat floating point representable - 2", "[common][big-num]") {
    double const base = std::numeric_limits<double>::min();
    APFloat const f = base;
    CHECK(static_cast<float>(f)       != base);
    CHECK(static_cast<double>(f)      == base);
    CHECK(static_cast<long double>(f) == base);
}

TEST_CASE("APFloat floating point representable - 3", "[common][big-num]") {
    double const base = std::numeric_limits<double>::max();
    APFloat const f = base;
    CHECK(static_cast<float>(f)       != base);
    CHECK(static_cast<double>(f)      == base);
    CHECK(static_cast<long double>(f) == base);
}

TEST_CASE("APFloat fromString()", "[common][big-num]") {
    CHECK(APFloat::parse("123").value() == 123);
    CHECK(APFloat::parse("123", 16).value() == 0x123);
    CHECK(APFloat::parse("0.5").value() == 0.5);
    CHECK(APFloat::parse("1.3").value() == 1.3);
}

TEST_CASE("APFloat arithmetic", "[common][big-num]") {
    SECTION("Addition") {
        APFloat n = 100;
        n += 0.5;
        CHECK(n == 100.5);
    }
    SECTION("Subtraction") {
        APFloat n = 100;
        n -= 0.5;
        CHECK(n == 99.5);
    }
    SECTION("Multiplication") {
        APFloat n = 2;
        n *= 0.25;
        CHECK(n == 0.5);
    }
    SECTION("Division") {
        APFloat n = 1;
        n /= 2;
        CHECK(n == 0.5);
    }
}

TEST_CASE("APFloat formatting", "[common][big-num]") {
    std::stringstream sstr;
    SECTION("Positive integral") {
        APFloat const n = 100;
        sstr << n;
        CHECK(sstr.str() == "100.0");
    }
    SECTION("Negative integral") {
        APFloat const n = -100.0;
        sstr << n;
        CHECK(sstr.str() == "-100.0");
    }
    SECTION("Positive fraction") {
        APFloat const n = 12.5;
        sstr << n;
        CHECK(sstr.str() == "12.5");
    }
    SECTION("Negative fraction") {
        APFloat const n = -12.5;
        sstr << n;
        CHECK(sstr.str() == "-12.5");
    }
    SECTION("Negative small fraction - 1") {
        APFloat const n(0.03125);
        sstr << n;
        CHECK(sstr.str() == "0.03125");
    }
    SECTION("Negative small fraction - 2") {
        APFloat const n = -0.125;
        sstr << n;
        CHECK(sstr.str() == "-0.125");
    }
}
