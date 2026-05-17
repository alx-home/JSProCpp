#include "../TestCommon.h"

using namespace std::chrono_literals;

TEST_CASE("Pool and MessageQueue reject dispatch after stop", "[Pool][MessageQueue]") {
   promise::Pool<2>      pool{"promise-pool-test"};
   promise::MessageQueue queue{"promise-queue-stopped"};

   WPromise flow{[&]() -> Promise<void> {
      auto pool_value = pool.Dispatch([] { return 9; });

      REQUIRE(co_await pool_value == 9);
      REQUIRE(pool_value.Resolved());
      REQUIRE(pool_value.Value() == 9);

      auto delayed = pool.Dispatch(1ms);
      co_await delayed;
      REQUIRE(delayed.Resolved());

      co_return;
   }};

   flow.WaitDone();
   REQUIRE(flow.Resolved());

   pool.Stop();
   auto stopped_pool_call = pool.Dispatch([] { return 3; });
   REQUIRE(stopped_pool_call.Rejected());

   queue.Stop();
   auto ensure_after_stop = queue.Ensure();
   REQUIRE(ensure_after_stop.Rejected());
}
