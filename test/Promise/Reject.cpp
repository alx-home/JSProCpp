#include "../TestCommon.h"

TEST_CASE("Reject callback ignores duplicate invocation", "[Promise][Reject]") {
   auto [promise, resolve, reject] = promise::Create<int>();

   REQUIRE((*reject)(std::make_exception_ptr(TestError{"first reject"})));
   REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"second reject"})));
   REQUIRE_FALSE((*resolve)(5));

   REQUIRE(promise.Rejected());
   RequireException<TestError>(promise.Exception());
}
