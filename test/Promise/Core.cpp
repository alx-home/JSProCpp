#include "../TestCommon.h"

#include <catch2/catch_test_macros.hpp>
#include "promise/core/Promise.h"
#include <coroutine>
#include <exception>

TEST_CASE("Then covers done and pending void/value combinations", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto done_void_to_value = Promise<void>::Resolve().Then([] { return 7; });
      REQUIRE(done_void_to_value.Resolved());
      REQUIRE(done_void_to_value.Value() == 7);

      auto done_value_to_void =
        Promise<int>::Resolve(3).Then([](int value) { REQUIRE(value == 3); });
      REQUIRE(done_value_to_void.Resolved());

      auto [pending_void, resolve_void, reject_void] = promise::Create<void>();
      auto pending_void_to_value                     = pending_void.Then([] { return 11; });
      pending_void.WaitAwaited(0);
      REQUIRE((*resolve_void)());
      REQUIRE_FALSE((*reject_void)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending_void_to_value.Resolved());
      REQUIRE(pending_void_to_value.Value() == 11);
   });
}

TEST_CASE("Then promise-returning continuations cover pending paths", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [pending_value, resolve_value, reject_value] = promise::Create<int>();
      auto chained_async =
        pending_value.Then([](int value) -> Promise<int> { co_return value + 5; });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(10));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(chained_async.Resolved());
      REQUIRE(chained_async.Value() == 15);

      auto rejected_passthrough =
        Promise<int>::Reject<TestError>("then reject").Then([](int) -> Promise<int> {
           co_return 0;
        });
      REQUIRE(rejected_passthrough.Rejected());
      RequireException<TestError>(rejected_passthrough.Exception());
   });
}

TEST_CASE("Catch covers done and pending value forwarding", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto done_rejected_to_value =
        Promise<void>::Reject<TestError>("catch").Catch([](TestError const&) { return 9; });
      REQUIRE(done_rejected_to_value.Resolved());
      REQUIRE(done_rejected_to_value.Value() == 9);

      auto [pending_value, resolve_value, reject_value] = promise::Create<int>();
      auto passthrough = pending_value.Catch([](TestError const&) { return 0; });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(21));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(passthrough.Resolved());
      REQUIRE(passthrough.Value() == 21);
   });
}

TEST_CASE("Catch with async lvalue-ref exception handler path", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto recovered = Promise<int>::Reject<TestError>("lvalue async")
                         .Catch([](TestError const& exception) -> Promise<int> {
                            REQUIRE(std::string{exception.what()} == "lvalue async");
                            co_return 123;
                         });

      REQUIRE(recovered.Resolved());
      REQUIRE(recovered.Value() == 123);
   });
}

TEST_CASE("Finally covers lvalue and rvalue pending/done branches", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      bool done_cleanup_called = false;
      auto done_chain = Promise<int>::Resolve(4).Finally([&] { done_cleanup_called = true; });
      REQUIRE(done_chain.Resolved());
      REQUIRE(done_chain.Value() == 4);
      REQUIRE(done_cleanup_called);

      auto [pending_value, resolve_value, reject_value] = promise::Create<int>();
      bool pending_cleanup_called                       = false;
      auto pending_chain = pending_value.Finally([&] { pending_cleanup_called = true; });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(8));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending_chain.Resolved());
      REQUIRE(pending_chain.Value() == 8);
      REQUIRE(pending_cleanup_called);

      auto [pending_reject, resolve_reject, reject_reject] = promise::Create<int>();
      bool reject_cleanup_called                           = false;
      auto reject_chain = std::move(pending_reject).Finally([&] { reject_cleanup_called = true; });
      REQUIRE((*reject_reject)(std::make_exception_ptr(TestError{"finally reject"})));
      REQUIRE_FALSE((*resolve_reject)(1));
      REQUIRE(reject_chain.Rejected());
      REQUIRE(reject_cleanup_called);
   });
}

