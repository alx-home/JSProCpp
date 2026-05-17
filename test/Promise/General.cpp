#include "../TestCommon.h"

TEST_CASE("Then Catch Then stays rejected when the final Then throws", "[Promise]") {
   auto p = Promise<int>::Reject<TestError>("initial failure")
              .Then([](int value) { return value + 1; })
              .Catch([](TestError const& exception) {
                 REQUIRE(std::string{exception.what()} == "initial failure");
                 return 5;
              })
              .Then([](int) -> int { throw FinalThenError("final then failed"); });

   REQUIRE(p.Rejected());
   RequireException<FinalThenError>(p.Exception());
}

TEST_CASE("Async catch-through chain preserves old promise_test behavior", "[Promise]") {
   auto p = WPromise{[]() -> Promise<int> { co_return 0; }}
              .Then([](int value) -> Promise<int> { co_return value + 3; })
              .Catch([](std::exception_ptr) -> Promise<void> { co_return; })
              .Then([](std::optional<int> const&) -> Promise<int> { co_return 0; })
              .Catch([](std::exception_ptr) -> Promise<int> { co_return 0; })
              .Then([](int) -> Promise<void> { co_return; })
              .Catch([](std::exception_ptr) -> Promise<int> { co_return 0; })
              .Then([](std::optional<int> const&) -> Promise<void> { co_return; })
              .Catch([](std::exception_ptr) -> Promise<void> { co_return; })
              .Then([]() -> Promise<void> { co_return; })
              .Then([]() -> Promise<int> { co_return 800; });

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 800);
}

TEST_CASE("Sync catch-through chain preserves old promise_test behavior", "[Promise]") {
   auto p = WPromise{[]() -> Promise<int> { co_return 0; }}
              .Then([](int value) { return value + 3; })
              .Catch([](std::exception_ptr) {})
              .Then([](std::optional<int> const&) { return 0; })
              .Catch([](std::exception_ptr) { return 0; })
              .Then([](int) {})
              .Catch([](std::exception_ptr) { return 0; })
              .Then([](std::optional<int> const&) {})
              .Catch([](std::exception_ptr) {})
              .Then([]() {})
              .Then([]() { return 800; });

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 800);
}

TEST_CASE("Complex Then Catch Finally chain matches old promise_test flow", "[Promise]") {
   auto p = Promise<int>::Resolve(6)
              .Then([](int value) -> Promise<double> {
                 throw TestError("test");
                 co_return value + 3;
              })
              .Then([](double value) -> Promise<double> { co_return value; })
              .Catch([](std::exception_ptr) -> Promise<int> { co_return 300; })
              .Finally([]() {})
              .Then([](std::variant<int, double> const&) -> Promise<double> {
                 throw CatchError("test3");
                 co_return 0.0;
              })
              .Finally([]() {})
              .Catch([](std::exception_ptr) -> Promise<double> { co_return 300; })
              .Finally([]() {})
              .Finally([]() { throw FinallyError("test55"); })
              .Catch([](FinallyError const&) -> Promise<double> { co_return 300; })
              .Then([](Resolve<int> const& resolve, double value) -> Promise<int, true> {
                 REQUIRE(resolve(static_cast<int>(value) + 3));
                 co_return;
              });

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 303);
}

TEST_CASE("Finally runs on resolved promise", "[Promise]") {
   bool cleanup_ran = false;

   auto p = Promise<int>::Resolve(7).Finally([&cleanup_ran]() { cleanup_ran = true; });

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 7);
   REQUIRE(cleanup_ran);
}

TEST_CASE("Finally runs on rejected promise", "[Promise]") {
   bool cleanup_ran = false;

   auto p =
     Promise<int>::Reject<TestError>("boom").Finally([&cleanup_ran]() { cleanup_ran = true; });

   REQUIRE(p.Rejected());
   REQUIRE(cleanup_ran);
   RequireException<TestError>(p.Exception());
}

TEST_CASE("promise::Create resolves externally", "[Promise]") {
   auto [p, resolve, reject] = promise::Create<int>();

   REQUIRE_FALSE(p.Done());
   REQUIRE_FALSE(*reject);

   REQUIRE((*resolve)(99));
   REQUIRE_FALSE((*resolve)(100));
   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 99);
}

TEST_CASE("promise::Create rejects externally", "[Promise]") {
   auto [p, resolve, reject] = promise::Create<int>();

   REQUIRE_FALSE(p.Done());
   REQUIRE((*reject)(std::make_exception_ptr(TestError{"boom"})));
   REQUIRE_FALSE((*resolve)(99));
   REQUIRE(p.Rejected());
   RequireException<TestError>(p.Exception());
}

TEST_CASE("MakePromise wraps synchronous and coroutine callables", "[Promise]") {
   auto sync      = MakePromise([] { return 12; });
   auto coroutine = MakePromise([]() -> Promise<int> { co_return 30; });

   REQUIRE(sync.Resolved());
   REQUIRE(sync.Value() == 12);
   REQUIRE(coroutine.Resolved());
   REQUIRE(coroutine.Value() == 30);
}

