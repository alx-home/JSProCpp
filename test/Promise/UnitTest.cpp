#include "../TestCommon.h"

#include <catch2/catch_test_macros.hpp>
#include <promise/core/Promise.h>
#include <coroutine>

TEST_CASE("Promise Destructor", "[Promise][UnitTest]") {
   auto promise = TestHelper::Create<int, false>();
   REQUIRE(true);  // Destructor at end of scope
}

TEST_CASE("Promise await_ready", "[Promise][UnitTest]") {
   auto promise = TestHelper::Create<int, false>();
   REQUIRE_NOTHROW(TestHelper::await_ready(*promise));
}

TEST_CASE("Promise await_suspend", "[Promise][UnitTest]") {
   auto                    promise = TestHelper::Create<int, false>();
   std::coroutine_handle<> handle{};
   REQUIRE_NOTHROW(TestHelper::await_suspend(*promise, handle));
}

TEST_CASE("Promise await_resume", "[Promise][UnitTest]") {
   auto promise = TestHelper::Create<int, false>();
   REQUIRE_NOTHROW(TestHelper::await_resume(*promise));
}
