#include "../TestCommon.h"

TEST_CASE("String promises cover pending and done branches", "[Promise][StringCoverage]") {
   RunWithTimeout(2s, [&] {
      auto done_then = Promise<std::string>::Resolve("a").Then([](std::string const& value) {
         return value + "b";
      });
      REQUIRE(done_then.Resolved());
      REQUIRE(done_then.Value() == "ab");

      auto done_catch_passthrough = Promise<std::string>::Resolve("ok").Catch([](TestError const&) {
         return std::string{"x"};
      });
      REQUIRE(done_catch_passthrough.Resolved());
      REQUIRE(done_catch_passthrough.Value() == "ok");

      auto done_recover =
        Promise<std::string>::Reject<TestError>("boom").Catch([](TestError const& err) {
           return std::string{err.what()} + "!";
        });
      REQUIRE(done_recover.Resolved());
      REQUIRE(done_recover.Value() == "boom!");

      bool done_finally_called = false;
      auto done_finally =
        Promise<std::string>::Resolve("f").Finally([&] { done_finally_called = true; });
      REQUIRE(done_finally.Resolved());
      REQUIRE(done_finally.Value() == "f");
      REQUIRE(done_finally_called);

      auto [pending, resolve, reject] = promise::Create<std::string>();
      auto pending_then    = pending.Then([](std::string const& value) { return value + "-then"; });
      auto pending_catch   = pending.Catch([](TestError const&) { return std::string{"catch"}; });
      auto pending_finally = pending.Finally([] {});

      pending.WaitAwaited(0);
      REQUIRE((*resolve)("pending"));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(pending_then.Resolved());
      REQUIRE(pending_then.Value() == "pending-then");
      REQUIRE(pending_catch.Resolved());
      REQUIRE(pending_catch.Value() == "pending");
      REQUIRE(pending_finally.Resolved());
      REQUIRE(pending_finally.Value() == "pending");
   });
}

TEST_CASE("String promises cover rejection propagation branches", "[Promise][StringCoverage]") {
   RunWithTimeout(2s, [&] {
      auto [pending, resolve, reject] = promise::Create<std::string>();

      auto then_passthrough =
        pending.Then([](std::string const& value) { return value + " never"; });
      auto catch_recover = pending.Catch([](TestError const&) { return std::string{"recovered"}; });
      auto finally_keep  = pending.Finally([] {});

      pending.WaitAwaited(0);
      REQUIRE((*reject)(std::make_exception_ptr(TestError{"string reject"})));
      REQUIRE_FALSE((*resolve)("ignored"));

      REQUIRE(then_passthrough.Rejected());
      RequireException<TestError>(then_passthrough.Exception());

      REQUIRE(catch_recover.Resolved());
      REQUIRE(catch_recover.Value() == "recovered");

      REQUIRE(finally_keep.Rejected());
      RequireException<TestError>(finally_keep.Exception());
   });
}

TEST_CASE("Resolver-backed string promises and race paths", "[Promise][StringCoverage]") {
   RunWithTimeout(2s, [&] {
      auto resolver_ok = MakePromise(
        [](Resolve<std::string> const& resolve, Reject const&) -> Promise<std::string, true> {
           REQUIRE(resolve("resolver"));
           co_return;
        }
      );
      REQUIRE(resolver_ok.Resolved());
      REQUIRE(resolver_ok.Value() == "resolver");

      auto resolver_reject = MakePromise(
        [](Resolve<std::string> const&, Reject const& reject) -> Promise<std::string, true> {
           reject.Apply<TestError>("resolver fail");
           co_return;
        }
      );
      REQUIRE(resolver_reject.Rejected());
      RequireException<TestError>(resolver_reject.Exception());

      auto [slow, resolve_slow, reject_slow] = promise::Create<std::string>();
      auto race_done = promise::Race(Promise<std::string>::Resolve("winner"), slow);
      REQUIRE(race_done.Resolved());
      REQUIRE(race_done.Value() == "winner");
      REQUIRE((*resolve_slow)("late"));
      REQUIRE_FALSE((*reject_slow)(std::make_exception_ptr(TestError{"ignored"})));

      auto [slow2, resolve_slow2, reject_slow2] = promise::Create<std::string>();
      auto race_reject =
        promise::Race(Promise<std::string>::Reject<TestError>("fast reject"), slow2);
      REQUIRE(race_reject.Rejected());
      RequireException<TestError>(race_reject.Exception());
      REQUIRE((*resolve_slow2)("late"));
      REQUIRE_FALSE((*reject_slow2)(std::make_exception_ptr(TestError{"late reject"})));
   });
}

TEST_CASE("String WPromise co_await and erased awaitable paths", "[Promise][StringCoverage]") {
   RunWithTimeout(2s, [&] {
      auto source = WPromise{[]() -> Promise<std::string> { co_return "awaited"; }};
      auto waiter = WPromise{[&]() -> Promise<std::string> {
         auto value = co_await source;
         co_return value + "-ok";
      }};
      REQUIRE(waiter.Resolved());
      REQUIRE(waiter.Value() == "awaited-ok");

      auto rejected_source = WPromise{[]() -> Promise<std::string> {
         throw TestError{"await reject"};
         co_return "";
      }};
      auto waiter_reject   = WPromise{[&]() -> Promise<std::string> {
         try {
            (void)(co_await rejected_source);
            co_return "bad";
         } catch (TestError const&) {
            co_return "caught";
         }
      }};
      REQUIRE(waiter_reject.Resolved());
      REQUIRE(waiter_reject.Value() == "caught");

      auto  erased_ptr = WPromise<std::string>::Resolve("erased").ToPointer<promise::VPromise>();
      auto& aw         = erased_ptr->VAwait();
      REQUIRE(aw.await_ready());
      REQUIRE_FALSE(aw.await_suspend(std::noop_coroutine()));
      REQUIRE_NOTHROW(aw.await_resume());
   });
}
