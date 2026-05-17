#include "../TestCommon.h"

TEST_CASE("Type-erased base destructors are exercised", "[Promise][Coverage]") {
   struct DerivedFunction final : promise::Function {};

   promise::Function* function_base = new DerivedFunction{};
   REQUIRE_NOTHROW(delete function_base);

   struct LocalAwaitable final : promise::VPromise::Awaitable {
      bool await_ready() const override { return true; }
      void await_resume() const override {}
      bool await_suspend(std::coroutine_handle<>) const override { return false; }
   };

   promise::VPromise::Awaitable* awaitable_base = new LocalAwaitable{};
   REQUIRE(awaitable_base->await_ready());
   REQUIRE_FALSE(awaitable_base->await_suspend(std::noop_coroutine()));
   REQUIRE_NOTHROW(awaitable_base->await_resume());
   REQUIRE_NOTHROW(delete awaitable_base);

   auto erased = WPromise<int>::Resolve(9).ToPointer<promise::VPromise>();
   REQUIRE(erased != nullptr);
   erased.reset();
}

#ifdef PROMISE_MEMCHECK
TEST_CASE("Memcheck no-leak path is executable", "[Promise][Coverage]") {
   REQUIRE_NOTHROW([&] {
      auto guard = promise::Memcheck();

      auto done = Promise<void>::Resolve();
      done.WaitDone();

      auto value = Promise<int>::Resolve(7);
      value.WaitDone();
      REQUIRE(value.Value() == 7);

      (void)guard;
   }());
}
#endif