TEST_CASE("Finally async continuation on pending paths", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [pending_value, resolve_value, reject_value] = promise::Create<int>();
      auto async_finally_value = pending_value.Finally([]() -> Promise<void> { co_return; });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(14));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(async_finally_value.Resolved());
      REQUIRE(async_finally_value.Value() == 14);

      auto [pending_reject, resolve_reject, reject_reject] = promise::Create<int>();
      auto async_finally_reject = pending_reject.Finally([]() -> Promise<void> { co_return; });
      pending_reject.WaitAwaited(0);
      REQUIRE((*reject_reject)(std::make_exception_ptr(TestError{"async finally reject"})));
      REQUIRE_FALSE((*resolve_reject)(2));
      REQUIRE(async_finally_reject.Rejected());
      RequireException<TestError>(async_finally_reject.Exception());
   });
}

TEST_CASE("Race covers helper and member race with already done inputs", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [pending_for_helper, resolve_pending_for_helper, reject_pending_for_helper] =
        promise::Create<int>();
      auto race_from_helpers = promise::Race(Promise<int>::Resolve(1), pending_for_helper);
      REQUIRE(race_from_helpers.Done());
      REQUIRE(race_from_helpers.Resolved());
      REQUIRE((*resolve_pending_for_helper)(2));
      REQUIRE_FALSE((*reject_pending_for_helper)(std::make_exception_ptr(TestError{"ignored"})));

      auto [pending_member, resolve_member, reject_member] = promise::Create<int>();
      auto source                                          = Promise<int>::Resolve(33);
      auto raced = source.Race(std::move(pending_member), resolve_member, reject_member);
      REQUIRE(raced.Done());
      REQUIRE_FALSE((*resolve_member)(44));
      REQUIRE_FALSE((*reject_member)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(raced.Resolved());
      REQUIRE(raced.Value() == 33);
   });
}

TEST_CASE("Resolver-style MakePromise overloads are exercised", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto from_resolve_and_reject =
        MakePromise([](Resolve<int> const& resolve, Reject const&) -> Promise<int, true> {
           REQUIRE(resolve(41));
           co_return;
        });

      REQUIRE(from_resolve_and_reject.Resolved());
      REQUIRE(from_resolve_and_reject.Value() == 41);

      auto from_resolve_only = MakePromise([](Resolve<void> const& resolve) -> Promise<void, true> {
         REQUIRE(resolve());
         co_return;
      });

      REQUIRE(from_resolve_only.Resolved());
   });
}

TEST_CASE("Race mixed value and void covers optional return branches", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [pending_value, resolve_value, reject_value] = promise::Create<int>();
      auto mixed_race = promise::Race(Promise<void>::Resolve(), pending_value);

      REQUIRE(mixed_race.Resolved());
      REQUIRE_FALSE(mixed_race.Value().has_value());
      REQUIRE((*resolve_value)(13));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));

      auto [pending_void, resolve_void, reject_void] = promise::Create<void>();
      auto mixed_race_2 = promise::Race(Promise<int>::Resolve(22), pending_void);

      REQUIRE(mixed_race_2.Resolved());
      REQUIRE(mixed_race_2.Value().has_value());
      REQUIRE(mixed_race_2.Value().value() == 22);
      REQUIRE((*resolve_void)());
      REQUIRE_FALSE((*reject_void)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEST_CASE("MakePromise argument-shape dispatch covers Create routing branches", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto no_arg = MakePromise([]() -> Promise<int> { co_return 3; });
      REQUIRE(no_arg.Resolved());
      REQUIRE(no_arg.Value() == 3);

      auto one_value_arg = MakePromise([](int value) -> Promise<int> { co_return value + 2; }, 4);
      REQUIRE(one_value_arg.Resolved());
      REQUIRE(one_value_arg.Value() == 6);

      auto two_value_args =
        MakePromise([](int lhs, int rhs) -> Promise<int> { co_return lhs + rhs; }, 5, 7);
      REQUIRE(two_value_args.Resolved());
      REQUIRE(two_value_args.Value() == 12);

      auto resolver_first = MakePromise(
        [](Resolve<int> const& resolve, int value) -> Promise<int, true> {
           REQUIRE(resolve(value + 10));
           co_return;
        },
        8
      );
      REQUIRE(resolver_first.Resolved());
      REQUIRE(resolver_first.Value() == 18);

      auto resolver_and_reject = MakePromise(
        [](Resolve<int> const&, Reject const& reject, int code) -> Promise<int, true> {
           reject.Apply<TestError>(std::to_string(code));
           co_return;
        },
        99
      );
      REQUIRE(resolver_and_reject.Rejected());
      RequireException<TestError>(resolver_and_reject.Exception());
   });
}

