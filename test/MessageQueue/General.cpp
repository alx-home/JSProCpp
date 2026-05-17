#include "../TestCommon.h"

#include <promise/core/MessageQueueProxy.inl>

namespace {

struct MessageQueue {
   explicit MessageQueue(bool running = true)
      : running_(running) {}

   template <class F>
   bool Ensure(F&& callback) {
      if (!running_) {
         return false;
      }

      std::forward<F>(callback)();
      return true;
   }

   bool running_{true};
};

struct ObjectOk : MessageQueue {
   ObjectOk()
      : MessageQueue(true) {}

   int value_{0};
};

struct ObjectStopped : MessageQueue {
   ObjectStopped()
      : MessageQueue(false) {}

   int value_{0};
};

}  // namespace

TEST_CASE("MessageQueue dispatch runs work and resolves on queue thread", "[MessageQueue]") {
   promise::MessageQueue queue{"promise-test"};
   std::atomic<bool>     ran_on_queue_thread{false};

   auto flow = WPromise{[&]() -> Promise<void> {
      auto dispatched = queue.Dispatch([&queue, &ran_on_queue_thread]() {
         ran_on_queue_thread.store(std::this_thread::get_id() == queue.ThreadId());
         return 42;
      });

      auto value = co_await dispatched;

      REQUIRE(dispatched.Done());
      REQUIRE(dispatched.Resolved());
      REQUIRE(value == 42);
      REQUIRE(dispatched.Value() == 42);
      REQUIRE(ran_on_queue_thread.load());

      co_return;
   }};

   flow.WaitDone();
   REQUIRE(flow.Resolved());
   queue.Stop();
}

TEST_CASE("Ensure co_await verifies thread ID consistency in MessageQueue", "[MessageQueue]") {
   promise::MessageQueue queue{"promise-queue-ensure"};

   WPromise promise{[&]() -> Promise<void> {
      co_await queue.Ensure();
      REQUIRE(std::this_thread::get_id() == queue.ThreadId());
   }};

   promise.WaitDone();
   REQUIRE(promise.Resolved());
}

TEST_CASE("MessageQueue Ensure forwards callables and rejects after stop", "[MessageQueue]") {
   promise::MessageQueue queue{"promise-queue-forward"};

   auto ensure_call =
     queue.Ensure([&queue]() { REQUIRE(std::this_thread::get_id() == queue.ThreadId()); });

   ensure_call.WaitDone();
   REQUIRE(ensure_call.Resolved());

   queue.Stop();
   auto ensure_after_stop = queue.Ensure([] {});
   REQUIRE(ensure_after_stop.Rejected());
}

TEST_CASE(
  "MessageQueueProxy non-void operator resolves and rejects as expected",
  "[MessageQueue]"
) {
   utils::queue::Proxy<ObjectOk> proxy_ok;

   auto resolved = proxy_ok(std::function<int(ObjectOk&)>{[](ObjectOk& object) {
      object.value_ = 21;
      return object.value_ * 2;
   }});

   REQUIRE(resolved.Resolved());
   REQUIRE(resolved.Value() == 42);

   auto rejected_from_throw = proxy_ok(std::function<int(ObjectOk&)>{[](ObjectOk&) -> int {
      throw TestError("proxy callback failure");
   }});

   REQUIRE(rejected_from_throw.Rejected());
   RequireException<TestError>(rejected_from_throw.Exception());

   utils::queue::Proxy<ObjectStopped> proxy_stopped;
   auto                               rejected_from_dispatch =
     proxy_stopped(std::function<int(ObjectStopped&)>{[](ObjectStopped&) { return 1; }});

   REQUIRE(rejected_from_dispatch.Rejected());
}
