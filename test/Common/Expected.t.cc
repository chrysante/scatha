#include <Catch/Catch2.hpp>

#include "Common/Expected.h"

namespace {

struct MyError {
    explicit MyError(int i): i(i) {}
    int value() const { return i; }
    MyError(MyError const&)     = default;
    MyError(MyError&&) noexcept = default;
    ~MyError() { dtorRun = true; }

    static bool dtorRun;

private:
    int i;
};

bool MyError::dtorRun = false;

} // namespace

TEST_CASE("Expected", "[common]") {
    auto f = [](bool b) -> scatha::Expected<int, MyError> {
        if (b) {
            return 0;
        }
        return MyError(1);
    };
    auto a = f(true);
    CHECK(a);
    CHECK(*a == 0);
    {
        auto b = f(false);
        CHECK(!b);
        CHECK(b.error().value() == 1);
        MyError::dtorRun = false;
    }
    CHECK(MyError::dtorRun);
}

TEST_CASE("Expected<void>", "[common]") {
    auto f = [](bool b) -> scatha::Expected<void, MyError> {
        if (b) {
            return {};
        }
        return MyError(1);
    };
    auto a = f(true);
    CHECK(a);
    {
        auto b = f(false);
        CHECK(!b);
        CHECK(b.error().value() == 1);
        MyError::dtorRun = false;
    }
    CHECK(MyError::dtorRun);
}

TEST_CASE("Expected Reference", "[common]") {
    auto f = [](bool b, int& x) -> scatha::Expected<int&, MyError> {
        if (b) {
            return x;
        }
        return MyError(1);
    };
    int i  = 10;
    auto a = f(true, i);
    CHECK(std::is_same_v<decltype(*a), int&>);
    CHECK(a);
    ++*a;
    CHECK(i == 11);
    {
        int j  = 1;
        auto b = f(false, j);
        CHECK(!b);
        CHECK(b.error().value() == 1);
        MyError::dtorRun = false;
    }
    CHECK(MyError::dtorRun);
    auto const c = f(true, i);
    CHECK(std::is_same_v<decltype(*c), int&>);
}

TEST_CASE("Expected Const Reference", "[common]") {
    auto f = [](bool b, int& x) -> scatha::Expected<int const&, MyError> {
        if (b) {
            return x;
        }
        return MyError(1);
    };
    int i  = 10;
    auto a = f(true, i); // Deliberately not const
    CHECK(std::is_same_v<decltype(*a), int const&>);
    CHECK(a);
    ++i;
    CHECK(*a == 11);
    {
        int j  = 1;
        auto b = f(false, j);
        CHECK(!b);
        CHECK(b.error().value() == 1);
        MyError::dtorRun = false;
    }
    CHECK(MyError::dtorRun);
}
