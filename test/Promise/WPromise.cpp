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

TEST_CASE("WPromise void adapters and rvalue chaining", "[Promise][WPromise]") {
   RunWithTimeout(2s, [&] {
      auto typed_void = WPromise<void>::Resolve();
      auto aw1        = typed_void.operator co_await();
      REQUIRE(aw1.await_ready());
      REQUIRE_NOTHROW(aw1.await_resume());

      auto aw2 = operator co_await(typed_void);
      REQUIRE(aw2.await_ready());
      REQUIRE_NOTHROW(aw2.await_resume());

      auto  as_vpromise = WPromise<void>::Resolve();
      auto  pointer     = std::move(as_vpromise).ToPointer<promise::VPromise>();
      auto& erased      = pointer->VAwait();
      REQUIRE(erased.await_ready());
      REQUIRE_FALSE(erased.await_suspend(std::noop_coroutine()));
      REQUIRE_NOTHROW(erased.await_resume());

      auto then_chain = WPromise<void>::Resolve().Then([] { return 5; });
      REQUIRE(then_chain.Resolved());
      REQUIRE(then_chain.Value() == 5);

      auto catch_chain =
        WPromise<void>::Reject<TestError>("void reject").Catch([](TestError const&) { return 11; });
      REQUIRE(catch_chain.Resolved());
      REQUIRE(catch_chain.Value() == 11);

      bool finally_called = false;
      auto finally_chain =
        WPromise<void>::Resolve().Finally([&finally_called] { finally_called = true; });
      REQUIRE(finally_chain.Resolved());
      REQUIRE(finally_called);
   });
}

TEST_CASE("WPromise awaiter counters and exception_ptr reject overload", "[Promise][WPromise]") {
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = WPromise<int>::Create();
      REQUIRE(source.Awaiters() == 0);
      REQUIRE(source.UseCount() == 0);

      auto chained = source.Then([](int value) { return value + 1; });
      source.WaitAwaited(0);
      REQUIRE(source.Awaiters() == 1);
      REQUIRE(source.UseCount() >= 1);

      REQUIRE((*resolve)(41));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(chained.Resolved());
      REQUIRE(chained.Value() == 42);
      REQUIRE(source.Awaiters() == 0);

      auto rejected = WPromise<int>::Reject(std::make_exception_ptr(TestError{"exception_ptr"}));
      REQUIRE(rejected.Rejected());
      RequireException<TestError>(rejected.Exception());
   });
}

SCENARIO(
  "WPromise visits resolver and non-resolver detail variants",
  "[Promise][WPromise][Scenario]"
) {
   GIVEN("a non-resolver coroutine promise") {
      RunWithTimeout(2s, [&] {
         auto non_resolver =
           MakePromise([](int value) -> Promise<int> { co_return value + 20; }, 2);

         REQUIRE(non_resolver.Done());
         REQUIRE(non_resolver.Resolved());
         REQUIRE_FALSE(non_resolver.Rejected());
         REQUIRE(non_resolver.Value() == 22);
         REQUIRE(non_resolver.Exception() == nullptr);

         auto then_chain = non_resolver.Then([](int value) { return value + 1; });
         REQUIRE(then_chain.Resolved());
         REQUIRE(then_chain.Value() == 23);

         auto catch_passthrough = non_resolver.Catch([](std::exception_ptr const&) { return 111; });
         REQUIRE(catch_passthrough.Resolved());
         REQUIRE(catch_passthrough.Value() == 22);

         bool final_called = false;
         auto final_chain  = non_resolver.Finally([&final_called] { final_called = true; });
         REQUIRE(final_chain.Resolved());
         REQUIRE(final_chain.Value() == 22);
         REQUIRE(final_called);
      });
   }

   GIVEN("a resolver-backed promise") {
      RunWithTimeout(2s, [&] {
         auto with_resolver = MakePromise(
           [](Resolve<int> const& resolve, Reject const&, int value) -> Promise<int, true> {
              REQUIRE(resolve(value + 30));
              co_return;
           },
           3
         );

         with_resolver.WaitDone();
         REQUIRE(with_resolver.Done());
         REQUIRE(with_resolver.Resolved());
         REQUIRE_FALSE(with_resolver.Rejected());
         REQUIRE(with_resolver.Value() == 33);
         REQUIRE(with_resolver.Exception() == nullptr);

         auto then_chain = std::move(with_resolver).Then([](int value) { return value + 1; });
         REQUIRE(then_chain.Resolved());
         REQUIRE(then_chain.Value() == 34);
      });
   }
}
