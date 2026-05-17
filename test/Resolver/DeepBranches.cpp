#include "../TestCommon.h"

TEST_CASE("Make promise covers resolver signatures", "[Resolver][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto with_resolve_and_reject =
        MakePromise([](Resolve<int> const& resolve, Reject const&) -> Promise<int, true> {
           REQUIRE(resolve(11));
           co_return;
        });
      REQUIRE(with_resolve_and_reject.Resolved());
      REQUIRE(with_resolve_and_reject.Value() == 11);

      auto with_resolve_and_arg = MakePromise(
        [](Resolve<int> const& resolve, int base) -> Promise<int, true> {
           REQUIRE(resolve(base + 1));
           co_return;
        },
        20
      );
      REQUIRE(with_resolve_and_arg.Resolved());
      REQUIRE(with_resolve_and_arg.Value() == 21);

      auto promise_arg = MakePromise([](int base) -> Promise<int> { co_return base * 2; }, 12);
      REQUIRE(promise_arg.Resolved());
      REQUIRE(promise_arg.Value() == 24);

      auto promise_no_arg = MakePromise([]() -> Promise<int> { co_return 7; });
      REQUIRE(promise_no_arg.Resolved());
      REQUIRE(promise_no_arg.Value() == 7);
   });
}