TEST_CASE("Pending Then branches cover sync throw and async throw propagation", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [pending_sync_throw, resolve_sync_throw, reject_sync_throw] = promise::Create<int>();
      auto sync_throw_chain =
        pending_sync_throw.Then([](int) -> int { throw CatchError{"pending then sync throw"}; });
      pending_sync_throw.WaitAwaited(0);
      REQUIRE((*resolve_sync_throw)(5));
      REQUIRE_FALSE((*reject_sync_throw)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(sync_throw_chain.Rejected());
      RequireException<CatchError>(sync_throw_chain.Exception());

      auto [pending_async_throw, resolve_async_throw, reject_async_throw] = promise::Create<int>();
      auto async_throw_chain = pending_async_throw.Then([](int) -> Promise<int> {
         throw CatchError{"pending then async throw"};
         co_return 0;
      });
      pending_async_throw.WaitAwaited(0);
      REQUIRE((*resolve_async_throw)(6));
      REQUIRE_FALSE((*reject_async_throw)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(async_throw_chain.Rejected());
      RequireException<CatchError>(async_throw_chain.Exception());

      auto [pending_then_void, resolve_then_void, reject_then_void] = promise::Create<void>();
      auto then_void_to_async = pending_then_void.Then([]() -> Promise<int> { co_return 19; });
      pending_then_void.WaitAwaited(0);
      REQUIRE((*resolve_then_void)());
      REQUIRE_FALSE((*reject_then_void)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(then_void_to_async.Resolved());
      REQUIRE(then_void_to_async.Value() == 19);
   });
}

TEST_CASE("Pending Catch branches cover promise and void handlers", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [pending_catch_async, resolve_catch_async, reject_catch_async] = promise::Create<int>();
      auto catch_async =
        pending_catch_async.Catch([](TestError const&) -> Promise<int> { co_return 88; });
      pending_catch_async.WaitAwaited(0);
      REQUIRE((*reject_catch_async)(std::make_exception_ptr(TestError{"pending catch async"})));
      REQUIRE_FALSE((*resolve_catch_async)(1));
      REQUIRE(catch_async.Resolved());
      REQUIRE(catch_async.Value() == 88);

      auto [pending_catch_void, resolve_catch_void, reject_catch_void] = promise::Create<int>();
      auto catch_to_void = pending_catch_void.Catch([](TestError const&) {});
      pending_catch_void.WaitAwaited(0);
      REQUIRE((*reject_catch_void)(std::make_exception_ptr(TestError{"pending catch to void"})));
      REQUIRE_FALSE((*resolve_catch_void)(2));
      REQUIRE(catch_to_void.Resolved());
      REQUIRE_FALSE(catch_to_void.Value().has_value());

      auto [pending_void_passthrough, resolve_void_passthrough, reject_void_passthrough] =
        promise::Create<void>();
      auto void_passthrough = pending_void_passthrough.Catch([](TestError const&) { return 123; });
      pending_void_passthrough.WaitAwaited(0);
      REQUIRE((*resolve_void_passthrough)());
      REQUIRE_FALSE(
        (*reject_void_passthrough)(std::make_exception_ptr(TestError{"ignored passthrough"}))
      );
      REQUIRE(void_passthrough.Resolved());
      REQUIRE_FALSE(void_passthrough.Value().has_value());
   });
}

TEST_CASE("Pending Finally branches cover async success and failure", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [pending_value, resolve_value, reject_value] = promise::Create<int>();
      auto finally_async_fail = pending_value.Finally([]() -> Promise<void> {
         throw FinallyError{"pending finally async failure"};
         co_return;
      });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(9));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(finally_async_fail.Rejected());
      RequireException<FinallyError>(finally_async_fail.Exception());

      auto [pending_void, resolve_void, reject_void] = promise::Create<void>();
      auto finally_async_ok = pending_void.Finally([]() -> Promise<void> { co_return; });
      pending_void.WaitAwaited(0);
      REQUIRE((*resolve_void)());
      REQUIRE_FALSE((*reject_void)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(finally_async_ok.Resolved());
   });
}

