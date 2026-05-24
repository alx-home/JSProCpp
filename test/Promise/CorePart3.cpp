#include "../TestCommon.h"

#include <catch2/catch_test_macros.hpp>
#include <exception>

TEMPLATE_LIST_TEST_CASE(
  "Pending Finally branches cover async success and failure",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_value, resolve_value, reject_value] = promise::Create<T>();
      auto finally_async_fail = pending_value.Finally([]() -> Promise<void> {
         throw FinallyError{"pending finally async failure"};
         co_return;
      });
      pending_value.WaitAwaited(0);
      REQUIRE((*resolve_value)(test_types::ValueFromInt<T>(9)));
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

TEMPLATE_LIST_TEST_CASE(
  "Type-erased VAwait covers pending and rejection paths",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_ok, resolve_ok, reject_ok] = promise::Create<T>();
      auto erased_ok  = std::move(pending_ok).template ToPointer<promise::VPromise>();
      bool resumed_ok = false;

      auto waiter_ok = WPromise{[&]() -> Promise<void> {
         co_await erased_ok->VAwait();
         resumed_ok = true;
         co_return;
      }};

      REQUIRE_FALSE(waiter_ok.Done());
      REQUIRE((*resolve_ok)(test_types::ValueFromInt<T>(17)));
      REQUIRE_FALSE((*reject_ok)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(waiter_ok.Resolved());
      REQUIRE(resumed_ok);

      auto [pending_reject, resolve_reject, reject_reject] = promise::Create<T>();
      auto erased_reject = std::move(pending_reject).template ToPointer<promise::VPromise>();
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
      REQUIRE_FALSE((*resolve_reject)(test_types::ValueFromInt<T>(99)));
      REQUIRE(waiter_reject.Resolved());
      REQUIRE(caught_reject);
   });
}

TEMPLATE_LIST_TEST_CASE(
  "WaitAwaited default current-count branch is exercised",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<T>();

      std::promise<T> chained_result_promise;
      auto            chained_result = chained_result_promise.get_future();

      std::thread worker([&] {
         auto chained = source.Then([](T value) { return test_types::BumpValue(value, 30); });
         chained.WaitDone();
         if (chained.Resolved()) {
            chained_result_promise.set_value(chained.Value());
         } else {
            chained_result_promise.set_value(test_types::ValueFromInt<T>(-1));
         }
      });

      source.WaitAwaited();
      REQUIRE(source.Awaiters() == 1);

      REQUIRE((*resolve)(test_types::ValueFromInt<T>(12)));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(chained_result.get() == test_types::BumpValue(test_types::ValueFromInt<T>(12), 30));

      worker.join();
   });
}

TEMPLATE_LIST_TEST_CASE(
  "WPromise Race lvalue and rvalue dispatch variants",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending_lvalue, resolve_lvalue, reject_lvalue] = promise::Create<T>();
      auto source_lvalue = WPromise<T>::Resolve(test_types::ValueFromInt<T>(5));
      auto raced_lvalue =
        source_lvalue.Race(std::move(pending_lvalue), resolve_lvalue, reject_lvalue);

      REQUIRE(raced_lvalue.Resolved());
      REQUIRE(raced_lvalue.Value() == test_types::ValueFromInt<T>(5));
      REQUIRE_FALSE((*resolve_lvalue)(test_types::ValueFromInt<T>(44)));
      REQUIRE_FALSE((*reject_lvalue)(std::make_exception_ptr(TestError{"ignored"})));

      auto [pending_rvalue, resolve_rvalue, reject_rvalue] = promise::Create<T>();
      auto raced_rvalue = WPromise<T>::Resolve(test_types::ValueFromInt<T>(6))
                            .Race(std::move(pending_rvalue), resolve_rvalue, reject_rvalue);

      REQUIRE(raced_rvalue.Resolved());
      REQUIRE(raced_rvalue.Value() == test_types::ValueFromInt<T>(6));
      REQUIRE_FALSE((*resolve_rvalue)(test_types::ValueFromInt<T>(45)));
      REQUIRE_FALSE((*reject_rvalue)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "WPromise co_await covers pending, const-lvalue, and rejection paths",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [gate, resolve_gate, reject_gate] = promise::Create<void>();

      auto source_pending = WPromise{[&]() -> Promise<T> {
         co_await gate;
         co_return test_types::ValueFromInt<T>(31);
      }};

      auto waiter_pending = WPromise{[&]() -> Promise<T> {
         T value = co_await source_pending;
         co_return test_types::BumpValue(value, 1);
      }};

      REQUIRE_FALSE(waiter_pending.Done());
      REQUIRE((*resolve_gate)());
      REQUIRE_FALSE((*reject_gate)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(waiter_pending.Resolved());
      REQUIRE(waiter_pending.Value() == test_types::BumpValue(test_types::ValueFromInt<T>(31), 1));

      auto source_reject = WPromise{[]() -> Promise<T> {
         throw TestError{"co_await reject"};
         co_return test_types::ValueFromInt<T>(0);
      }};

      auto waiter_reject = WPromise{[&]() -> Promise<T> {
         try {
            (void)(co_await source_reject);
            co_return test_types::ValueFromInt<T>(-1);
         } catch (TestError const&) {
            co_return test_types::ValueFromInt<T>(77);
         }
      }};

      REQUIRE(waiter_reject.Resolved());
      REQUIRE(waiter_reject.Value() == test_types::ValueFromInt<T>(77));

      auto source_const =
        WPromise{[]() -> Promise<T> { co_return test_types::ValueFromInt<T>(44); }};
      const auto const_source{source_const};

      auto waiter_const = WPromise{[&]() -> Promise<T> {
         T value = co_await const_source;
         co_return value;
      }};

      REQUIRE(waiter_const.Resolved());
      REQUIRE(waiter_const.Value() == test_types::ValueFromInt<T>(44));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "WPromise destructor waits for pending completion",
  "[Promise][Core]",
  test_types::Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [pending, resolve, reject] = promise::Create<T>();

      std::thread resolver([resolve = resolve] {
         std::this_thread::sleep_for(20ms);
         (void)(*resolve)(test_types::ValueFromInt<T>(88));
      });

      {
         auto pending_copy = pending;
      }

      resolver.join();
      REQUIRE(pending.Resolved());
      REQUIRE(pending.Value() == test_types::ValueFromInt<T>(88));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
   });
}
