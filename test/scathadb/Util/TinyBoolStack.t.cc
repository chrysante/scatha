#include <catch2/catch_template_test_macros.hpp>

#include <scathadb/Util/TinyBoolStack.h>

TEMPLATE_TEST_CASE("TinyBoolStack", "[util]", uint64_t, uint32_t, uint16_t,
                   uint8_t) {
    sdb::TinyBoolStack<TestType> stack;

    SECTION("New stack is empty") {
        CHECK(stack.empty());
        CHECK(stack.size() == 0);
    }

    SECTION("Push and pop single value") {
        stack.push(true);
        REQUIRE(!stack.empty());
        CHECK(stack.size() == 1);
        CHECK(stack.top() == true);

        stack.pop();
        CHECK(stack.empty());
        CHECK(stack.size() == 0);
    }

    SECTION("Push multiple values and check top") {
        stack.push(false);
        stack.push(true);
        stack.push(false);

        REQUIRE(stack.size() == 3);
        CHECK(stack.top() == false);
        stack.pop();

        CHECK(stack.top() == true);
        stack.pop();

        CHECK(stack.top() == false);
        stack.pop();

        CHECK(stack.empty());
    }

    SECTION("Pushing up to capacity succeeds") {
        for (size_t i = 0; i < stack.capacity(); ++i) {
            REQUIRE(!stack.full());
            stack.push(i % 2 == 0);
        }
        CHECK(stack.full());
        if (std::same_as<typename decltype(stack)::value_type, uint64_t>)
            CHECK(stack.size() >= 58);
    }

    SECTION("Pushing beyond capacity fails or throws") {
        for (size_t i = 0; i < stack.capacity(); ++i)
            stack.push(true);

        CHECK(stack.full());

        CHECK_THROWS_AS(stack.push(false), std::overflow_error);
    }

    SECTION("Pop all and check LIFO behavior") {
        for (size_t i = 0; i < stack.capacity(); ++i)
            stack.push(i % 2 == 1);

        for (int i = (int)stack.capacity() - 1; i >= 0; --i) {
            CHECK(stack.pop() == (i % 2 == 1));
        }
        CHECK(stack.empty());
    }

    SECTION("Pop from empty stack throws") {
        REQUIRE(stack.empty());
        CHECK_THROWS_AS(stack.pop(), std::underflow_error);
    }

    SECTION("Top on empty stack throws") {
        REQUIRE(stack.empty());
        CHECK_THROWS_AS(stack.top(), std::underflow_error);
    }
}