TEST_CASE("Type-erased VAwait covers pending and rejection paths", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [pending_ok, resolve_ok, reject_ok] = promise::Create<int>();
      auto erased_ok  = std::move(pending_ok).ToPointer<promise::VPromise>();
      bool resumed_ok = false;

      auto waiter_ok = WPromise{[&]() -> Promise<void> {
         co_await erased_ok->VAwait();
         resumed_ok = true;
         co_return;
      }};

      REQUIRE_FALSE(waiter_ok.Done());
      REQUIRE((*resolve_ok)(17));
      REQUIRE_FALSE((*reject_ok)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(waiter_ok.Resolved());
      REQUIRE(resumed_ok);

      auto [pending_reject, resolve_reject, reject_reject] = promise::Create<int>();
      auto erased_reject = std::move(pending_reject).ToPointer<promise::VPromise>();
      bool caught_reject = false;

      auto waiter_reject = WPromise{[&]() -> Promise<void> {
         try {
            co_await erased_reject->VAwait();
         } catch (TestError const&) {
            caught_reject = true;
         }
         co_return;
      }};

      REQUIRE_FALSE(waiter_reject.Done());
      REQUIRE((*reject_reject)(std::make_exception_ptr(TestError{"erased reject"})));
      REQUIRE_FALSE((*resolve_reject)(99));
      REQUIRE(waiter_reject.Resolved());
      REQUIRE(caught_reject);
   });
}

TEST_CASE("WaitAwaited default current-count branch is exercised", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<int>();

      std::promise<int> chained_result_promise;
      auto              chained_result = chained_result_promise.get_future();

      std::thread worker([&] {
         auto chained = source.Then([](int value) { return value + 30; });
         chained.WaitDone();
         if (chained.Resolved()) {
            chained_result_promise.set_value(chained.Value());
         } else {
            chained_result_promise.set_value(-1);
         }
      });

      source.WaitAwaited();
      REQUIRE(source.Awaiters() == 1);

      REQUIRE((*resolve)(12));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(chained_result.get() == 42);

      worker.join();
   });
}

