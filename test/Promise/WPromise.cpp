#include "../TestCommon.h"

TEST_CASE("WPromise static helpers and detach APIs", "[Promise][WPromise]") {
   RunWithTimeout(2s, [&] {
      auto resolved = WPromise<int>::Resolve(9);
      REQUIRE(resolved.Resolved());
      REQUIRE(resolved.Value() == 9);

      auto rejected = WPromise<int>::Reject<TestError>("wreject");
      REQUIRE(rejected.Rejected());
      RequireException<TestError>(rejected.Exception());

      auto [pending, resolve, reject] = WPromise<int>::Create();
      REQUIRE_FALSE(pending.Done());
      REQUIRE((*resolve)(12));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending.Resolved());
      REQUIRE(pending.Value() == 12);

      auto detached = WPromise{[]() -> Promise<void> { co_return; }};
      std::move(detached).Detach();

      auto vdetached = WPromise{[]() -> Promise<void> { co_return; }};
      std::move(vdetached).VDetach();
   });
}

TEST_CASE("WPromise VAwait and co_await adapters", "[Promise][WPromise]") {
   RunWithTimeout(2s, [&] {
      auto as_vpromise = WPromise<int>::Resolve(17);
      auto pointer     = std::move(as_vpromise).ToPointer<promise::VPromise>();

      auto& erased_awaitable = pointer->VAwait();
      REQUIRE(erased_awaitable.await_ready());
      REQUIRE_FALSE(erased_awaitable.await_suspend(std::noop_coroutine()));
      REQUIRE_NOTHROW(erased_awaitable.await_resume());

      auto typed = WPromise<int>::Resolve(23);
      auto aw1   = typed.operator co_await();
      REQUIRE(aw1.await_ready());
      REQUIRE(aw1.await_resume() == 23);

      auto aw2 = operator co_await(typed);
      REQUIRE(aw2.await_ready());
      REQUIRE(aw2.await_resume() == 23);
   });
}

TEST_CASE("WPromise rvalue Then Catch Finally paths", "[Promise][WPromise]") {
   RunWithTimeout(2s, [&] {
      auto value_source = WPromise<int>::Resolve(4);
      auto then_chain   = std::move(value_source).Then([](int value) { return value + 6; });
      REQUIRE(then_chain.Resolved());
      REQUIRE(then_chain.Value() == 10);

      auto reject_source = WPromise<int>::Reject<TestError>("rvalue catch");
      auto catch_chain   = std::move(reject_source).Catch([](TestError const&) { return 99; });
      REQUIRE(catch_chain.Resolved());
      REQUIRE(catch_chain.Value() == 99);

      bool finally_called = false;
      auto finally_source = WPromise<int>::Resolve(12);
      auto finally_chain =
        std::move(finally_source).Finally([&finally_called] { finally_called = true; });
      REQUIRE(finally_chain.Resolved());
      REQUIRE(finally_chain.Value() == 12);
      REQUIRE(finally_called);
   });
}
