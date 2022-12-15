#include <Catch/Catch2.hpp>

#include "Common/BigNum.h"

#include <sstream>

using namespace scatha;

TEST_CASE("BigNum comparison", "[common][big-num]") {
    BigNum n = 300;
    CHECK( n.isIntegral());
    CHECK(n == BigNum(300));
    CHECK(n > BigNum(0));
    CHECK(n < BigNum(1000));
    CHECK(n == 300);
    CHECK(n > 0);
    CHECK(n < 1000);
    CHECK(300 == n);
    CHECK(0 < n);
    CHECK(1000 > n);
}

TEST_CASE("BigNum integral representable - 1", "[common][big-num]") {
    BigNum n = 300;
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

TEST_CASE("BigNum integral representable - 2", "[common][big-num]") {
    BigNum n = BigNum::fromString("FFffFFffFFffFFffFFffFFffFFffFFff", 16).value();
    REQUIRE(n.isIntegral());
    CHECK(!n.representableAs<         int >());
    CHECK(!n.representableAs<unsigned int >());
    CHECK(!n.representableAs<  signed long>());
    CHECK(!n.representableAs<unsigned long>());
    CHECK(!n.representableAs<         char>());
    CHECK(!n.representableAs<unsigned char>());
}

TEST_CASE("BigNum integral representable - 3", "[common][big-num]") {
    BigNum n = -200;
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

TEST_CASE("BigNum floating point representable - 1", "[common][big-num]") {
    BigNum n = std::numeric_limits<double>::max();
    CHECK(!n.representableAs<float>());
    CHECK( n.representableAs<double>());
    CHECK( n.representableAs<long double>());
    CHECK(static_cast<double>(n) == std::numeric_limits<double>::max());
}

TEST_CASE("BigNum floating point representable - 2", "[common][big-num]") {
    BigNum n = std::numeric_limits<double>::min();
    CHECK(!n.representableAs<float>());
    CHECK( n.representableAs<double>());
    CHECK( n.representableAs<long double>());
    CHECK(static_cast<double>(n) == std::numeric_limits<double>::min());
}

TEST_CASE("BigNum floating point representable - 3", "[common][big-num]") {
    BigNum n = 0.5;
    CHECK(!n.isIntegral());
    CHECK(!n.representableAs<         int >());
    CHECK(!n.representableAs<unsigned int >());
    CHECK(!n.representableAs<  signed long>());
    CHECK(!n.representableAs<unsigned long>());
    CHECK(!n.representableAs<         char>());
    CHECK(!n.representableAs<unsigned char>());
    CHECK(static_cast<double>(n) == 0.5);
}

TEST_CASE("BigNum fromString()", "[common][big-num]") {
    CHECK(BigNum::fromString("123").value() == 123);
    CHECK(BigNum::fromString("123", 16).value() == 0x123);
    CHECK(BigNum::fromString("0.5").value() == 0.5);
    CHECK(BigNum::fromString("1.3").value() == 1.3);
}

TEST_CASE("BigNum arithmetic", "[common][big-num]") {
    SECTION("Addition") {
        BigNum n = 100;
        n += 0.5;
        CHECK(n == 100.5);
    }
    SECTION("Addition overflow") {
        BigNum n = BigNum(0xFFffFFffFFffFFff);
        n += 1;
        CHECK(n == BigNum::fromString("0x10000000000000000").value());
    }
    SECTION("Subtraction") {
        BigNum n = 100;
        n -= 0.5;
        CHECK(n == 99.5);
    }
    SECTION("Subtraction overflow") {
        BigNum n = BigNum::fromString("0x10000000000000000").value();
        n -= BigNum(1);
        CHECK(n == 0xFFffFFffFFffFFff);
    }
    SECTION("Multiplication") {
        BigNum n = 2;
        n *= 0.25;
        CHECK(n == 0.5);
    }
    SECTION("Division") {
        BigNum n = 1;
        n /= 2;
        CHECK(n == 0.5);
    }
}

TEST_CASE("BigNum formatting", "[common][big-num]") {
    std::stringstream sstr;
    SECTION("Positive integral") {
        BigNum const n = 100;
        sstr << n;
        CHECK(sstr.str() == "100");
    }
    SECTION("Negative integral") {
        BigNum const n = -100;
        sstr << n;
        CHECK(sstr.str() == "-100");
    }
    SECTION("Positive fraction") {
        BigNum const n = 12.5;
        sstr << n;
        CHECK(sstr.str() == "12.5");
    }
    SECTION("Negative fraction") {
        BigNum const n = -12.5;
        sstr << n;
        CHECK(sstr.str() == "-12.5");
    }
}

TEST_CASE("BigNum rational", "[common][big-num]") {
    BigNum n = 12.5;
    CHECK(!n.isIntegral());
}
