#include "../TestCommon.h"

#include <atomic>
#include <vector>

TEST_CASE("Mutex lock acquisition is serialized", "[Promise][Mutex]") {
   RunWithTimeout(2s, [] {
      promise::Mutex     mutex;
      promise::LockGuard first{mutex};
      promise::LockGuard second{mutex};

      auto first_lock = first.Lock();
      REQUIRE(first_lock.Done());
      REQUIRE(first.OwnLock());
      REQUIRE(mutex.OwnLock());

      auto second_lock = second.Lock();
      REQUIRE_FALSE(second_lock.Done());
      REQUIRE(mutex.OwnLock());

      first.Unlock();

      second_lock.WaitDone();
      REQUIRE(second_lock.Done());
      REQUIRE(second.OwnLock());
      REQUIRE(mutex.OwnLock());

      second.Unlock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE("Mutex operator* acquires and queues waiters", "[Promise][Mutex]") {
   RunWithTimeout(2s, [] {
      promise::Mutex mutex;

      auto first = *mutex;
      REQUIRE(first.Done());
      REQUIRE(mutex.OwnLock());

      auto second = *mutex;
      REQUIRE_FALSE(second.Done());
      REQUIRE(mutex.OwnLock());

      mutex.UnLock();

      second.WaitDone();
      REQUIRE(second.Done());
      REQUIRE(mutex.OwnLock());

      mutex.UnLock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE("LockGuard destructor unlocks and wakes queued waiter", "[Promise][Mutex]") {
   RunWithTimeout(2s, [] {
      promise::Mutex     mutex;
      promise::LockGuard waiter{mutex};
      WPromise<void>     waiter_lock = WPromise<void>::Resolve();

      {
         promise::LockGuard owner{mutex};
         auto               owner_lock = owner.Lock();
         REQUIRE(owner_lock.Done());
         REQUIRE(owner.OwnLock());
         REQUIRE(mutex.OwnLock());

         waiter_lock = waiter.Lock();
         REQUIRE_FALSE(waiter_lock.Done());
      }

      waiter_lock.WaitDone();
      REQUIRE(waiter_lock.Done());
      REQUIRE(waiter.OwnLock());
      REQUIRE(mutex.OwnLock());

      waiter.Unlock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE(
  "LockGuard move constructor transfers ownership and moved-from is inert",
  "[Promise][Mutex]"
) {
   RunWithTimeout(2s, [] {
      promise::Mutex     mutex;
      promise::LockGuard waiter{mutex};
      WPromise<void>     waiter_lock = WPromise<void>::Resolve();

      {
         promise::LockGuard owner{mutex};
         auto               owner_lock = owner.Lock();
         REQUIRE(owner_lock.Done());
         REQUIRE(owner.OwnLock());

         waiter_lock = waiter.Lock();
         REQUIRE_FALSE(waiter_lock.Done());

         promise::LockGuard moved{std::move(owner)};
         REQUIRE(moved.OwnLock());
         REQUIRE_FALSE(owner.OwnLock());
      }

      waiter_lock.WaitDone();
      REQUIRE(waiter_lock.Done());
      REQUIRE(waiter.OwnLock());
      REQUIRE(mutex.OwnLock());

      waiter.Unlock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE(
  "LockGuard move assignment unlocks current ownership before transfer",
  "[Promise][Mutex]"
) {
   RunWithTimeout(2s, [] {
      promise::Mutex     mutex;
      promise::LockGuard owner{mutex};
      promise::LockGuard waiter{mutex};
      promise::LockGuard replacement{mutex};

      auto owner_lock = owner.Lock();
      REQUIRE(owner_lock.Done());
      REQUIRE(owner.OwnLock());

      auto waiter_lock = waiter.Lock();
      REQUIRE_FALSE(waiter_lock.Done());

      owner = std::move(replacement);
      REQUIRE_FALSE(owner.OwnLock());
      REQUIRE_FALSE(replacement.OwnLock());

      waiter_lock.WaitDone();
      REQUIRE(waiter_lock.Done());
      REQUIRE(waiter.OwnLock());
      REQUIRE(mutex.OwnLock());

      waiter.Unlock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE(
  "LockGuard move assignment handles self move without releasing lock",
  "[Promise][Mutex]"
) {
   RunWithTimeout(2s, [] {
      promise::Mutex     mutex;
      promise::LockGuard owner{mutex};
      promise::LockGuard waiter{mutex};

      auto owner_lock = owner.Lock();
      REQUIRE(owner_lock.Done());
      REQUIRE(owner.OwnLock());

      auto waiter_lock = waiter.Lock();
      REQUIRE_FALSE(waiter_lock.Done());

#ifdef __GNUC__
// Suppress self-move warning for testing purposes
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wself-move"
#elif __clang__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wself-move"
#endif
      owner = std::move(owner);
#ifdef __GNUC__
#   pragma GCC diagnostic pop
#elif __clang__
#   pragma clang diagnostic pop
#endif
      REQUIRE(owner.OwnLock());
      REQUIRE_FALSE(waiter_lock.Done());

      owner.Unlock();

      waiter_lock.WaitDone();
      REQUIRE(waiter_lock.Done());
      REQUIRE(waiter.OwnLock());

      waiter.Unlock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE("Mutex Lock handles immediate and queued branch progression", "[Promise][Mutex]") {
   RunWithTimeout(2s, [] {
      promise::Mutex mutex;

      auto first = mutex.Lock();
      REQUIRE(first.Done());
      REQUIRE(mutex.OwnLock());

      auto second = mutex.Lock();
      auto third  = mutex.Lock();

      REQUIRE_FALSE(second.Done());
      REQUIRE_FALSE(third.Done());

      mutex.UnLock();
      second.WaitDone();

      REQUIRE(second.Done());
      REQUIRE_FALSE(third.Done());
      REQUIRE(mutex.OwnLock());

      mutex.UnLock();
      third.WaitDone();

      REQUIRE(third.Done());
      REQUIRE(mutex.OwnLock());

      mutex.UnLock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE("CVPromise Wait(lock) reacquires when notified", "[Promise][Mutex][CVPromise]") {
   RunWithTimeout(2s, [] {
      promise::Mutex     mutex;
      promise::LockGuard waiter_lock{mutex};
      CVPromise          cv;

      REQUIRE_FALSE(waiter_lock.OwnLock());
      REQUIRE_FALSE(mutex.OwnLock());

      auto wait_with_lock = cv.Wait(waiter_lock);
      REQUIRE_FALSE(waiter_lock.OwnLock());
      REQUIRE_FALSE(mutex.OwnLock());
      REQUIRE_FALSE(wait_with_lock.Done());

      cv.Notify();

      wait_with_lock.WaitDone();
      REQUIRE(wait_with_lock.Done());
      REQUIRE(wait_with_lock.Resolved());
      REQUIRE(waiter_lock.OwnLock());
      REQUIRE(mutex.OwnLock());

      waiter_lock.Unlock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE(
  "CVPromise Wait(lock) blocks reacquire while another guard owns lock",
  "[Promise][Mutex][CVPromise]"
) {
   RunWithTimeout(2s, [] {
      promise::Mutex     mutex;
      promise::LockGuard waiter_lock{mutex};
      promise::LockGuard blocker_lock{mutex};
      CVPromise          cv;

      auto waiter_first_lock = waiter_lock.Lock();
      REQUIRE(waiter_first_lock.Done());
      REQUIRE(waiter_lock.OwnLock());

      auto wait_with_lock = cv.Wait(waiter_lock);

      bool resumed = false;
      auto waiter  = WPromise{[&]() -> Promise<void> {
         co_await wait_with_lock;
         resumed = true;
      }};
      REQUIRE_FALSE(waiter.Done());

      auto blocker_acquire = blocker_lock.Lock();
      REQUIRE(blocker_acquire.Done());

      cv.Notify();
      REQUIRE_FALSE(waiter.Done());

      blocker_lock.Unlock();

      waiter.WaitDone();
      REQUIRE(waiter.Done());
      REQUIRE(waiter.Resolved());
      REQUIRE(resumed);
      REQUIRE(waiter_lock.OwnLock());

      waiter_lock.Unlock();
   });
}

TEST_CASE(
  "CVPromise Wait(lock) does not reacquire lock on rejection",
  "[Promise][Mutex][CVPromise]"
) {
   RunWithTimeout(2s, [] {
      promise::Mutex     mutex;
      promise::LockGuard waiter_lock{mutex};
      CVPromise          cv;

      auto wait_with_lock = cv.Wait(waiter_lock);

      bool caught = false;
      auto waiter = WPromise{[&]() -> Promise<void> {
         try {
            co_await wait_with_lock;
         } catch (TestError const&) {
            caught = true;
         }
      }};
      REQUIRE_FALSE(waiter.Done());

      cv.Reject<TestError>("reject path");

      waiter.WaitDone();
      REQUIRE(waiter.Done());
      REQUIRE(waiter.Resolved());
      REQUIRE(caught);
      REQUIRE_FALSE(waiter_lock.OwnLock());
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE("CVPromise Wait(lock) direct WaitDone path", "[Promise][Mutex][CVPromise]") {
   RunWithTimeout(2s, [] {
      promise::Mutex     mutex;
      promise::LockGuard waiter_lock{mutex};
      CVPromise          cv;

      auto wait_with_lock = cv.Wait(waiter_lock);
      cv.Notify();

      wait_with_lock.WaitDone();
      REQUIRE(wait_with_lock.Done());
      REQUIRE(wait_with_lock.Resolved());
      REQUIRE(waiter_lock.OwnLock());

      waiter_lock.Unlock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE("Mutex serializes critical section across many threads", "[Promise][Mutex][Concurrent]") {
   RunWithTimeout(4s, [] {
      promise::Mutex           mutex;
      std::atomic<int>         in_critical{0};
      std::atomic<int>         max_in_critical{0};
      std::atomic<int>         completed{0};
      constexpr int            kThreads    = 8;
      constexpr int            kIterations = 30;
      std::vector<std::thread> workers;
      workers.reserve(kThreads);

      auto update_max = [&](int value) {
         int observed = max_in_critical.load();
         while (observed < value && !max_in_critical.compare_exchange_weak(observed, value)) {
         }
      };

      for (int t = 0; t < kThreads; ++t) {
         workers.emplace_back([&] {
            for (int i = 0; i < kIterations; ++i) {
               promise::LockGuard lock_guard{mutex};
               auto               lock = lock_guard.Lock();
               lock.WaitDone();

               int now = in_critical.fetch_add(1) + 1;
               update_max(now);
               std::this_thread::yield();
               in_critical.fetch_sub(1);

               lock_guard.Unlock();
               completed.fetch_add(1);
            }
         });
      }

      for (auto& worker : workers) {
         worker.join();
      }

      REQUIRE(completed.load() == kThreads * kIterations);
      REQUIRE(in_critical.load() == 0);
      REQUIRE(max_in_critical.load() == 1);
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE(
  "CVPromise Wait(lock) resumes when notified from another thread",
  "[Promise][Mutex][Concurrent][CVPromise]"
) {
   RunWithTimeout(4s, [] {
      promise::Mutex     mutex;
      promise::LockGuard waiter_lock{mutex};
      CVPromise          cv;

      auto wait_with_lock = cv.Wait(waiter_lock);
      REQUIRE_FALSE(wait_with_lock.Done());

      std::thread notifier([&] {
         std::this_thread::sleep_for(2ms);
         cv.Notify();
      });

      wait_with_lock.WaitDone();
      notifier.join();

      REQUIRE(wait_with_lock.Done());
      REQUIRE(wait_with_lock.Resolved());
      REQUIRE(waiter_lock.OwnLock());
      REQUIRE(mutex.OwnLock());

      waiter_lock.Unlock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE(
  "LockGuard co_await acquires and serializes with existing owner",
  "[Promise][Mutex][co_await]"
) {
   RunWithTimeout(4s, [] {
      promise::Mutex     mutex;
      promise::LockGuard first{mutex};
      promise::LockGuard second{mutex};

      auto first_lock = first.Lock();
      REQUIRE(first_lock.Done());
      REQUIRE(first.OwnLock());

      bool second_resumed = false;
      auto waiter         = WPromise{[&]() mutable -> Promise<void> {
         co_await second.Lock();
         second_resumed = true;
      }};
      REQUIRE_FALSE(waiter.Done());
      REQUIRE_FALSE(second_resumed);

      first.Unlock();

      waiter.WaitDone();
      REQUIRE(waiter.Done());
      REQUIRE(waiter.Resolved());
      REQUIRE(second_resumed);
      REQUIRE(second.OwnLock());
      REQUIRE(mutex.OwnLock());

      second.Unlock();
      REQUIRE_FALSE(mutex.OwnLock());
   });
}

TEST_CASE(
  "CVPromise co_await via dereference resumes when notified from another thread",
  "[Promise][Mutex][Concurrent][CVPromise][co_await]"
) {
   RunWithTimeout(4s, [] {
      CVPromise         cv;
      std::atomic<bool> resumed{false};

      auto waiter = WPromise{[&]() -> Promise<void> {
         co_await *cv;
         resumed.store(true);
      }};
      REQUIRE_FALSE(waiter.Done());

      std::thread notifier([&] {
         std::this_thread::sleep_for(2ms);
         cv.Notify();
      });

      waiter.WaitDone();
      notifier.join();

      REQUIRE(waiter.Done());
      REQUIRE(waiter.Resolved());
      REQUIRE(resumed.load());
   });
}