TEST_CASE("WPromise Race lvalue and rvalue dispatch variants", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [pending_lvalue, resolve_lvalue, reject_lvalue] = promise::Create<int>();
      auto source_lvalue                                   = WPromise<int>::Resolve(5);
      auto raced_lvalue =
        source_lvalue.Race(std::move(pending_lvalue), resolve_lvalue, reject_lvalue);

      REQUIRE(raced_lvalue.Resolved());
      REQUIRE(raced_lvalue.Value() == 5);
      REQUIRE_FALSE((*resolve_lvalue)(44));
      REQUIRE_FALSE((*reject_lvalue)(std::make_exception_ptr(TestError{"ignored"})));

      auto [pending_rvalue, resolve_rvalue, reject_rvalue] = promise::Create<int>();
      auto raced_rvalue =
        WPromise<int>::Resolve(6).Race(std::move(pending_rvalue), resolve_rvalue, reject_rvalue);

      REQUIRE(raced_rvalue.Resolved());
      REQUIRE(raced_rvalue.Value() == 6);
      REQUIRE_FALSE((*resolve_rvalue)(45));
      REQUIRE_FALSE((*reject_rvalue)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEST_CASE(
  "WPromise co_await covers pending, const-lvalue, and rejection paths",
  "[Promise][Core]"
) {
   RunWithTimeout(2s, [&] {
      auto [gate, resolve_gate, reject_gate] = promise::Create<void>();

      auto source_pending = WPromise{[&]() -> Promise<int> {
         co_await gate;
         co_return 31;
      }};

      auto waiter_pending = WPromise{[&]() -> Promise<int> {
         int value = co_await source_pending;
         co_return value + 1;
      }};

      REQUIRE_FALSE(waiter_pending.Done());
      REQUIRE((*resolve_gate)());
      REQUIRE_FALSE((*reject_gate)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(waiter_pending.Resolved());
      REQUIRE(waiter_pending.Value() == 32);

      auto source_reject = WPromise{[]() -> Promise<int> {
         throw TestError{"co_await reject"};
         co_return 0;
      }};

      auto waiter_reject = WPromise{[&]() -> Promise<int> {
         try {
            (void)(co_await source_reject);
            co_return -1;
         } catch (TestError const&) {
            co_return 77;
         }
      }};

      REQUIRE(waiter_reject.Resolved());
      REQUIRE(waiter_reject.Value() == 77);

      auto       source_const = WPromise{[]() -> Promise<int> { co_return 44; }};
      const auto const_source{source_const};

      auto waiter_const = WPromise{[&]() -> Promise<int> {
         int value = co_await const_source;
         co_return value;
      }};

      REQUIRE(waiter_const.Resolved());
      REQUIRE(waiter_const.Value() == 44);
   });
}

TEST_CASE("WPromise destructor waits for pending completion", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [pending, resolve, reject] = promise::Create<int>();

      std::thread resolver([resolve = resolve] {
         std::this_thread::sleep_for(20ms);
         (void)(*resolve)(88);
      });

      {
         auto pending_copy = pending;
      }

      resolver.join();
      REQUIRE(pending.Resolved());
      REQUIRE(pending.Value() == 88);
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEST_CASE("Race cleanup covers UnAwait true and false branches", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [source_a, resolve_source_a, reject_source_a] = promise::Create<int>();
      auto [race_a, resolve_race_a, reject_race_a]       = promise::Create<int>();

      auto raced_a = source_a.Race(std::move(race_a), resolve_race_a, reject_race_a);
      source_a.WaitAwaited(0);
      REQUIRE(source_a.Awaiters() == 1);

      REQUIRE((*resolve_race_a)(500));
      REQUIRE_FALSE((*reject_race_a)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(raced_a.Resolved());
      REQUIRE(raced_a.Value() == 500);
      REQUIRE(source_a.Awaiters() == 0);

      REQUIRE((*resolve_source_a)(100));
      REQUIRE_FALSE((*reject_source_a)(std::make_exception_ptr(TestError{"ignored"})));

      auto [source_b, resolve_source_b, reject_source_b] = promise::Create<int>();
      auto [race_b, resolve_race_b, reject_race_b]       = promise::Create<int>();

      auto raced_b = source_b.Race(std::move(race_b), resolve_race_b, reject_race_b);
      source_b.WaitAwaited(0);
      REQUIRE(source_b.Awaiters() == 1);

      REQUIRE((*resolve_source_b)(777));
      REQUIRE_FALSE((*reject_source_b)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(raced_b.Resolved());
      REQUIRE(raced_b.Value() == 777);

      REQUIRE_FALSE((*resolve_race_b)(123));
      REQUIRE_FALSE((*reject_race_b)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEST_CASE("Catch async exception_ptr and lvalue-ref pending paths", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto done_ptr_async = Promise<int>::Reject<TestError>("done ptr async")
                              .Catch([](std::exception_ptr exception) -> Promise<void> {
                                 RequireException<TestError>(exception);
                                 co_return;
                              });
      REQUIRE(done_ptr_async.Resolved());
      REQUIRE_FALSE(done_ptr_async.Value().has_value());

      auto [pending_ptr, resolve_ptr, reject_ptr] = promise::Create<int>();
      auto pending_ptr_async = pending_ptr.Catch([](std::exception_ptr exception) -> Promise<void> {
         RequireException<TestError>(exception);
         co_return;
      });
      pending_ptr.WaitAwaited(0);
      REQUIRE((*reject_ptr)(std::make_exception_ptr(TestError{"pending ptr async"})));
      REQUIRE_FALSE((*resolve_ptr)(1));
      REQUIRE(pending_ptr_async.Resolved());
      REQUIRE_FALSE(pending_ptr_async.Value().has_value());

      auto [gate, resolve_gate, reject_gate]         = promise::Create<void>();
      auto [pending_lref, resolve_lref, reject_lref] = promise::Create<int>();
      auto pending_lref_async = pending_lref.Catch([&](TestError const& exception) -> Promise<int> {
         REQUIRE(std::string{exception.what()} == "pending lref async");
         co_await gate;
         co_return 909;
      });

      pending_lref.WaitAwaited(0);
      REQUIRE((*reject_lref)(std::make_exception_ptr(TestError{"pending lref async"})));
      REQUIRE_FALSE((*resolve_lref)(2));
      REQUIRE_FALSE(pending_lref_async.Done());
      REQUIRE((*resolve_gate)());
      REQUIRE_FALSE((*reject_gate)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending_lref_async.Resolved());
      REQUIRE(pending_lref_async.Value() == 909);
   });
}

TEST_CASE("Catch typed mismatch preserves original exception", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto done_mismatch =
        Promise<int>::Reject<CatchError>("mismatch done").Catch([](TestError const&) { return 1; });
      REQUIRE(done_mismatch.Rejected());
      RequireException<CatchError>(done_mismatch.Exception());

      auto [pending_mismatch, resolve_mismatch, reject_mismatch] = promise::Create<int>();
      auto pending_chain = pending_mismatch.Catch([](TestError const&) { return 2; });

      pending_mismatch.WaitAwaited(0);
      REQUIRE((*reject_mismatch)(std::make_exception_ptr(CatchError{"mismatch pending"})));
      REQUIRE_FALSE((*resolve_mismatch)(99));
      REQUIRE(pending_chain.Rejected());
      RequireException<CatchError>(pending_chain.Exception());
   });
}

TEST_CASE("Finally async on done resolved can replace with finally exception", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto resolved_then_finally_reject = Promise<int>::Resolve(123).Finally([]() -> Promise<void> {
         throw FinallyError{"finally async done reject"};
         co_return;
      });

      REQUIRE(resolved_then_finally_reject.Rejected());
      RequireException<FinallyError>(resolved_then_finally_reject.Exception());
   });
}

TEST_CASE("Catch typed mismatch with async handler keeps original rejection", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto done_async_mismatch = Promise<int>::Reject<CatchError>("async mismatch")
                                   .Catch([](TestError const&) -> Promise<int> { co_return 1; });

      REQUIRE(done_async_mismatch.Rejected());
      RequireException<CatchError>(done_async_mismatch.Exception());
   });
}

