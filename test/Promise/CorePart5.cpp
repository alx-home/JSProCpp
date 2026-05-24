#include "../TestCommon.h"

#include <catch2/catch_test_macros.hpp>
#include "promise/core/Promise.h"
#include <exception>

TEMPLATE_LIST_TEST_CASE(
  "Resolver-backed pending chains exercise WITH_RESOLVER branches",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [gate_resolve, resolve_gate_resolve, reject_gate_resolve] = promise::Create<void>();

      auto resolver_pending =
        MakePromise([&](Resolve<T> const& resolve, Reject const&) -> Promise<T, true> {
           co_await gate_resolve;
           REQUIRE(resolve(test_types::ValueFromInt<T>(40)));
           co_return;
        });

      auto resolver_then =
        resolver_pending.Then([](T value) { return test_types::BumpValue(value, 2); });
      auto resolver_finally_ran = false;
      auto resolver_finally     = resolver_pending.Finally([&] { resolver_finally_ran = true; });

      REQUIRE_FALSE(resolver_then.Done());
      REQUIRE((*resolve_gate_resolve)());
      REQUIRE_FALSE((*reject_gate_resolve)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(resolver_then.Resolved());
      REQUIRE(resolver_then.Value() == test_types::BumpValue(test_types::ValueFromInt<T>(40), 2));
      REQUIRE(resolver_finally.Resolved());
      REQUIRE(resolver_finally.Value() == test_types::ValueFromInt<T>(40));
      REQUIRE(resolver_finally_ran);

      auto [gate_reject, resolve_gate_reject, reject_gate_reject] = promise::Create<void>();

      auto resolver_pending_reject =
        MakePromise([&](Resolve<T> const&, Reject const& reject) -> Promise<T, true> {
           co_await gate_reject;
           reject.Apply<TestError>("resolver pending reject");
           co_return;
        });

      auto resolver_catch              = resolver_pending_reject.Catch([](TestError const&) {
         return test_types::ValueFromInt<T>(99);
      });
      auto resolver_reject_finally_ran = false;
      auto resolver_reject_finally =
        resolver_pending_reject.Finally([&] { resolver_reject_finally_ran = true; });

      REQUIRE_FALSE(resolver_catch.Done());
      REQUIRE((*resolve_gate_reject)());
      REQUIRE_FALSE((*reject_gate_reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(resolver_catch.Resolved());
      REQUIRE(resolver_catch.Value() == test_types::ValueFromInt<T>(99));
      REQUIRE(resolver_reject_finally.Rejected());
      RequireException<TestError>(resolver_reject_finally.Exception());
      REQUIRE(resolver_reject_finally_ran);
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Details Promise static forwarding for resolver-less mode",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [created, resolve_created, reject_created] =
        promise::details::Promise<T, false>::Create();

      REQUIRE_FALSE(created.Done());
      REQUIRE((*resolve_created)(test_types::ValueFromInt<T>(55)));
      REQUIRE_FALSE((*reject_created)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(created.Resolved());
      REQUIRE(created.Value() == test_types::ValueFromInt<T>(55));

      auto resolved_via_false_details =
        promise::details::Promise<T, false>::Resolve(test_types::ValueFromInt<T>(77));
      REQUIRE(resolved_via_false_details.Resolved());
      REQUIRE(resolved_via_false_details.Value() == test_types::ValueFromInt<T>(77));

      auto rejected_via_false_details =
        promise::details::Promise<T, false>::template Reject<TestError>("forwarded reject");
      REQUIRE(rejected_via_false_details.Rejected());
      RequireException<TestError>(rejected_via_false_details.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Done Then sync-throw paths reject correctly",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto value_then_throw = Promise<T>::Resolve(test_types::ValueFromInt<T>(4)).Then([](T) -> T {
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

TEMPLATE_LIST_TEST_CASE(
  "Done Finally sync throw overrides source state",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto resolved_then_throw = Promise<T>::Resolve(test_types::ValueFromInt<T>(1)).Finally([] {
         throw FinallyError{"finally overrides resolve"};
      });
      REQUIRE(resolved_then_throw.Rejected());
      RequireException<FinallyError>(resolved_then_throw.Exception());

      auto rejected_then_throw = Promise<T>::template Reject<TestError>("base reject").Finally([] {
         throw FinallyError{"finally overrides reject"};
      });
      REQUIRE(rejected_then_throw.Rejected());
      RequireException<FinallyError>(rejected_then_throw.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Done Catch handler throw and mismatch branches",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto done_catch_throw =
        Promise<T>::template Reject<TestError>("done catch throw").Catch([](TestError const&) -> T {
           throw CatchError{"catch handler throw"};
        });
      REQUIRE(done_catch_throw.Rejected());
      RequireException<CatchError>(done_catch_throw.Exception());

      auto done_mismatch_passthrough =
        Promise<T>::template Reject<CatchError>("mismatch").Catch([](TestError const&) -> T {
           return test_types::ValueFromInt<T>(1);
        });
      REQUIRE(done_mismatch_passthrough.Rejected());
      RequireException<CatchError>(done_mismatch_passthrough.Exception());

      auto done_exception_ptr_throw =
        Promise<T>::template Reject<TestError>("ptr throw").Catch([](std::exception_ptr) -> T {
           throw CatchError{"ptr handler throw"};
        });
      REQUIRE(done_exception_ptr_throw.Rejected());
      RequireException<CatchError>(done_exception_ptr_throw.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Done Finally async branches on resolved and rejected sources",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto done_resolved_async =
        Promise<T>::Resolve(test_types::ValueFromInt<T>(9)).Finally([]() -> Promise<void> {
           co_return;
        });
      REQUIRE(done_resolved_async.Resolved());
      REQUIRE(done_resolved_async.Value() == test_types::ValueFromInt<T>(9));
   });
}
