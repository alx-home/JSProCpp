#include "../TestCommon.h"

TEST_CASE("CVPromise notifies, rejects, and destructor behavior", "[CVPromise]") {
   RunWithTimeout(2s, [&] {
      CVPromise cv;

      auto initial = cv.Wait();
      REQUIRE_FALSE(initial.Done());

      cv.Notify();
      REQUIRE(initial.Done());
      REQUIRE(initial.Resolved());

      bool     caught = false;
      WPromise waiter{[&]() -> Promise<void> {
         try {
            co_await cv.Wait();
         } catch (const TestError&) {
            caught = true;
         }
         co_return;
      }};
      cv.Reject<TestError>("boom");
      REQUIRE(waiter.Done());
      REQUIRE(caught);

      bool     end_caught = false;
      auto     temp_cv    = std::make_unique<CVPromise>();
      WPromise waiter2{[&]() -> Promise<void> {
         try {
            co_await temp_cv->Wait();
         } catch (const CVPromise::End&) {
            end_caught = true;
         }
         co_return;
      }};
      REQUIRE(!waiter2.Done());
      REQUIRE(!end_caught);

      temp_cv = nullptr;

      REQUIRE(waiter2.Done());
      REQUIRE(end_caught);
   });
}

TEST_CASE("CVPromise conversion operators expose updated state", "[CVPromise]") {
   RunWithTimeout(2s, [&] {
      CVPromise cv;

      REQUIRE_FALSE(cv.Resolved());
      REQUIRE_FALSE(cv.Rejected());

      auto wait_via_deref = *cv;
      cv.Notify();

      REQUIRE(wait_via_deref.Done());
      REQUIRE(wait_via_deref.Resolved());

      auto wait_via_conversion = static_cast<WPromise<void>>(cv);
      REQUIRE_FALSE(wait_via_conversion.Done());

      cv.Reject<TestError>("cv rejected");
      REQUIRE(wait_via_conversion.Rejected());
      RequireException<TestError>(wait_via_conversion.Exception());
   });
}

TEST_CASE("CVPromise default construction and destruction stay balanced", "[CVPromise]") {
   RunWithTimeout(2s, [&] {
      {
         CVPromise cv;
         REQUIRE_FALSE(cv.Resolved());
         REQUIRE_FALSE(cv.Rejected());
      }

      {
         auto cv = std::make_unique<CVPromise>();
         REQUIRE_FALSE(cv->Resolved());
         REQUIRE_FALSE(cv->Rejected());
      }
   });
}
