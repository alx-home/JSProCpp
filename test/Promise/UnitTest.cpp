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

TEST_CASE("Promise Then covers done and pending branches", "[Promise][UnitTest]") {
   RunWithTimeout(2s, [&] {
      auto done = TestHelper::Create<int, false>();

      auto then_value = TestHelper::Then(*done, [](int value) { return value + 4; });
      REQUIRE(then_value.Resolved());
      REQUIRE(then_value.Value() == 4);

      auto then_void = TestHelper::Then(*done, [](int) {});
      REQUIRE(then_void.Resolved());

      auto then_throw = TestHelper::Then(*done, [](int) -> int { throw TestError{"then throw"}; });
      REQUIRE(then_throw.Rejected());
      RequireException<TestError>(then_throw.Exception());

      auto then_promise =
        TestHelper::Then(*done, [](int value) -> Promise<int> { co_return value + 8; });
      then_promise.WaitDone();
      REQUIRE(then_promise.Resolved());
      REQUIRE(then_promise.Value() == 8);

      auto pending = TestHelper::CreatePending<int, true>();
      auto chained = TestHelper::Then(*pending, [](int value) { return value * 2; });
      REQUIRE(TestHelper::Resolve(*pending, 21));
      chained.WaitDone();
      REQUIRE(chained.Resolved());
      REQUIRE(chained.Value() == 42);

      auto pending_reject = TestHelper::CreatePending<int, true>();
      auto rejected_chain = TestHelper::Then(*pending_reject, [](int value) { return value + 1; });
      REQUIRE(
        TestHelper::Reject(*pending_reject, std::make_exception_ptr(TestError{"then reject"}))
      );
      rejected_chain.WaitDone();
      REQUIRE(rejected_chain.Rejected());
      RequireException<TestError>(rejected_chain.Exception());
   });
}

TEST_CASE("Promise Catch covers typed and exception_ptr handlers", "[Promise][UnitTest]") {
   RunWithTimeout(2s, [&] {
      auto rejected = TestHelper::CreatePending<int, true>();
      REQUIRE(TestHelper::Reject(*rejected, std::make_exception_ptr(TestError{"catch typed"})));

      auto catch_typed = TestHelper::Catch(*rejected, [](TestError const&) { return 15; });
      catch_typed.WaitDone();
      REQUIRE(catch_typed.Resolved());
      REQUIRE(catch_typed.Value() == 15);

      auto catch_ptr = TestHelper::Catch(*rejected, [](std::exception_ptr const& ex) {
         RequireException<TestError>(ex);
         return 17;
      });
      catch_ptr.WaitDone();
      REQUIRE(catch_ptr.Resolved());
      REQUIRE(catch_ptr.Value() == 17);

      auto catch_promise = TestHelper::Catch(
        *rejected, [](std::exception_ptr const&) -> Promise<int> { co_return 19; }
      );
      catch_promise.WaitDone();
      REQUIRE(catch_promise.Resolved());
      REQUIRE(catch_promise.Value() == 19);

      auto resolved = TestHelper::Create<int, false>();
      auto pass_through =
        TestHelper::Catch(*resolved, [](std::exception_ptr const&) { return 99; });
      REQUIRE(pass_through.Resolved());
      REQUIRE(pass_through.Value() == 0);
   });
}

TEST_CASE("Promise Finally covers success and failure continuations", "[Promise][UnitTest]") {
   RunWithTimeout(2s, [&] {
      bool finally_called = false;
      auto done           = TestHelper::Create<int, false>();

      auto final_value = TestHelper::Finally(*done, [&finally_called] { finally_called = true; });
      REQUIRE(final_value.Resolved());
      REQUIRE(final_value.Value() == 0);
      REQUIRE(finally_called);

      auto rejected = TestHelper::CreatePending<int, true>();
      REQUIRE(TestHelper::Reject(*rejected, std::make_exception_ptr(TestError{"finally reject"})));

      auto keeps_reject = TestHelper::Finally(*rejected, [] {});
      keeps_reject.WaitDone();
      REQUIRE(keeps_reject.Rejected());
      RequireException<TestError>(keeps_reject.Exception());

      auto override_reject =
        TestHelper::Finally(*rejected, [] { throw FinallyError{"finally throw"}; });
      override_reject.WaitDone();
      REQUIRE(override_reject.Rejected());
      RequireException<FinallyError>(override_reject.Exception());

      auto promise_final = TestHelper::Finally(*done, []() -> Promise<void> { co_return; });
      promise_final.WaitDone();
      REQUIRE(promise_final.Resolved());
      REQUIRE(promise_final.Value() == 0);
   });
}
