#include "../TestCommon.h"
#include <utils/Pool.h>

TEST_CASE("MessageQueue exposes a running worker thread", "[Pool]") {
   RunWithTimeout(2s, [&] {
      promise::MessageQueue queue{"TestQueue"};
      REQUIRE(queue.ThreadId() != std::thread::id{});
   });
}

TEST_CASE("Dispatch after Stop throws QueueStopped", "[Pool]") {
   RunWithTimeout(2s, [&] {
      promise::MessageQueue queue{"StoppedQueue"};
      queue.Stop();
      auto promise = queue.Dispatch([] {});
      REQUIRE(promise.Rejected());
      RequireException<QueueStopped>(promise.Exception());
   });
}

TEST_CASE("Direct construction and throw of QueueStopped", "[Pool]") {
   RunWithTimeout(2s, [&] {
      REQUIRE_THROWS_AS(([] { throw QueueStopped{"DirectTest"}; }()), QueueStopped);
   });
}

TEST_CASE("Explicit Pool::GetName instantiations", "[Pool]") {
   RunWithTimeout(2s, [&] {
      auto check_pool_name = []<bool ASYNC, std::size_t SIZE>(char const* name) {
         Pool<ASYNC, SIZE> pool{name};
         REQUIRE(std::string{pool.GetName()} == name);
         pool.Stop();
      };

      check_pool_name.template operator()<false, 63>("Pool63");
      check_pool_name.template operator()<true, 64>("Pool64");
   });
}

TEST_CASE("Pool no-arg Dispatch overloads and stop paths", "[Pool]") {
   RunWithTimeout(2s, [&] {
      promise::Pool<2> pool{"dispatch-overloads"};

      auto immediate     = pool.Dispatch();
      auto delayed       = pool.Dispatch(1ms);
      auto delayed_until = pool.Dispatch(std::chrono::steady_clock::now() + 1ms);

      auto waiter = WPromise{[&]() -> Promise<void> {
         co_await immediate;
         co_await delayed;
         co_await delayed_until;
         co_return;
      }};

      waiter.WaitDone();
      REQUIRE(waiter.Resolved());
      REQUIRE(immediate.Resolved());
      REQUIRE(delayed.Resolved());
      REQUIRE(delayed_until.Resolved());

      pool.Stop();

      auto stopped_no_arg = pool.Dispatch();
      REQUIRE(stopped_no_arg.Rejected());
      RequireException<QueueStopped>(stopped_no_arg.Exception());

      std::function<int()> value_fun     = [] { return 7; };
      auto                 stopped_value = pool.Dispatch(std::move(value_fun));
      REQUIRE(stopped_value.Rejected());
      RequireException<QueueStopped>(stopped_value.Exception());
   });
}
