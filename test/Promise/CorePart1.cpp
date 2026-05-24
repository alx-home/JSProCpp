#include "../TestCommon.h"

#include <catch2/catch_test_macros.hpp>
#include <exception>

TEMPLATE_LIST_TEST_CASE(
  "Then covers done and pending void/value combinations",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto done_void_to_value =
        Promise<void>::Resolve().Then([] { return test_types::ValueFromInt<T>(7); });
      REQUIRE(done_void_to_value.Resolved());
      REQUIRE(done_void_to_value.Value() == test_types::ValueFromInt<T>(7));

      auto done_value_to_void =
        Promise<T>::Resolve(test_types::ValueFromInt<T>(3)).Then([](T value) {
           REQUIRE(value == test_types::ValueFromInt<T>(3));
        });
      REQUIRE(done_value_to_void.Resolved());

      auto [pending_void, resolve_void, reject_void] = promise::Create<void>();
      auto pending_void_to_value =
        pending_void.Then([] { return test_types::ValueFromInt<T>(11); });
      pending_void.WaitAwaited(0);
      REQUIRE((*resolve_void)());
      REQUIRE_FALSE((*reject_void)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending_void_to_value.Resolved());
      REQUIRE(pending_void_to_value.Value() == test_types::ValueFromInt<T>(11));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Then promise-returning continuations cover pending paths",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_value, resolve_value, reject_value] = promise::Create<T>();
      auto chained_async = pending_value.Then([](T value) -> Promise<T> {
         co_return test_types::BumpValue(value, 5);
      });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(test_types::ValueFromInt<T>(10)));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(chained_async.Resolved());
      REQUIRE(chained_async.Value() == test_types::BumpValue(test_types::ValueFromInt<T>(10), 5));

      auto rejected_passthrough =
        Promise<T>::template Reject<TestError>("then reject").Then([](T) -> Promise<T> {
           co_return test_types::ValueFromInt<T>(0);
        });
      REQUIRE(rejected_passthrough.Rejected());
      RequireException<TestError>(rejected_passthrough.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch covers done and pending value forwarding",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto done_rejected_to_value =
        Promise<void>::Reject<TestError>("catch").Catch([](TestError const&) {
           return test_types::ValueFromInt<T>(9);
        });
      REQUIRE(done_rejected_to_value.Resolved());
      REQUIRE(done_rejected_to_value.Value() == test_types::ValueFromInt<T>(9));

      auto [pending_value, resolve_value, reject_value] = promise::Create<T>();
      auto passthrough =
        pending_value.Catch([](TestError const&) { return test_types::ValueFromInt<T>(0); });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(test_types::ValueFromInt<T>(21)));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(passthrough.Resolved());
      REQUIRE(passthrough.Value() == test_types::ValueFromInt<T>(21));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch with async lvalue-ref exception handler path",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto recovered = Promise<T>::template Reject<TestError>("lvalue async")
                         .Catch([](TestError const& exception) -> Promise<T> {
                            REQUIRE(std::string{exception.what()} == "lvalue async");
                            co_return test_types::ValueFromInt<T>(123);
                         });

      REQUIRE(recovered.Resolved());
      REQUIRE(recovered.Value() == test_types::ValueFromInt<T>(123));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Finally covers lvalue and rvalue pending/done branches",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      bool done_cleanup_called = false;
      auto done_chain          = Promise<T>::Resolve(test_types::ValueFromInt<T>(4)).Finally([&] {
         done_cleanup_called = true;
      });
      REQUIRE(done_chain.Resolved());
      REQUIRE(done_chain.Value() == test_types::ValueFromInt<T>(4));
      REQUIRE(done_cleanup_called);

      auto [pending_value, resolve_value, reject_value] = promise::Create<T>();
      bool pending_cleanup_called                       = false;
      auto pending_chain = pending_value.Finally([&] { pending_cleanup_called = true; });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(test_types::ValueFromInt<T>(8)));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(pending_chain.Resolved());
      REQUIRE(pending_chain.Value() == test_types::ValueFromInt<T>(8));
      REQUIRE(pending_cleanup_called);

      auto [pending_reject, resolve_reject, reject_reject] = promise::Create<T>();
      bool reject_cleanup_called                           = false;
      auto reject_chain = std::move(pending_reject).Finally([&] { reject_cleanup_called = true; });
      REQUIRE((*reject_reject)(std::make_exception_ptr(TestError{"finally reject"})));
      REQUIRE_FALSE((*resolve_reject)(test_types::ValueFromInt<T>(1)));
      REQUIRE(reject_chain.Rejected());
      REQUIRE(reject_cleanup_called);
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Finally async continuation on pending paths",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_value, resolve_value, reject_value] = promise::Create<T>();
      auto async_finally_value = pending_value.Finally([]() -> Promise<void> { co_return; });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(test_types::ValueFromInt<T>(14)));
      REQUIRE_FALSE((*reject_value)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(async_finally_value.Resolved());
      REQUIRE(async_finally_value.Value() == test_types::ValueFromInt<T>(14));

      auto [pending_reject, resolve_reject, reject_reject] = promise::Create<T>();
      auto async_finally_reject = pending_reject.Finally([]() -> Promise<void> { co_return; });
      pending_reject.WaitAwaited(0);
      REQUIRE((*reject_reject)(std::make_exception_ptr(TestError{"async finally reject"})));
      REQUIRE_FALSE((*resolve_reject)(test_types::ValueFromInt<T>(2)));
      REQUIRE(async_finally_reject.Rejected());
      RequireException<TestError>(async_finally_reject.Exception());
   });
}
