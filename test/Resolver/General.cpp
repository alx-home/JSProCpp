#include "../TestCommon.h"

#include "promise/details/helpers.inl"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "MakePromise supports resolver-first synchronous callables",
  "[Resolver]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto resolved = MakePromise([](Resolve<T> const& resolve) {
         REQUIRE(resolve(test_types::ValueFromInt<T>(55)));
      });

      auto rejected = MakePromise([](Resolve<T> const&, Reject const& reject) {
         reject.Apply<TestError>("reject through resolver callable");
      });

      REQUIRE(resolved.Resolved());
      REQUIRE(resolved.Value() == test_types::ValueFromInt<T>(55));

      REQUIRE(rejected.Rejected());
      RequireException<TestError>(rejected.Exception());
   });
}