TEST_CASE("Direct coroutine promises resolve", "[Promise]") {
   auto p = MakePromise(CoroutineValue, 15);
   auto v = MakePromise(CoroutineVoid);

   REQUIRE(p.Resolved());
   REQUIRE(p.Value() == 15);
   REQUIRE(v.Resolved());
}

TEST_CASE("Race returns the first resolved alternative", "[Promise]") {
   auto [pending, resolve, reject] = promise::Create<std::string>();
   auto raced                      = promise::Race(Promise<int>::Resolve(7), pending);

   REQUIRE(raced.Resolved());
   REQUIRE(std::holds_alternative<int>(raced.Value()));
   REQUIRE(std::get<int>(raced.Value()) == 7);

   REQUIRE((*resolve)("late"));
   REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
}

TEST_CASE("Race rejection flows through Catch into optional variant", "[Promise]") {
   auto [delay, resolve, reject] = promise::Create<void>();

   auto raced = promise::Race(
                  WPromise{[]() -> Promise<double> {
                     throw TestError("race failure");
                     co_return 0.0;
                  }},
                  WPromise{[&delay]() -> Promise<double> {
                     co_await delay;
                     co_return 2.0;
                  }},
                  WPromise{[&delay]() -> Promise<int> {
                     co_await delay;
                     co_return 3;
                  }}
   )
                  .Catch([](TestError const& exception) {
                     REQUIRE(std::string{exception.what()} == "race failure");
                  })
                  .Then([](std::optional<std::variant<double, int>> const& value) {
                     return !value.has_value();
                  });

   REQUIRE(raced.Resolved());
   REQUIRE(raced.Value());

   REQUIRE((*resolve)());
   REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
}

TEST_CASE(
  "Resolver Create<true> returns resolve reject handles for external completion",
  "[Promise]"
) {
   std::function<Promise<int, true>(Resolve<int> const&, Reject const&)> resolver_factory =
     [](Resolve<int> const&, Reject const&) -> Promise<int, true> { co_return; };

   auto [promise_ok, resolve_ok, reject_ok] =
     promise::details::Promise<int, true>::template Create<true>(resolver_factory);

   REQUIRE_FALSE(promise_ok.Done());
   REQUIRE((*resolve_ok)(101));
   REQUIRE_FALSE((*reject_ok)(std::make_exception_ptr(TestError{"ignored"})));

   REQUIRE(promise_ok.Resolved());
   REQUIRE(promise_ok.Value() == 101);

   auto [promise_fail, resolve_fail, reject_fail] =
     promise::details::Promise<int, true>::template Create<true>(resolver_factory);

   REQUIRE_FALSE(promise_fail.Done());
   REQUIRE((*reject_fail)(std::make_exception_ptr(TestError{"rpromise reject"})));
   REQUIRE_FALSE((*resolve_fail)(55));

   REQUIRE(promise_fail.Rejected());
   RequireException<TestError>(promise_fail.Exception());
}

TEST_CASE("ToPointer exposes type-erased awaiting", "[Promise]") {
   auto pointer = WPromise{[]() -> Promise<void> { co_return; }}.ToPointer();

   auto waiter = WPromise{[pointer]() -> Promise<int> {
      co_await pointer->VAwait();
      co_return 1;
   }};

   REQUIRE(waiter.Done());

   REQUIRE(waiter.Resolved());
   REQUIRE(waiter.Value() == 1);
}

TEST_CASE("Pending Then registration updates awaiter counters", "[Promise]") {
   auto [source, resolve, reject] = promise::Create<int>();

   REQUIRE(source.Awaiters() == 0);
   REQUIRE(source.UseCount() == 0);

   auto chained = source.Then([](int value) { return value + 1; });

   source.WaitAwaited(0);
   REQUIRE(source.Awaiters() == 1);
   REQUIRE(source.UseCount() >= 1);

   REQUIRE((*resolve)(10));
   REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));

   REQUIRE(chained.Resolved());
   REQUIRE(chained.Value() == 11);
   REQUIRE(source.Awaiters() == 0);
}

TEST_CASE("Pending Catch registration recovers once rejected", "[Promise]") {
   auto [source, resolve, reject] = promise::Create<int>();

   auto recovered = source.Catch([](TestError const& exception) {
      REQUIRE(std::string{exception.what()} == "pending failure");
      return 42;
   });

   source.WaitAwaited(0);
   REQUIRE(source.Awaiters() == 1);

   REQUIRE((*reject)(std::make_exception_ptr(TestError{"pending failure"})));
   REQUIRE_FALSE((*resolve)(7));

   REQUIRE(recovered.Resolved());
   REQUIRE(recovered.Value() == 42);
}
