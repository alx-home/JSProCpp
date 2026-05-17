#include "../TestCommon.h"

TEST_CASE("MakePromise supports resolver-first synchronous callables", "[Resolver]") {
   auto resolved = MakePromise([](Resolve<int> const& resolve) { REQUIRE(resolve(55)); });

   auto rejected = MakePromise([](Resolve<int> const&, Reject const& reject) {
      reject.Apply<TestError>("reject through resolver callable");
   });

   REQUIRE(resolved.Resolved());
   REQUIRE(resolved.Value() == 55);

   REQUIRE(rejected.Rejected());
   RequireException<TestError>(rejected.Exception());
}
