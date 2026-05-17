#include "../TestCommon.h"

TEST_CASE("Promise resolves with value", "[Promise][Core]") {
   auto p = Promise<int>::Resolve(42);

   REQUIRE(p.Done());
   REQUIRE(p.Resolved());
   REQUIRE_FALSE(p.Rejected());
   REQUIRE(p.Value() == 42);
}

TEST_CASE("Promise resolves with void", "[Promise][Core]") {
   auto p = Promise<void>::Resolve();

   REQUIRE(p.Done());
   REQUIRE(p.Resolved());
   REQUIRE_FALSE(p.Rejected());
}

TEST_CASE("Promise rejects with exception", "[Promise][Core]") {
   auto p = Promise<int>::Reject<TestError>("boom");

   REQUIRE(p.Done());
   REQUIRE_FALSE(p.Resolved());
   REQUIRE(p.Rejected());
   RequireException<TestError>(p.Exception());
}

TEST_CASE("Then transforms a resolved value", "[Promise][Core]") {
   auto p = Promise<int>::Resolve(21).Then([](int value) { return value * 2; });

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 42);
}

TEST_CASE("Catch recovers from rejection", "[Promise][Core]") {
   auto p =
     Promise<int>::Reject<TestError>("boom").Catch([](std::exception_ptr const&) { return 42; });

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 42);
}

TEST_CASE("Catch preserves exception when rethrowing", "[Promise][Core]") {
   auto p = Promise<int>::Reject<TestError>("boom").Catch([](std::exception_ptr exception) -> int {
      std::rethrow_exception(exception);
   });

   REQUIRE(p.Rejected());
   RequireException<TestError>(p.Exception());
}
