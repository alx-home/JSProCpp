#include "../TestCommon.h"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "Reject callback ignores duplicate invocation",
  "[Promise][Reject]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto [promise, resolve, reject] = promise::Create<T>();

      REQUIRE((*reject)(std::make_exception_ptr(TestError{"first reject"})));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"second reject"})));
      REQUIRE_FALSE((*resolve)(test_types::ValueFromInt<T>(5)));

      REQUIRE(promise.Rejected());
      RequireException<TestError>(promise.Exception());
   });
}
