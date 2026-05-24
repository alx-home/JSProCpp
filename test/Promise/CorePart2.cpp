#include "../TestCommon.h"

#include <catch2/catch_test_macros.hpp>
#include <exception>

TEMPLATE_LIST_TEST_CASE(
  "Race covers helper and member race with already done inputs",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_for_helper, resolve_pending_for_helper, reject_pending_for_helper] =
        promise::Create<T>();
      auto race_from_helpers =
        promise::Race(Promise<T>::Resolve(test_types::ValueFromInt<T>(1)), pending_for_helper);
      REQUIRE(race_from_helpers.Done());
      REQUIRE(race_from_helpers.Resolved());
      REQUIRE((*resolve_pending_for_helper)(test_types::ValueFromInt<T>(2)));
      REQUIRE_FALSE((*reject_pending_for_helper)(std::make_exception_ptr(TestError{"ignored"})));

      auto [pending_member, resolve_member, reject_member] = promise::Create<T>();
      auto source = Promise<T>::Resolve(test_types::ValueFromInt<T>(33));
      auto raced  = source.Race(std::move(pending_member), resolve_member, reject_member);
      REQUIRE(raced.Done());
      REQUIRE_FALSE((*resolve_member)(test_types::ValueFromInt<T>(44)));
      REQUIRE_FALSE((*reject_member)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(raced.Resolved());
      REQUIRE(raced.Value() == test_types::ValueFromInt<T>(33));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Resolver-style MakePromise overloads are exercised",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto from_resolve_and_reject =
        MakePromise([](Resolve<T> const& resolve, Reject const&) -> Promise<T, true> {
           REQUIRE(resolve(test_types::ValueFromInt<T>(41)));
           co_return;
        });

      REQUIRE(from_resolve_and_reject.Resolved());
      REQUIRE(from_resolve_and_reject.Value() == test_types::ValueFromInt<T>(41));

      auto from_resolve_only = MakePromise([](Resolve<void> const& resolve) -> Promise<void, true> {
         REQUIRE(resolve());
         co_return;
      });

      REQUIRE(from_resolve_only.Resolved());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Race mixed value and void covers optional return branches",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_value, resolve_value, reject_value] = promise::Create<T>();
      auto mixed_race = promise::Race(Promise<void>::Resolve(), pending_value);

      REQUIRE(mixed_race.Resolved());
      REQUIRE_FALSE(mixed_race.Value().has_value());
      REQUIRE((*resolve_value)(test_types::ValueFromInt<T>(13)));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));

      auto [pending_void, resolve_void, reject_void] = promise::Create<void>();
      auto mixed_race_2 =
        promise::Race(Promise<T>::Resolve(test_types::ValueFromInt<T>(22)), pending_void);

      REQUIRE(mixed_race_2.Resolved());
      REQUIRE(mixed_race_2.Value().has_value());
      REQUIRE(mixed_race_2.Value().value() == test_types::ValueFromInt<T>(22));
      REQUIRE((*resolve_void)());
      REQUIRE_FALSE((*reject_void)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "MakePromise argument-shape dispatch covers Create routing branches",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto no_arg = MakePromise([]() -> Promise<T> { co_return test_types::ValueFromInt<T>(3); });
      REQUIRE(no_arg.Resolved());
      REQUIRE(no_arg.Value() == test_types::ValueFromInt<T>(3));

      auto one_value_arg = MakePromise(
        [](T value) -> Promise<T> { co_return test_types::BumpValue(value, 2); },
        test_types::ValueFromInt<T>(4)
      );
      REQUIRE(one_value_arg.Resolved());
      REQUIRE(one_value_arg.Value() == test_types::BumpValue(test_types::ValueFromInt<T>(4), 2));

      auto two_value_args = MakePromise(
        [](T lhs, T rhs) -> Promise<T> {
           (void)rhs;
           co_return test_types::BumpValue(lhs, 7);
        },
        test_types::ValueFromInt<T>(5),
        test_types::ValueFromInt<T>(7)
      );
      REQUIRE(two_value_args.Resolved());
      REQUIRE(two_value_args.Value() == test_types::BumpValue(test_types::ValueFromInt<T>(5), 7));

      auto resolver_first = MakePromise(
        [](Resolve<T> const& resolve, T value) -> Promise<T, true> {
           REQUIRE(resolve(test_types::BumpValue(value, 10)));
           co_return;
        },
        test_types::ValueFromInt<T>(8)
      );
      REQUIRE(resolver_first.Resolved());
      REQUIRE(resolver_first.Value() == test_types::BumpValue(test_types::ValueFromInt<T>(8), 10));

      auto resolver_and_reject = MakePromise(
        [](Resolve<T> const&, Reject const& reject, int code) -> Promise<T, true> {
           reject.Apply<TestError>(std::to_string(code));
           co_return;
        },
        99
      );
      REQUIRE(resolver_and_reject.Rejected());
      RequireException<TestError>(resolver_and_reject.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Pending Then branches cover sync throw and async throw propagation",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_sync_throw, resolve_sync_throw, reject_sync_throw] = promise::Create<T>();
      auto sync_throw_chain =
        pending_sync_throw.Then([](T) -> T { throw CatchError{"pending then sync throw"}; });
      pending_sync_throw.WaitAwaited(0);
      REQUIRE((*resolve_sync_throw)(test_types::ValueFromInt<T>(5)));
      REQUIRE_FALSE((*reject_sync_throw)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(sync_throw_chain.Rejected());
      RequireException<CatchError>(sync_throw_chain.Exception());

      auto [pending_async_throw, resolve_async_throw, reject_async_throw] = promise::Create<T>();
      auto async_throw_chain = pending_async_throw.Then([](T) -> Promise<T> {
         throw CatchError{"pending then async throw"};
         co_return test_types::ValueFromInt<T>(0);
      });
      pending_async_throw.WaitAwaited(0);
      REQUIRE((*resolve_async_throw)(test_types::ValueFromInt<T>(6)));
      REQUIRE_FALSE((*reject_async_throw)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(async_throw_chain.Rejected());
      RequireException<CatchError>(async_throw_chain.Exception());

      auto [pending_then_void, resolve_then_void, reject_then_void] = promise::Create<void>();
      auto then_void_to_async =
        pending_then_void.Then([]() -> Promise<T> { co_return test_types::ValueFromInt<T>(19); });
      pending_then_void.WaitAwaited(0);
      REQUIRE((*resolve_then_void)());
      REQUIRE_FALSE((*reject_then_void)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(then_void_to_async.Resolved());
      REQUIRE(then_void_to_async.Value() == test_types::ValueFromInt<T>(19));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Pending Catch branches cover promise and void handlers",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_catch_async, resolve_catch_async, reject_catch_async] = promise::Create<T>();
      auto catch_async = pending_catch_async.Catch([](TestError const&) -> Promise<T> {
         co_return test_types::ValueFromInt<T>(88);
      });
      pending_catch_async.WaitAwaited(0);
      REQUIRE((*reject_catch_async)(std::make_exception_ptr(TestError{"pending catch async"})));
      REQUIRE_FALSE((*resolve_catch_async)(test_types::ValueFromInt<T>(1)));
      REQUIRE(catch_async.Resolved());
      REQUIRE(catch_async.Value() == test_types::ValueFromInt<T>(88));

      auto [pending_catch_void, resolve_catch_void, reject_catch_void] = promise::Create<T>();
      auto catch_to_void = pending_catch_void.Catch([](TestError const&) {});
      pending_catch_void.WaitAwaited(0);
      REQUIRE((*reject_catch_void)(std::make_exception_ptr(TestError{"pending catch to void"})));
      REQUIRE_FALSE((*resolve_catch_void)(test_types::ValueFromInt<T>(2)));
      REQUIRE(catch_to_void.Resolved());
      REQUIRE_FALSE(catch_to_void.Value().has_value());

      auto [pending_void_passthrough, resolve_void_passthrough, reject_void_passthrough] =
        promise::Create<void>();
      auto void_passthrough = pending_void_passthrough.Catch([](TestError const&) {
         return test_types::ValueFromInt<T>(123);
      });
      pending_void_passthrough.WaitAwaited(0);
      REQUIRE((*resolve_void_passthrough)());
      REQUIRE_FALSE(
        (*reject_void_passthrough)(std::make_exception_ptr(TestError{"ignored passthrough"}))
      );
      REQUIRE(void_passthrough.Resolved());
      REQUIRE_FALSE(void_passthrough.Value().has_value());
   });
}
