#include "../TestCommon.h"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "Then can chain coroutine promises",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p =
        Promise<T>::Resolve(test_types::ValueFromInt<T>(21)).Then([](T const& value) -> Promise<T> {
           co_return test_types::BumpValue<T>(value, 21);
        });

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::BumpValue<T>(test_types::ValueFromInt<T>(21), 21));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Then Catch Then passes through when no exception is raised",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = Promise<T>::Resolve(test_types::ValueFromInt<T>(4))
                 .Then([](T const& value) { return test_types::BumpValue<T>(value, 1); })
                 .Catch([](std::exception_ptr const&) { return test_types::ValueFromInt<T>(0); })
                 .Then([](T const& value) { return test_types::BumpValue<T>(value, 5); });

      REQUIRE(p.Resolved());
      REQUIRE(
        p.Value()
        == test_types::BumpValue<T>(test_types::BumpValue<T>(test_types::ValueFromInt<T>(4), 1), 5)
      );
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Then Catch Then recovers when the first Then throws",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = Promise<T>::Resolve(test_types::ValueFromInt<T>(4))
                 .Then([](T const&) -> T { throw TestError("then failed"); })
                 .Catch([](TestError const& exception) {
                    REQUIRE(std::string{exception.what()} == "then failed");
                    return test_types::ValueFromInt<T>(7);
                 })
                 .Then([](T const& value) { return test_types::BumpValue<T>(value, 14); });

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::BumpValue<T>(test_types::ValueFromInt<T>(7), 14));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Then Catch Then stays rejected when Catch rethrows",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p =
        Promise<T>::Resolve(test_types::ValueFromInt<T>(4))
          .Then([](T const&) -> T { throw TestError("then failed"); })
          .Catch([](std::exception_ptr exception) -> T { std::rethrow_exception(exception); })
          .Then([](T const& value) { return test_types::BumpValue<T>(value, 2); });

      REQUIRE(p.Rejected());
      RequireException<TestError>(p.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Then Catch Then stays rejected when Catch throws a new exception",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = Promise<T>::template Reject<TestError>("initial failure")
                 .Then([](T const& value) { return test_types::BumpValue<T>(value, 1); })
                 .Catch([](TestError const& exception) -> T {
                    REQUIRE(std::string{exception.what()} == "initial failure");
                    throw CatchError("catch failed");
                 })
                 .Then([](T const& value) { return test_types::BumpValue<T>(value, 2); });

      REQUIRE(p.Rejected());
      RequireException<CatchError>(p.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Rvalue chaining exercises moved Then Catch Finally overloads",
  "[Promise][Chains]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = WPromise{[]() -> Promise<T> { co_return test_types::ValueFromInt<T>(6); }};

      auto moved_chain = std::move(source)
                           .Then([](T const& value) { return test_types::BumpValue<T>(value, 1); })
                           .Catch([](std::exception_ptr) { return test_types::ValueFromInt<T>(0); })
                           .Finally([] {});

      REQUIRE(moved_chain.Resolved());
      REQUIRE(moved_chain.Value() == test_types::BumpValue<T>(test_types::ValueFromInt<T>(6), 1));

      auto source_reject = WPromise{[]() -> Promise<T> {
         throw TestError{"moved reject"};
         co_return test_types::ValueFromInt<T>(0);
      }};

      auto moved_recover =
        std::move(source_reject)
          .Then([](T const& value) { return test_types::BumpValue<T>(value, 10); })
          .Catch([](TestError const&) { return test_types::ValueFromInt<T>(77); })
          .Finally([] {});

      REQUIRE(moved_recover.Resolved());
      REQUIRE(moved_recover.Value() == test_types::ValueFromInt<T>(77));
   });
}
