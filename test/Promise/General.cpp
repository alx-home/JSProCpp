#include "../TestCommon.h"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "Then Catch Then stays rejected when the final Then throws",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = Promise<T>::template Reject<TestError>("initial failure")
                 .Then([](T const& value) { return test_types::BumpValue<T>(value, 1); })
                 .Catch([](TestError const& exception) {
                    REQUIRE(std::string{exception.what()} == "initial failure");
                    return test_types::ValueFromInt<T>(5);
                 })
                 .Then([](T const&) -> T { throw FinalThenError("final then failed"); });

      REQUIRE(p.Rejected());
      RequireException<FinalThenError>(p.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Async catch-through chain preserves old promise_test behavior",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p =
        WPromise{[]() -> Promise<T> { co_return test_types::ValueFromInt<T>(0); }}
          .Then([](T const& value) -> Promise<T> { co_return test_types::BumpValue<T>(value, 3); })
          .Catch([](std::exception_ptr) -> Promise<void> { co_return; })
          .Then([](std::optional<T> const&) -> Promise<T> {
             co_return test_types::ValueFromInt<T>(0);
          })
          .Catch([](std::exception_ptr) -> Promise<T> { co_return test_types::ValueFromInt<T>(0); })
          .Then([](T const&) -> Promise<void> { co_return; })
          .Catch([](std::exception_ptr) -> Promise<T> { co_return test_types::ValueFromInt<T>(0); })
          .Then([](std::optional<T> const&) -> Promise<void> { co_return; })
          .Catch([](std::exception_ptr) -> Promise<void> { co_return; })
          .Then([]() -> Promise<void> { co_return; })
          .Then([]() -> Promise<T> { co_return test_types::ValueFromInt<T>(800); });

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::ValueFromInt<T>(800));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Sync catch-through chain preserves old promise_test behavior",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = WPromise{[]() -> Promise<T> { co_return test_types::ValueFromInt<T>(0); }}
                 .Then([](T const& value) { return test_types::BumpValue<T>(value, 3); })
                 .Catch([](std::exception_ptr) {})
                 .Then([](std::optional<T> const&) { return test_types::ValueFromInt<T>(0); })
                 .Catch([](std::exception_ptr) { return test_types::ValueFromInt<T>(0); })
                 .Then([](T const&) {})
                 .Catch([](std::exception_ptr) { return test_types::ValueFromInt<T>(0); })
                 .Then([](std::optional<T> const&) {})
                 .Catch([](std::exception_ptr) {})
                 .Then([]() {})
                 .Then([]() { return test_types::ValueFromInt<T>(800); });

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::ValueFromInt<T>(800));
   });
}

TEST_CASE("Complex Then Catch Finally chain matches old promise_test flow", "[Promise]") {
   RunWithTimeout(2s, [&] {
      auto p = Promise<int>::Resolve(6)
                 .Then([](int value) -> Promise<double> {
                    throw TestError("test");
                    co_return value + 3;
                 })
                 .Then([](double value) -> Promise<double> { co_return value; })
                 .Catch([](std::exception_ptr) -> Promise<int> { co_return 300; })
                 .Finally([]() {})
                 .Then([](Matrix::VariantIntDouble const&) -> Promise<double> {
                    throw CatchError("test3");
                    co_return 0.0;
                 })
                 .Finally([]() {})
                 .Catch([](std::exception_ptr) -> Promise<double> { co_return 300; })
                 .Finally([]() {})
                 .Finally([]() { throw FinallyError("test55"); })
                 .Catch([](FinallyError const&) -> Promise<double> { co_return 300; })
                 .Then([](Resolve<int> const& resolve, double value) -> Promise<int, true> {
                    REQUIRE(resolve(static_cast<int>(value) + 3));
                    co_return;
                 });

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == 303);
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Finally runs on resolved promise",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      bool cleanup_ran = false;

      auto p = Promise<T>::Resolve(test_types::ValueFromInt<T>(7)).Finally([&cleanup_ran]() {
         cleanup_ran = true;
      });

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::ValueFromInt<T>(7));
      REQUIRE(cleanup_ran);
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Finally runs on rejected promise",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      bool cleanup_ran = false;

      auto p = Promise<T>::template Reject<TestError>("boom").Finally([&cleanup_ran]() {
         cleanup_ran = true;
      });

      REQUIRE(p.Rejected());
      REQUIRE(cleanup_ran);
      RequireException<TestError>(p.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "promise::Create resolves externally",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [p, resolve, reject] = promise::Create<T>();

      REQUIRE_FALSE(p.Done());
      REQUIRE_FALSE(*reject);

      REQUIRE((*resolve)(test_types::ValueFromInt<T>(99)));
      REQUIRE_FALSE((*resolve)(test_types::ValueFromInt<T>(100)));
      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::ValueFromInt<T>(99));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "promise::Create rejects externally",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [p, resolve, reject] = promise::Create<T>();

      REQUIRE_FALSE(p.Done());
      REQUIRE((*reject)(std::make_exception_ptr(TestError{"boom"})));
      REQUIRE_FALSE((*resolve)(test_types::ValueFromInt<T>(99)));
      REQUIRE(p.Rejected());
      RequireException<TestError>(p.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "MakePromise wraps synchronous and coroutine callables",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto sync = MakePromise([] { return test_types::ValueFromInt<T>(12); });
      auto coroutine =
        MakePromise([]() -> Promise<T> { co_return test_types::ValueFromInt<T>(30); });

      REQUIRE(sync.Resolved());
      REQUIRE(sync.Value() == test_types::ValueFromInt<T>(12));
      REQUIRE(coroutine.Resolved());
      REQUIRE(coroutine.Value() == test_types::ValueFromInt<T>(30));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Direct coroutine promises resolve",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = MakePromise(
        [](T value) -> Promise<T> { co_return value; }, test_types::ValueFromInt<T>(15)
      );
      auto v = MakePromise(CoroutineVoid);

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::ValueFromInt<T>(15));
      REQUIRE(v.Resolved());
   });
}