TEST_CASE("Catch promise-returning passthrough on resolved sources", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto done_value_passthrough =
        Promise<int>::Resolve(21).Catch([](std::exception_ptr) -> Promise<void> { co_return; });
      REQUIRE(done_value_passthrough.Resolved());
      REQUIRE(done_value_passthrough.Value().has_value());
      REQUIRE(done_value_passthrough.Value().value() == 21);

      auto done_void_passthrough =
        Promise<void>::Resolve().Catch([](std::exception_ptr) -> Promise<int> { co_return 7; });
      REQUIRE(done_void_passthrough.Resolved());
      REQUIRE_FALSE(done_void_passthrough.Value().has_value());

      auto [pending_value, resolve_value, reject_value] = promise::Create<int>();
      auto pending_value_passthrough =
        pending_value.Catch([](std::exception_ptr) -> Promise<void> { co_return; });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(33));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending_value_passthrough.Resolved());
      REQUIRE(pending_value_passthrough.Value().has_value());
      REQUIRE(pending_value_passthrough.Value().value() == 33);

      auto [pending_void, resolve_void, reject_void] = promise::Create<void>();
      auto pending_void_passthrough =
        pending_void.Catch([](std::exception_ptr) -> Promise<int> { co_return 9; });
      pending_void.WaitAwaited(0);
      REQUIRE((*resolve_void)());
      REQUIRE_FALSE((*reject_void)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending_void_passthrough.Resolved());
      REQUIRE_FALSE(pending_void_passthrough.Value().has_value());
   });
}

