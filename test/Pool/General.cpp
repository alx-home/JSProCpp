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

TEST_CASE("Pool callback Dispatch overload and functor dispatch", "[Pool]") {
   RunWithTimeout(2s, [&] {
      using ReturnCallback = std::function<void(Resolve<int> const&, Reject const&)>;

      promise::Pool<8> pool{"callback-overload"};

      auto success = pool.Dispatch<int>(
        ReturnCallback{[](Resolve<int> const& resolve, Reject const&) { REQUIRE(resolve(33)); }}
      );
      success.WaitDone();
      REQUIRE(success.Resolved());
      REQUIRE(success.Value() == 33);

      auto throwing = pool.Dispatch<int>(ReturnCallback{[](Resolve<int> const&, Reject const&) {
         throw TestError{"callback throw"};
      }});
      throwing.WaitDone();
      REQUIRE(throwing.Rejected());
      RequireException<TestError>(throwing.Exception());

      auto from_functor = pool.Dispatch([] { return 21; });
      from_functor.WaitDone();
      REQUIRE(from_functor.Resolved());
      REQUIRE(from_functor.Value() == 21);

      pool.Stop();

      auto stopped = pool.Dispatch<int>(
        ReturnCallback{[](Resolve<int> const& resolve, Reject const&) { (void)resolve; }}
      );
      stopped.WaitDone();
      REQUIRE(stopped.Rejected());
      RequireException<QueueStopped>(stopped.Exception());
   });
}

TEST_CASE("Pool delay overload wrappers keep behavior", "[Pool]") {
   RunWithTimeout(2s, [&] {
      promise::Pool<2> pool{"delay-overload-wrappers"};

      std::function<void()> void_fun     = [] {};
      auto                  delayed_void = pool.Dispatch(std::move(void_fun), 1ms);
      delayed_void.WaitDone();
      REQUIRE(delayed_void.Resolved());

      std::function<int()> value_fun     = [] { return 42; };
      auto                 delayed_value = pool.Dispatch(std::move(value_fun), 1ms);
      delayed_value.WaitDone();
      REQUIRE(delayed_value.Resolved());
      REQUIRE(delayed_value.Value() == 42);

      auto delayed_lambda = pool.Dispatch([] { return 11; }, 1ms);
      delayed_lambda.WaitDone();
      REQUIRE(delayed_lambda.Resolved());
      REQUIRE(delayed_lambda.Value() == 11);

      pool.Stop();

      std::function<int()> stopped_fun   = [] { return 1; };
      auto                 stopped_value = pool.Dispatch(std::move(stopped_fun), 1ms);
      stopped_value.WaitDone();
      REQUIRE(stopped_value.Rejected());
      RequireException<QueueStopped>(stopped_value.Exception());
   });
}
