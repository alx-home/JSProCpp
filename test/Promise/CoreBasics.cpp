#include "../TestCommon.h"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "Promise resolves with value",
  "[Promise][Core]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = Promise<T>::Resolve(test_types::ValueFromInt<T>(42));

      REQUIRE(p.Done());
      REQUIRE(p.Resolved());
      REQUIRE_FALSE(p.Rejected());
      REQUIRE(p.Value() == test_types::ValueFromInt<T>(42));
   });
}

TEST_CASE("Promise resolves with void", "[Promise][Core]") {
   RunWithTimeout(2s, [&] {
      auto p = Promise<void>::Resolve();

      REQUIRE(p.Done());
      REQUIRE(p.Resolved());
      REQUIRE_FALSE(p.Rejected());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Promise rejects with exception",
  "[Promise][Core]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = Promise<T>::template Reject<TestError>("boom");

      REQUIRE(p.Done());
      REQUIRE_FALSE(p.Resolved());
      REQUIRE(p.Rejected());
      RequireException<TestError>(p.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Then transforms a resolved value",
  "[Promise][Core]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = Promise<T>::Resolve(test_types::ValueFromInt<T>(21)).Then([](T const& value) {
         return test_types::BumpValue<T>(value, 21);
      });

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::BumpValue<T>(test_types::ValueFromInt<T>(21), 21));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch recovers from rejection",
  "[Promise][Core]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p = Promise<T>::template Reject<TestError>("boom").Catch([](std::exception_ptr const&) {
         return test_types::ValueFromInt<T>(42);
      });

      REQUIRE(p.Resolved());
      REQUIRE(p.Value() == test_types::ValueFromInt<T>(42));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Catch preserves exception when rethrowing",
  "[Promise][Core]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto p =
        Promise<T>::template Reject<TestError>("boom").Catch([](std::exception_ptr exception) -> T {
           std::rethrow_exception(exception);
        });

      REQUIRE(p.Rejected());
      RequireException<TestError>(p.Exception());
   });
}
