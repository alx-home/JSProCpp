#include "../TestCommon.h"

TEST_CASE("Then can chain coroutine promises", "[Promise][Chains]") {
   auto p = Promise<int>::Resolve(21).Then([](int value) -> Promise<int> { co_return value * 2; });

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 42);
}

TEST_CASE("Then Catch Then passes through when no exception is raised", "[Promise][Chains]") {
   auto p = Promise<int>::Resolve(4).Then(
                                      [](int value) { return value + 1; }
   ).Catch([](std::exception_ptr const&) {
       return 0;
    }).Then([](int value) { return value * 2; });

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 10);
}

TEST_CASE("Then Catch Then recovers when the first Then throws", "[Promise][Chains]") {
   auto p = Promise<int>::Resolve(4)
              .Then([](int) -> int { throw TestError("then failed"); })
              .Catch([](TestError const& exception) {
                 REQUIRE(std::string{exception.what()} == "then failed");
                 return 7;
              })
              .Then([](int value) { return value * 3; });

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 21);
}

TEST_CASE("Then Catch Then stays rejected when Catch rethrows", "[Promise][Chains]") {
   auto p = Promise<int>::Resolve(4).Then(
                                      [](int) -> int { throw TestError("then failed"); }
   ).Catch([](std::exception_ptr exception) -> int {
       std::rethrow_exception(exception);
    }).Then([](int value) { return value * 2; });

   REQUIRE(p.Rejected());
   RequireException<TestError>(p.Exception());
}

TEST_CASE("Then Catch Then stays rejected when Catch throws a new exception", "[Promise][Chains]") {
   auto p = Promise<int>::Reject<TestError>("initial failure")
              .Then([](int value) { return value + 1; })
              .Catch([](TestError const& exception) -> int {
                 REQUIRE(std::string{exception.what()} == "initial failure");
                 throw CatchError("catch failed");
              })
              .Then([](int value) { return value * 2; });

   REQUIRE(p.Rejected());
   RequireException<CatchError>(p.Exception());
}

TEST_CASE("Rvalue chaining exercises moved Then Catch Finally overloads", "[Promise][Chains]") {
   RunWithTimeout(2s, [&] {
      auto source = WPromise{[]() -> Promise<int> { co_return 6; }};

      auto moved_chain = std::move(source).Then(
                                            [](int value) { return value + 1; }
      ).Catch([](std::exception_ptr) {
          return 0;
       }).Finally([] {});

      REQUIRE(moved_chain.Resolved());
      REQUIRE(moved_chain.Value() == 7);

      auto source_reject = WPromise{[]() -> Promise<int> {
         throw TestError{"moved reject"};
         co_return 0;
      }};

      auto moved_recover = std::move(source_reject)
                             .Then([](int value) { return value + 10; })
                             .Catch([](TestError const&) { return 77; })
                             .Finally([] {});

      REQUIRE(moved_recover.Resolved());
      REQUIRE(moved_recover.Value() == 77);
   });
}