TEST_CASE("Resolver-backed pending chains exercise WITH_RESOLVER branches", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [gate_resolve, resolve_gate_resolve, reject_gate_resolve] = promise::Create<void>();

      auto resolver_pending =
        MakePromise([&](Resolve<int> const& resolve, Reject const&) -> Promise<int, true> {
           co_await gate_resolve;
           REQUIRE(resolve(40));
           co_return;
        });

      auto resolver_then        = resolver_pending.Then([](int value) { return value + 2; });
      auto resolver_finally_ran = false;
      auto resolver_finally     = resolver_pending.Finally([&] { resolver_finally_ran = true; });

      REQUIRE_FALSE(resolver_then.Done());
      REQUIRE((*resolve_gate_resolve)());
      REQUIRE_FALSE((*reject_gate_resolve)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(resolver_then.Resolved());
      REQUIRE(resolver_then.Value() == 42);
      REQUIRE(resolver_finally.Resolved());
      REQUIRE(resolver_finally.Value() == 40);
      REQUIRE(resolver_finally_ran);

      auto [gate_reject, resolve_gate_reject, reject_gate_reject] = promise::Create<void>();

      auto resolver_pending_reject =
        MakePromise([&](Resolve<int> const&, Reject const& reject) -> Promise<int, true> {
           co_await gate_reject;
           reject.Apply<TestError>("resolver pending reject");
           co_return;
        });

      auto resolver_catch = resolver_pending_reject.Catch([](TestError const&) { return 99; });
      auto resolver_reject_finally_ran = false;
      auto resolver_reject_finally =
        resolver_pending_reject.Finally([&] { resolver_reject_finally_ran = true; });

      REQUIRE_FALSE(resolver_catch.Done());
      REQUIRE((*resolve_gate_reject)());
      REQUIRE_FALSE((*reject_gate_reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(resolver_catch.Resolved());
      REQUIRE(resolver_catch.Value() == 99);
      REQUIRE(resolver_reject_finally.Rejected());
      RequireException<TestError>(resolver_reject_finally.Exception());
      REQUIRE(resolver_reject_finally_ran);
   });
}

TEST_CASE("Details Promise static forwarding for resolver-less mode", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto [created, resolve_created, reject_created] =
        promise::details::Promise<int, false>::Create();

      REQUIRE_FALSE(created.Done());
      REQUIRE((*resolve_created)(55));
      REQUIRE_FALSE((*reject_created)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(created.Resolved());
      REQUIRE(created.Value() == 55);

      auto resolved_via_false_details = promise::details::Promise<int, false>::Resolve(77);
      REQUIRE(resolved_via_false_details.Resolved());
      REQUIRE(resolved_via_false_details.Value() == 77);

      auto rejected_via_false_details =
        promise::details::Promise<int, false>::Reject<TestError>("forwarded reject");
      REQUIRE(rejected_via_false_details.Rejected());
      RequireException<TestError>(rejected_via_false_details.Exception());
   });
}

TEST_CASE("Done Then sync-throw paths reject correctly", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto value_then_throw = Promise<int>::Resolve(4).Then([](int) -> int {
         throw CatchError{"done value then throw"};
      });
      REQUIRE(value_then_throw.Rejected());
      RequireException<CatchError>(value_then_throw.Exception());

      auto void_then_throw =
        Promise<void>::Resolve().Then([]() { throw CatchError{"done void then throw"}; });
      REQUIRE(void_then_throw.Rejected());
      RequireException<CatchError>(void_then_throw.Exception());
   });
}

TEST_CASE("Done Finally sync throw overrides source state", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto resolved_then_throw =
        Promise<int>::Resolve(1).Finally([] { throw FinallyError{"finally overrides resolve"}; });
      REQUIRE(resolved_then_throw.Rejected());
      RequireException<FinallyError>(resolved_then_throw.Exception());

      auto rejected_then_throw = Promise<int>::Reject<TestError>("base reject").Finally([] {
         throw FinallyError{"finally overrides reject"};
      });
      REQUIRE(rejected_then_throw.Rejected());
      RequireException<FinallyError>(rejected_then_throw.Exception());
   });
}

TEST_CASE("Done Catch handler throw and mismatch branches", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto done_catch_throw =
        Promise<int>::Reject<TestError>("done catch throw").Catch([](TestError const&) -> int {
           throw CatchError{"catch handler throw"};
        });
      REQUIRE(done_catch_throw.Rejected());
      RequireException<CatchError>(done_catch_throw.Exception());

      auto done_mismatch_passthrough =
        Promise<int>::Reject<CatchError>("mismatch").Catch([](TestError const&) -> int {
           return 1;
        });
      REQUIRE(done_mismatch_passthrough.Rejected());
      RequireException<CatchError>(done_mismatch_passthrough.Exception());

      auto done_exception_ptr_throw =
        Promise<int>::Reject<TestError>("ptr throw").Catch([](std::exception_ptr) -> int {
           throw CatchError{"ptr handler throw"};
        });
      REQUIRE(done_exception_ptr_throw.Rejected());
      RequireException<CatchError>(done_exception_ptr_throw.Exception());
   });
}

TEST_CASE("Done Finally async branches on resolved and rejected sources", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto done_resolved_async =
        Promise<int>::Resolve(9).Finally([]() -> Promise<void> { co_return; });
      REQUIRE(done_resolved_async.Resolved());
      REQUIRE(done_resolved_async.Value() == 9);
   });
}