TEST_CASE("Race returns the first resolved alternative", "[Promise]") {
   RunWithTimeout(2s, [&] {
      auto [pending, resolve, reject] = promise::Create<std::string>();
      auto raced                      = promise::Race(Promise<int>::Resolve(7), pending);

      REQUIRE(raced.Resolved());
      REQUIRE(std::holds_alternative<int>(raced.Value()));
      REQUIRE(std::get<int>(raced.Value()) == 7);

      REQUIRE((*resolve)("late"));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEST_CASE("Race rejection flows through Catch into optional variant", "[Promise]") {
   RunWithTimeout(2s, [&] {
      auto [delay, resolve, reject] = promise::Create<void>();

      auto raced =
        promise::Race(
          WPromise{[]() -> Promise<double> {
             throw TestError("race failure");
             co_return 0.0;
          }},
          WPromise{[&delay]() -> Promise<double> {
             co_await delay;
             co_return 2.0;
          }},
          WPromise{[&delay]() -> Promise<int> {
             co_await delay;
             co_return 3;
          }}
        )
          .Catch([](TestError const& exception) {
             REQUIRE(std::string{exception.what()} == "race failure");
          })
          .Then([](Matrix::OptionalVariantDoubleInt const& value) { return !value.has_value(); });

      REQUIRE(raced.Resolved());
      REQUIRE(raced.Value());

      REQUIRE((*resolve)());
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Resolver Create<true> returns resolve reject handles for external completion",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      std::function<Promise<T, true>(Resolve<T> const&, Reject const&)> resolver_factory =
        [](Resolve<T> const&, Reject const&) -> Promise<T, true> { co_return; };

      auto [promise_ok, resolve_ok, reject_ok] =
        promise::details::Promise<T, true>::template Create<true>(resolver_factory);

      REQUIRE_FALSE(promise_ok.Done());
      REQUIRE((*resolve_ok)(test_types::ValueFromInt<T>(101)));
      REQUIRE_FALSE((*reject_ok)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(promise_ok.Resolved());
      REQUIRE(promise_ok.Value() == test_types::ValueFromInt<T>(101));

      auto [promise_fail, resolve_fail, reject_fail] =
        promise::details::Promise<T, true>::template Create<true>(resolver_factory);

      REQUIRE_FALSE(promise_fail.Done());
      REQUIRE((*reject_fail)(std::make_exception_ptr(TestError{"rpromise reject"})));
      REQUIRE_FALSE((*resolve_fail)(test_types::ValueFromInt<T>(55)));

      REQUIRE(promise_fail.Rejected());
      RequireException<TestError>(promise_fail.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "ToPointer exposes type-erased awaiting",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto pointer = WPromise{[]() -> Promise<void> { co_return; }}.ToPointer();

      auto waiter = WPromise{[pointer]() -> Promise<T> {
         co_await pointer->VAwait();
         co_return test_types::ValueFromInt<T>(1);
      }};

      REQUIRE(waiter.Done());

      REQUIRE(waiter.Resolved());
      REQUIRE(waiter.Value() == test_types::ValueFromInt<T>(1));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Pending Then registration updates awaiter counters",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<T>();

      REQUIRE(source.Awaiters() == 0);
      REQUIRE(source.UseCount() == 0);

      auto chained = source.Then([](T const& value) { return test_types::BumpValue<T>(value, 1); });

      source.WaitAwaited(0);
      REQUIRE(source.Awaiters() == 1);
      REQUIRE(source.UseCount() >= 1);

      REQUIRE((*resolve)(test_types::ValueFromInt<T>(10)));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

      REQUIRE(chained.Resolved());
      REQUIRE(chained.Value() == test_types::BumpValue<T>(test_types::ValueFromInt<T>(10), 1));
      REQUIRE(source.Awaiters() == 0);
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Pending Catch registration recovers once rejected",
  "[Promise]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<T>();

      auto recovered = source.Catch([](TestError const& exception) {
         REQUIRE(std::string{exception.what()} == "pending failure");
         return test_types::ValueFromInt<T>(42);
      });

      source.WaitAwaited(0);
      REQUIRE(source.Awaiters() == 1);

      REQUIRE((*reject)(std::make_exception_ptr(TestError{"pending failure"})));
      REQUIRE_FALSE((*resolve)(test_types::ValueFromInt<T>(7)));

      REQUIRE(recovered.Resolved());
      REQUIRE(recovered.Value() == test_types::ValueFromInt<T>(42));
   });
}
