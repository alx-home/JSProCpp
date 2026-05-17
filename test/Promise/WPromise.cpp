#include "../TestCommon.h"

TEST_CASE("WPromise static helpers and detach APIs", "[Promise][WPromise]") {
   auto resolved = WPromise<int>::Resolve(9);
   REQUIRE(resolved.Resolved());
   REQUIRE(resolved.Value() == 9);

   auto rejected = WPromise<int>::Reject<TestError>("wreject");
   REQUIRE(rejected.Rejected());
   RequireException<TestError>(rejected.Exception());

   auto [pending, resolve, reject] = WPromise<int>::Create();
   REQUIRE_FALSE(pending.Done());
   REQUIRE((*resolve)(12));
   REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
   REQUIRE(pending.Resolved());
   REQUIRE(pending.Value() == 12);

   auto detached = WPromise{[]() -> Promise<void> { co_return; }};
   std::move(detached).Detach();

   auto vdetached = WPromise{[]() -> Promise<void> { co_return; }};
   std::move(vdetached).VDetach();
}
