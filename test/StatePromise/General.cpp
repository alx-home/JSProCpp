#include "../TestCommon.h"

TEST_CASE("StatePromise tracks ready, done, and reject transitions", "[StatePromise]") {
   RunWithTimeout(2s, [&] {
      StatePromise state;

      auto wait_any   = state.Wait();
      auto wait_ready = state.WaitReady();

      state.Ready();
      REQUIRE(wait_any.Done());
      REQUIRE(wait_ready.Done());

      REQUIRE(wait_any.Resolved());
      REQUIRE(wait_ready.Resolved());
      REQUIRE_FALSE(state.IsDone());

      auto wait_done = state.WaitDone();
      state.Done();
      REQUIRE(wait_done.Done());

      REQUIRE(wait_done.Resolved());
      REQUIRE(state.IsDone());

      StatePromise state2;
      auto         wait_done_reject = state2.WaitDoneWithReject();
      state2.Done();
      REQUIRE(wait_done_reject.Done());
      REQUIRE(wait_done_reject.Rejected());
      RequireException<StatePromise::End>(wait_done_reject.Exception());

      StatePromise state3;
      auto         wait_with_reject = state3.WaitWithReject();
      state3.Ready();
      REQUIRE(wait_with_reject.Done());
      REQUIRE(wait_with_reject.Resolved());

      StatePromise state4;
      auto         wait_with_reject2 = state4.WaitWithReject();
      state4.Done();
      REQUIRE(wait_with_reject2.Done());
      REQUIRE(wait_with_reject2.Rejected());
      RequireException<StatePromise::End>(wait_with_reject2.Exception());

      bool     end_caught_state = false;
      auto     temp_state       = std::make_unique<StatePromise>();
      WPromise waiter_state{[&]() -> Promise<void> {
         try {
            co_await temp_state->WaitDoneWithReject();
         } catch (const StatePromise::End&) {
            end_caught_state = true;
         }
         co_return;
      }};
      REQUIRE(!waiter_state.Done());
      REQUIRE(!end_caught_state);

      temp_state = nullptr;

      REQUIRE(waiter_state.Done());
      REQUIRE(end_caught_state);
   });
}

TEST_CASE("StatePromise reset restores pending transitions", "[StatePromise]") {
   RunWithTimeout(2s, [&] {
      WPromise flow{[]() -> Promise<void> {
         StatePromise state;

         auto wait_ready = state.WaitReady();
         auto wait_done  = state.WaitDone();

         state.Ready();
         co_await wait_ready;
         REQUIRE(wait_ready.Resolved());
         REQUIRE_FALSE(wait_done.Done());

         state.Reset();
         auto wait_ready_after_reset = state.WaitReady();
         auto wait_done_after_reset  = state.WaitDone();

         REQUIRE_FALSE(wait_ready_after_reset.Done());
         REQUIRE_FALSE(wait_done_after_reset.Done());

         state.Done();
         co_await wait_done;
         co_await wait_done_after_reset;
         co_await wait_ready_after_reset;
         REQUIRE(state.IsDone());

         StatePromise state_ready_only;
         auto         ready_only_wait_ready = state_ready_only.WaitReady();
         auto         ready_only_wait_done  = state_ready_only.WaitDone();

         state_ready_only.Ready();
         co_await ready_only_wait_ready;
         REQUIRE_FALSE(ready_only_wait_done.Done());

         state_ready_only.Reset();
         auto ready_only_wait_ready_after_reset = state_ready_only.WaitReady();
         auto ready_only_wait_done_after_reset  = state_ready_only.WaitDone();

         REQUIRE_FALSE(ready_only_wait_ready_after_reset.Done());
         REQUIRE_FALSE(ready_only_wait_done_after_reset.Done());

         state_ready_only.Done();
         co_await ready_only_wait_done;
         co_await ready_only_wait_done_after_reset;
         co_await ready_only_wait_ready_after_reset;

         co_return;
      }};

      REQUIRE(flow.Resolved());
   });
}

TEST_CASE("StatePromise reset after Done reinitializes both promises", "[StatePromise]") {
   RunWithTimeout(2s, [&] {
      StatePromise state;

      auto wait_ready_before = state.WaitReady();
      auto wait_done_before  = state.WaitDone();

      state.Done();
      REQUIRE(wait_ready_before.Done());
      REQUIRE(wait_done_before.Done());
      REQUIRE(state.IsDone());

      state.Reset();

      auto wait_ready_after = state.WaitReady();
      auto wait_done_after  = state.WaitDone();
      REQUIRE_FALSE(wait_ready_after.Done());
      REQUIRE_FALSE(wait_done_after.Done());
      REQUIRE_FALSE(state.IsDone());

      state.Ready();
      REQUIRE(wait_ready_after.Done());
      REQUIRE_FALSE(wait_done_after.Done());

      state.Done();
      REQUIRE(wait_done_after.Done());
      REQUIRE(state.IsDone());
   });
}

TEST_CASE("StatePromise default construction creates pending waiters", "[StatePromise]") {
   RunWithTimeout(2s, [&] {
      StatePromise state;

      auto wait_ready = state.WaitReady();
      auto wait_done  = state.WaitDone();

      REQUIRE_FALSE(wait_ready.Done());
      REQUIRE_FALSE(wait_done.Done());
      REQUIRE_FALSE(state.IsDone());

      state.Done();
      REQUIRE(wait_ready.Done());
      REQUIRE(wait_done.Done());
      REQUIRE(state.IsDone());
   });
}
