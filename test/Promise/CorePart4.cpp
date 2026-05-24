#include "../TestCommon.h"

#include <catch2/catch_test_macros.hpp>
#include <exception>

TEMPLATE_LIST_TEST_CASE(
  "Race cleanup covers UnAwait true and false branches",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [source_a, resolve_source_a, reject_source_a] = promise::Create<T>();
      auto [race_a, resolve_race_a, reject_race_a]       = promise::Create<T>();

      auto raced_a = source_a.Race(std::move(race_a), resolve_race_a, reject_race_a);
      source_a.WaitAwaited(0);
      REQUIRE(source_a.Awaiters() == 1);

      REQUIRE((*resolve_race_a)(test_types::ValueFromInt<T>(500)));
      REQUIRE_FALSE((*reject_race_a)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(raced_a.Resolved());
      REQUIRE(raced_a.Value() == test_types::ValueFromInt<T>(500));
      REQUIRE(source_a.Awaiters() == 0);

      REQUIRE((*resolve_source_a)(test_types::ValueFromInt<T>(100)));
      REQUIRE_FALSE((*reject_source_a)(std::make_exception_ptr(TestError{"ignored"})));

      auto [source_b, resolve_source_b, reject_source_b] = promise::Create<T>();
      auto [race_b, resolve_race_b, reject_race_b]       = promise::Create<T>();

      auto raced_b = source_b.Race(std::move(race_b), resolve_race_b, reject_race_b);
      source_b.WaitAwaited(0);
      REQUIRE(source_b.Awaiters() == 1);

      REQUIRE((*resolve_source_b)(test_types::ValueFromInt<T>(777)));
      REQUIRE_FALSE((*reject_source_b)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(raced_b.Resolved());
      REQUIRE(raced_b.Value() == test_types::ValueFromInt<T>(777));

      REQUIRE_FALSE((*resolve_race_b)(test_types::ValueFromInt<T>(123)));
      REQUIRE_FALSE((*reject_race_b)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch async exception_ptr and lvalue-ref pending paths",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto done_ptr_async = Promise<T>::template Reject<TestError>("done ptr async")
                              .Catch([](std::exception_ptr exception) -> Promise<void> {
                                 RequireException<TestError>(exception);
                                 co_return;
                              });
      REQUIRE(done_ptr_async.Resolved());
      REQUIRE_FALSE(done_ptr_async.Value().has_value());

      auto [pending_ptr, resolve_ptr, reject_ptr] = promise::Create<T>();
      auto pending_ptr_async = pending_ptr.Catch([](std::exception_ptr exception) -> Promise<void> {
         RequireException<TestError>(exception);
         co_return;
      });
      pending_ptr.WaitAwaited(0);
      REQUIRE((*reject_ptr)(std::make_exception_ptr(TestError{"pending ptr async"})));
      REQUIRE_FALSE((*resolve_ptr)(test_types::ValueFromInt<T>(1)));
      REQUIRE(pending_ptr_async.Resolved());
      REQUIRE_FALSE(pending_ptr_async.Value().has_value());

      auto [gate, resolve_gate, reject_gate]         = promise::Create<void>();
      auto [pending_lref, resolve_lref, reject_lref] = promise::Create<T>();
      auto pending_lref_async = pending_lref.Catch([&](TestError const& exception) -> Promise<T> {
         REQUIRE(std::string{exception.what()} == "pending lref async");
         co_await gate;
         co_return test_types::ValueFromInt<T>(909);
      });

      pending_lref.WaitAwaited(0);
      REQUIRE((*reject_lref)(std::make_exception_ptr(TestError{"pending lref async"})));
      REQUIRE_FALSE((*resolve_lref)(test_types::ValueFromInt<T>(2)));
      REQUIRE_FALSE(pending_lref_async.Done());
      REQUIRE((*resolve_gate)());
      REQUIRE_FALSE((*reject_gate)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending_lref_async.Resolved());
      REQUIRE(pending_lref_async.Value() == test_types::ValueFromInt<T>(909));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch typed mismatch preserves original exception",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto done_mismatch =
        Promise<T>::template Reject<CatchError>("mismatch done").Catch([](TestError const&) {
           return test_types::ValueFromInt<T>(1);
        });
      REQUIRE(done_mismatch.Rejected());
      RequireException<CatchError>(done_mismatch.Exception());

      auto [pending_mismatch, resolve_mismatch, reject_mismatch] = promise::Create<T>();
      auto pending_chain =
        pending_mismatch.Catch([](TestError const&) { return test_types::ValueFromInt<T>(2); });

      pending_mismatch.WaitAwaited(0);
      REQUIRE((*reject_mismatch)(std::make_exception_ptr(CatchError{"mismatch pending"})));
      REQUIRE_FALSE((*resolve_mismatch)(test_types::ValueFromInt<T>(99)));
      REQUIRE(pending_chain.Rejected());
      RequireException<CatchError>(pending_chain.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Finally async on done resolved can replace with finally exception",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto resolved_then_finally_reject =
        Promise<T>::Resolve(test_types::ValueFromInt<T>(123)).Finally([]() -> Promise<void> {
           throw FinallyError{"finally async done reject"};
           co_return;
        });

      REQUIRE(resolved_then_finally_reject.Rejected());
      RequireException<FinallyError>(resolved_then_finally_reject.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch typed mismatch with async handler keeps original rejection",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto done_async_mismatch =
        Promise<T>::template Reject<CatchError>("async mismatch")
          .Catch([](TestError const&) -> Promise<T> { co_return test_types::ValueFromInt<T>(1); });

      REQUIRE(done_async_mismatch.Rejected());
      RequireException<CatchError>(done_async_mismatch.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch promise-returning passthrough on resolved sources",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto done_value_passthrough =
        Promise<T>::Resolve(test_types::ValueFromInt<T>(21))
          .Catch([](std::exception_ptr) -> Promise<void> { co_return; });
      REQUIRE(done_value_passthrough.Resolved());
      REQUIRE(done_value_passthrough.Value().has_value());
      REQUIRE(done_value_passthrough.Value().value() == test_types::ValueFromInt<T>(21));

      auto done_void_passthrough =
        Promise<void>::Resolve().Catch([](std::exception_ptr) -> Promise<T> {
           co_return test_types::ValueFromInt<T>(7);
        });
      REQUIRE(done_void_passthrough.Resolved());
      REQUIRE_FALSE(done_void_passthrough.Value().has_value());

      auto [pending_value, resolve_value, reject_value] = promise::Create<T>();
      auto pending_value_passthrough =
        pending_value.Catch([](std::exception_ptr) -> Promise<void> { co_return; });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(test_types::ValueFromInt<T>(33)));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending_value_passthrough.Resolved());
      REQUIRE(pending_value_passthrough.Value().has_value());
      REQUIRE(pending_value_passthrough.Value().value() == test_types::ValueFromInt<T>(33));

      auto [pending_void, resolve_void, reject_void] = promise::Create<void>();
      auto pending_void_passthrough = pending_void.Catch([](std::exception_ptr) -> Promise<T> {
         co_return test_types::ValueFromInt<T>(9);
      });
      pending_void.WaitAwaited(0);
      REQUIRE((*resolve_void)());
      REQUIRE_FALSE((*reject_void)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending_void_passthrough.Resolved());
      REQUIRE_FALSE(pending_void_passthrough.Value().has_value());
   });
}
