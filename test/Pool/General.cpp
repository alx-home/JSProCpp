#include "../TestCommon.h"
#include <utils/Pool.h>

TEST_CASE("MessageQueue exposes a running worker thread", "[Pool]") {
   promise::MessageQueue queue{"TestQueue"};
   REQUIRE(queue.ThreadId() != std::thread::id{});
}

TEST_CASE("Dispatch after Stop throws QueueStopped", "[Pool]") {
   promise::MessageQueue queue{"StoppedQueue"};
   queue.Stop();
   auto promise = queue.Dispatch([] {});
   REQUIRE(promise.Rejected());
   RequireException<QueueStopped>(promise.Exception());
}

TEST_CASE("Direct construction and throw of QueueStopped", "[Pool]") {
   REQUIRE_THROWS_AS(([] { throw QueueStopped{"DirectTest"}; }()), QueueStopped);
}

TEST_CASE("Explicit Pool::GetName instantiations", "[Pool]") {
   auto check_pool_name = []<bool ASYNC, std::size_t SIZE>(char const* name) {
      Pool<ASYNC, SIZE> pool{name};
      REQUIRE(std::string{pool.GetName()} == name);
      pool.Stop();
   };

   check_pool_name.template operator()<false, 63>("Pool63");
   check_pool_name.template operator()<true, 64>("Pool64");
}
