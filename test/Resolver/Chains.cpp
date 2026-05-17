#include "../TestCommon.h"

TEST_CASE(
  "Then supports resolver-based continuations across value and void",
  "[Resolver][Chains]"
) {
   auto p =
     WPromise{[]() -> Promise<int> { co_return 999; }}
       .Then([](int value) -> Promise<void> {
          REQUIRE(value == 999);
          co_return;
       })
       .Then([]() -> Promise<void> { co_return; })
       .Then([](Resolve<int> const& resolve, Reject const&) -> Promise<int, true> {
          REQUIRE(resolve(111));
          co_return;
       })
       .Then([](Resolve<void> const& resolve, Reject const&, int value) -> Promise<void, true> {
          REQUIRE(value == 111);
          REQUIRE(resolve());
          co_return;
       })
       .Then([]() -> Promise<int> { co_return 888; });

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 888);
}
