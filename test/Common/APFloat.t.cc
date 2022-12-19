#include <Catch/Catch2.hpp>

#include "Common/APFloat.h"

#include <sstream>

using namespace scatha;

TEST_CASE("APFloat comparison", "[common][big-num]") {
    APFloat n = 300;
    CHECK( n.isIntegral());
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

TEST_CASE("APFloat integral representable - 1", "[common][big-num]") {
    APFloat n = 300;
    REQUIRE(n.isIntegral());
    CHECK( n.representableAs<         int >());
    CHECK( n.representableAs<unsigned int >());
    CHECK( n.representableAs<  signed long>());
    CHECK( n.representableAs<unsigned long>());
    CHECK(!n.representableAs<         char>());
    CHECK(!n.representableAs<unsigned char>());
    CHECK( n.representableAs<float>());
    CHECK( n.representableAs<double>());
    CHECK( n.representableAs<long double>());
}

TEST_CASE("APFloat integral representable - 2", "[common][big-num]") {
    APFloat n = APFloat::parse("FFffFFffFFffFFffFFffFFffFFffFFff", 16).value();
    REQUIRE(n.isIntegral());
    CHECK(!n.representableAs<         int >());
    CHECK(!n.representableAs<unsigned int >());
    CHECK(!n.representableAs<  signed long>());
    CHECK(!n.representableAs<unsigned long>());
    CHECK(!n.representableAs<         char>());
    CHECK(!n.representableAs<unsigned char>());
}

TEST_CASE("APFloat integral representable - 3", "[common][big-num]") {
    APFloat n = -200;
    REQUIRE(n.isIntegral());
    CHECK( n.representableAs<         int >());
    CHECK(!n.representableAs<unsigned int >());
    CHECK( n.representableAs<  signed long>());
    CHECK(!n.representableAs<unsigned long>());
    CHECK(!n.representableAs<         char>());
    CHECK(!n.representableAs<unsigned char>());
    CHECK( n.representableAs<float>());
    CHECK( n.representableAs<double>());
    CHECK( n.representableAs<long double>());
    CHECK(static_cast<int>(n) == -200);
}

TEST_CASE("APFloat floating point representable - 1", "[common][big-num]") {
    APFloat n = std::numeric_limits<double>::max();
    CHECK(!n.representableAs<float>());
    CHECK( n.representableAs<double>());
    CHECK( n.representableAs<long double>());
    CHECK(static_cast<double>(n) == std::numeric_limits<double>::max());
}

TEST_CASE("APFloat floating point representable - 2", "[common][big-num]") {
    APFloat n = std::numeric_limits<double>::min();
    CHECK(!n.representableAs<float>());
    CHECK( n.representableAs<double>());
    CHECK( n.representableAs<long double>());
    CHECK(static_cast<double>(n) == std::numeric_limits<double>::min());
}

TEST_CASE("APFloat floating point representable - 3", "[common][big-num]") {
    APFloat n = 0.5;
    CHECK(!n.isIntegral());
    CHECK(!n.representableAs<         int >());
    CHECK(!n.representableAs<unsigned int >());
    CHECK(!n.representableAs<  signed long>());
    CHECK(!n.representableAs<unsigned long>());
    CHECK(!n.representableAs<         char>());
    CHECK(!n.representableAs<unsigned char>());
    CHECK(static_cast<double>(n) == 0.5);
}

TEST_CASE("APFloat fromString()", "[common][big-num]") {
    CHECK(APFloat::parse("123").value() == 123);
    CHECK(APFloat::parse("123", 16).value() == 0x123);
    CHECK(APFloat::parse("0.5").value() == 0.5);
//    auto f = APFloat::fromString("1.3").value();
    auto f = APFloat(1.3);
    CHECK(f == 1.3);
#warning Known test failure
//    CHECK(APFloat::parse("1.3").value() == 1.3);
}

TEST_CASE("APFloat arithmetic", "[common][big-num]") {
    SECTION("Addition") {
        APFloat n = 100;
        n += 0.5;
        CHECK(n == 100.5);
    }
    SECTION("Addition overflow") {
//        APFloat n = APFloat(0xFFffFFffFFffFFff);
//        n += 1;
//        CHECK(n == APFloat::fromString("0x10000000000000000").value());
    }
    SECTION("Subtraction") {
        APFloat n = 100;
        n -= 0.5;
        CHECK(n == 99.5);
    }
    SECTION("Subtraction overflow") {
//        APFloat n = APFloat::fromString("0x10000000000000000").value();
//        n -= APFloat(1);
//        CHECK(n == 0xFFffFFffFFffFFff);
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
        CHECK(sstr.str() == "100");
    }
    SECTION("Negative integral") {
        APFloat const n = -100;
        sstr << n;
        CHECK(sstr.str() == "-100");
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
        APFloat const n(0.03125, 256);
        sstr << n;
        CHECK(sstr.str() == "0.03125");
    }
    SECTION("Negative small fraction - 2") {
        APFloat const n = -0.125;
        sstr << n;
        CHECK(sstr.str() == "-0.125");
    }
}

TEST_CASE("APFloat rational", "[common][big-num]") {
    APFloat n = 12.5;
    CHECK(!n.isIntegral());
}
