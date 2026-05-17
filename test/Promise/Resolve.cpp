#include "../TestCommon.h"

TEST_CASE("Resolve<void> bool reflects resolver completion", "[Promise][Resolve]") {
   auto [promise, resolve, reject] = promise::Create<void>();

   REQUIRE_FALSE(static_cast<bool>(*resolve));
   REQUIRE((*resolve)());
   REQUIRE(static_cast<bool>(*resolve));
   REQUIRE_FALSE((*resolve)());
   REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

   REQUIRE(promise.Resolved());
}
