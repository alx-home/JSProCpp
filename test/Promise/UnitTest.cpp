#include "../TestCommon.h"

#include <catch2/catch_test_macros.hpp>
#include <promise/core/Promise.h>
#include <coroutine>

TEST_CASE("Promise Destructor", "[Promise][UnitTest]") {
   RunWithTimeout(2s, [&] {
      auto promise = TestHelper::Create<int, false>();
      REQUIRE(true);  // Destructor at end of scope
   });
}

TEST_CASE("Promise await_ready", "[Promise][UnitTest]") {
   RunWithTimeout(2s, [&] {
      auto promise = TestHelper::Create<int, false>();
      REQUIRE_NOTHROW(TestHelper::await_ready(*promise));
   });
}

TEST_CASE("Promise await_suspend", "[Promise][UnitTest]") {
   RunWithTimeout(2s, [&] {
      auto                    promise = TestHelper::Create<int, false>();
      std::coroutine_handle<> handle{};
      REQUIRE_NOTHROW(TestHelper::await_suspend(*promise, handle));
   });
}

TEST_CASE("Promise await_resume", "[Promise][UnitTest]") {
   RunWithTimeout(2s, [&] {
      auto promise = TestHelper::Create<int, false>();
      REQUIRE_NOTHROW(TestHelper::await_resume(*promise));
   });
}

TEST_CASE("Promise internal await function registration lifecycle", "[Promise][UnitTest]") {
   RunWithTimeout(2s, [&] {
      auto promise = TestHelper::CreatePending<int, true>();

      bool called = false;
      auto id     = TestHelper::RegisterAwaitFunction(*promise, [&called] { called = true; });

      REQUIRE(TestHelper::Awaiters(*promise) == 1);
      REQUIRE(TestHelper::UseCount(*promise) == 1);

      REQUIRE(TestHelper::UnregisterAwaitFunction(*promise, id));
      REQUIRE_FALSE(TestHelper::UnregisterAwaitFunction(*promise, id));
      REQUIRE(TestHelper::Awaiters(*promise) == 0);
      REQUIRE_FALSE(called);
   });
}

TEST_CASE("Promise await_suspend pending and done branches", "[Promise][UnitTest]") {
   RunWithTimeout(2s, [&] {
      auto pending = TestHelper::CreatePending<int, true>();
      REQUIRE(TestHelper::await_suspend(*pending, std::noop_coroutine()));
      REQUIRE(TestHelper::Awaiters(*pending) == 1);

      REQUIRE(TestHelper::Resolve(*pending, 42));
      REQUIRE(TestHelper::IsResolved(*pending));
      REQUIRE(TestHelper::Awaiters(*pending) == 0);

      auto done = TestHelper::Create<int, false>();
      REQUIRE_FALSE(TestHelper::await_suspend(*done, std::noop_coroutine()));
   });
}

TEST_CASE("Promise await_resume throws on rejected state", "[Promise][UnitTest]") {
   RunWithTimeout(2s, [&] {
      auto promise = TestHelper::CreatePending<int, true>();

      REQUIRE(TestHelper::Reject(*promise, std::make_exception_ptr(TestError{"resume reject"})));
      REQUIRE_THROWS_AS(TestHelper::await_resume(*promise), TestError);
   });
}
