#include "../TestCommon.h"

TEST_CASE("Race covers void and optional branches", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto [pending_int, resolve_int, reject_int] = promise::Create<int>();
      auto raced_optional = promise::Race(Promise<void>::Resolve(), pending_int);
      REQUIRE(raced_optional.Resolved());
      REQUIRE_FALSE(raced_optional.Value().has_value());
      REQUIRE((*resolve_int)(15));
      REQUIRE_FALSE((*reject_int)(std::make_exception_ptr(TestError{"ignored"})));

      auto [pending_void, resolve_void, reject_void] = promise::Create<void>();
      auto raced_void = promise::Race(Promise<void>::Resolve(), pending_void);
      REQUIRE(raced_void.Resolved());
      REQUIRE((*resolve_void)());
      REQUIRE_FALSE((*reject_void)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEST_CASE(
  "Catch with promise returning handlers preserves passthrough",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto void_source =
        Promise<void>::Resolve().Catch([](TestError const&) -> Promise<void> { co_return; });
      REQUIRE(void_source.Resolved());

      auto value_source =
        Promise<int>::Resolve(66).Catch([](TestError const&) -> Promise<void> { co_return; });
      REQUIRE(value_source.Resolved());
      REQUIRE(value_source.Value().has_value());
      REQUIRE(value_source.Value().value() == 66);
   });
}

TEST_CASE(
  "Finally with promise returning handlers preserves values and exceptions",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto resolved_value = Promise<int>::Resolve(5).Finally([]() -> Promise<void> { co_return; });
      REQUIRE(resolved_value.Resolved());
      REQUIRE(resolved_value.Value() == 5);

      auto resolved_void = Promise<void>::Resolve().Finally([]() -> Promise<void> { co_return; });
      REQUIRE(resolved_void.Resolved());
   });
}

TEST_CASE("Catch rejected paths produce expected optional shapes", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto rejected_value_to_void =
        Promise<int>::Reject<TestError>("drop value").Catch([](TestError const&) {});
      REQUIRE(rejected_value_to_void.Resolved());
      REQUIRE_FALSE(rejected_value_to_void.Value().has_value());

      auto rejected_void_to_value =
        Promise<void>::Reject<TestError>("supply value").Catch([](TestError const&) {
           return 303;
        });
      REQUIRE(rejected_void_to_value.Resolved());
      REQUIRE(rejected_void_to_value.Value().has_value());
      REQUIRE(rejected_void_to_value.Value().value() == 303);
   });
}

TEST_CASE("WPromise create and v detach type erased paths", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto [created, resolve, reject] = WPromise<int>::Create();
      REQUIRE_FALSE(created.Done());
      REQUIRE((*resolve)(404));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(created.Resolved());
      REQUIRE(created.Value() == 404);

      auto [pending, resolve_pending, reject_pending] = WPromise<int>::Create();
      auto erased = std::move(pending).ToPointer<promise::VPromise>();
      std::move(*erased).VDetach();
      REQUIRE((*resolve_pending)(505));
      REQUIRE_FALSE((*reject_pending)(std::make_exception_ptr(TestError{"ignored"})));
   });
}

TEST_CASE("Member race covers void target and rejection forwarding", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto [void_race, resolve_void_race, reject_void_race] = promise::Create<void>();
      auto source_value                                     = Promise<int>::Resolve(1);
      auto race_void_result =
        source_value.Race(std::move(void_race), resolve_void_race, reject_void_race);
      REQUIRE(race_void_result.Resolved());
      REQUIRE_FALSE((*resolve_void_race)());
      REQUIRE_FALSE((*reject_void_race)(std::make_exception_ptr(TestError{"ignored"})));

      auto [pending_race, resolve_pending_race, reject_pending_race] = promise::Create<int>();
      auto source_rejected = Promise<int>::Reject<TestError>("race reject");
      auto race_rejected_result =
        source_rejected.Race(std::move(pending_race), resolve_pending_race, reject_pending_race);
      REQUIRE(race_rejected_result.Rejected());
      RequireException<TestError>(race_rejected_result.Exception());
      REQUIRE_FALSE((*resolve_pending_race)(66));
   });
}

TEST_CASE("Pending then promise continuation propagates exception", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<int>();
      auto chained                   = source.Then([](int) -> Promise<int> {
         throw CatchError("then async failure");
         co_return 0;
      });
      source.WaitAwaited(0);
      REQUIRE((*resolve)(7));
      REQUIRE_FALSE((*reject)(std::make_exception_ptr(TestError{"ignored"})));
      REQUIRE(chained.Rejected());
      RequireException<CatchError>(chained.Exception());
   });
}

TEST_CASE("Pending then sync continuation keeps rejection from source", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<int>();
      auto chained                   = source.Then([](int value) { return value + 1; });
      source.WaitAwaited(0);
      REQUIRE((*reject)(std::make_exception_ptr(TestError{"pending then reject"})));
      REQUIRE_FALSE((*resolve)(10));
      REQUIRE(chained.Rejected());
      RequireException<TestError>(chained.Exception());
   });
}

TEST_CASE("Pending catch handler throwing turns into rejection", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<int>();
      auto recovered =
        source.Catch([](TestError const&) -> int { throw CatchError("catch sync failure"); });
      source.WaitAwaited(0);
      REQUIRE((*reject)(std::make_exception_ptr(TestError{"trigger"})));
      REQUIRE_FALSE((*resolve)(12));
      REQUIRE(recovered.Rejected());
      RequireException<CatchError>(recovered.Exception());
   });
}

TEST_CASE("Pending catch async handler rejection propagates", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto [source, resolve, reject] = promise::Create<int>();
      auto recovered                 = source.Catch([](TestError const&) -> Promise<int> {
         throw CatchError("catch async failure");
         co_return 0;
      });
      source.WaitAwaited(0);
      REQUIRE((*reject)(std::make_exception_ptr(TestError{"trigger async"})));
      REQUIRE_FALSE((*resolve)(42));
      REQUIRE(recovered.Rejected());
      RequireException<CatchError>(recovered.Exception());
   });
}

TEST_CASE("Internal UnAwait returns false when id is missing", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto pending = TestHelper::CreatePending<int, false>();
      auto id      = TestHelper::RegisterAwaitFunction(*pending, [] {});

      REQUIRE_FALSE(TestHelper::UnregisterAwaitFunction(*pending, id + 1000));
      REQUIRE(TestHelper::UnregisterAwaitFunction(*pending, id));
      REQUIRE(TestHelper::Resolve(*pending, 1));
   });
}

TEST_CASE("Detail promise VAwait allocates erased awaitable", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto resolved_detail = TestHelper::Create<int, false>();

      auto& erased_base      = static_cast<promise::VPromise&>(*resolved_detail);
      auto& erased_awaitable = erased_base.VAwait();
      REQUIRE(erased_awaitable.await_ready());
      REQUIRE_FALSE(erased_awaitable.await_suspend(std::noop_coroutine()));
      REQUIRE_NOTHROW(erased_awaitable.await_resume());
   });
}

TEST_CASE("Internal Unlock guard releases the mutex on scope exit", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto resolved_detail = TestHelper::Create<int, false>();

      REQUIRE(TestHelper::ExerciseUnlock(*resolved_detail));
   });
}

TEST_CASE("Rejected void promise catch resolves through void handler", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto promise   = Promise<void>::Reject<TestError>("rejected void");
      auto recovered = std::move(promise).Catch([](TestError const&) {});

      REQUIRE(recovered.Resolved());
   });
}

TEST_CASE(
  "Finally throwing on resolved value promise rejects with the exception",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto promise = Promise<int>::Resolve(91);
      auto chained = std::move(promise).Finally([] { throw FinallyError{"finally throw branch"}; });

      REQUIRE(chained.Rejected());
      RequireException<FinallyError>(chained.Exception());
   });
}

TEST_CASE(
  "Rejected value promise catch with promise-returning handler resolves recovered value",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto promise = Promise<int>::Reject<TestError>("catch promise branch");
      auto chained =
        std::move(promise).Catch([](TestError const&) -> Promise<int> { co_return 1234; });

      REQUIRE(chained.Resolved());
      REQUIRE(chained.Value() == 1234);
   });
}

TEST_CASE(
  "Rejected void promise finally with promise-returning handler keeps rejection handling alive",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto promise = Promise<void>::Reject<TestError>("finally promise branch");
      auto chained = std::move(promise).Finally([]() -> Promise<void> { co_return; });

      REQUIRE(chained.Rejected());
      RequireException<TestError>(chained.Exception());
   });
}

TEST_CASE(
  "Resolver-backed InitSuspend await_resume throws without resolver",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto raw = TestHelper::CreateWithoutResolver<int, true>();
      REQUIRE_THROWS_AS(TestHelper::InitSuspendAwaitResume(*raw), std::runtime_error);
   });
}

TEST_CASE(
  "Resolver-backed pending Catch with typed async handler resolves value",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      auto chain  = TestHelper::Catch(*source, [](TestError const& ex) -> Promise<int> {
         REQUIRE(std::string{ex.what()} == "resolver typed pending");
         co_return 909;
      });

      source->WaitAwaited(0);
      REQUIRE(
        TestHelper::Reject(*source, std::make_exception_ptr(TestError{"resolver typed pending"}))
      );
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == 909);
   });
}

TEST_CASE(
  "Resolver-backed done Catch with typed async handler resolves value",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      REQUIRE(
        TestHelper::Reject(*source, std::make_exception_ptr(TestError{"resolver typed done"}))
      );

      auto chain = TestHelper::Catch(*source, [](TestError const& ex) -> Promise<int> {
         REQUIRE(std::string{ex.what()} == "resolver typed done");
         co_return 910;
      });

      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == 910);
   });
}

TEST_CASE(
  "Resolver-backed pending Finally promise handler preserves resolved value",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      auto chain  = TestHelper::Finally(*source, []() -> Promise<void> { co_return; });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source, 717));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == 717);
   });
}

TEST_CASE(
  "Resolver-backed pending Finally promise handler preserves rejection",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      auto chain  = TestHelper::Finally(*source, []() -> Promise<void> { co_return; });

      source->WaitAwaited(0);
      REQUIRE(
        TestHelper::Reject(*source, std::make_exception_ptr(TestError{"resolver final reject"}))
      );
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<TestError>(chain.Exception());
   });
}

TEST_CASE(
  "Resolver-backed pending void Catch with void handler resolves",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<void, true>();
      auto chain  = TestHelper::Catch(*source, [](TestError const&) {});

      source->WaitAwaited(0);
      REQUIRE(
        TestHelper::Reject(*source, std::make_exception_ptr(TestError{"pending void catch"}))
      );
      chain.WaitDone();
      REQUIRE(chain.Resolved());
   });
}

TEST_CASE("UnAwait ignores coroutine-handle awaiters", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto pending = TestHelper::CreatePending<int, true>();

      REQUIRE(TestHelper::await_suspend(*pending, std::noop_coroutine()));
      REQUIRE(TestHelper::Awaiters(*pending) == 1);
      REQUIRE_FALSE(TestHelper::UnregisterAwaitFunction(*pending, 1));
      REQUIRE(TestHelper::Awaiters(*pending) == 1);
      REQUIRE(TestHelper::Resolve(*pending, 9));
   });
}

TEST_CASE("TestHelper Then pending void resolves into value", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<void, true>();
      auto chain  = TestHelper::Then(*source, [] { return 11; });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == 11);
   });
}

TEST_CASE("TestHelper Then pending void keeps rejection", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<void, true>();
      auto chain  = TestHelper::Then(*source, [] { return 12; });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"then void reject"})));
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<TestError>(chain.Exception());
   });
}

TEST_CASE("TestHelper Then done rejected short-circuits handler", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"done reject"})));

      bool called = false;
      auto chain  = TestHelper::Then(*source, [&called](int) {
         called = true;
         return 1;
      });

      chain.WaitDone();
      REQUIRE(chain.Rejected());
      REQUIRE_FALSE(called);
      RequireException<TestError>(chain.Exception());
   });
}

TEST_CASE("TestHelper Then pending value to void handler resolves", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      bool called = false;
      int  seen   = 0;
      auto chain  = TestHelper::Then(*source, [&called, &seen](int value) {
         called = true;
         seen   = value;
      });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source, 44));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(called);
      REQUIRE(seen == 44);
   });
}

TEST_CASE(
  "TestHelper Catch pending void with exception_ptr async handler resolves",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<void, true>();
      auto chain  = TestHelper::Catch(*source, [](std::exception_ptr const& ex) -> Promise<void> {
         RequireException<TestError>(ex);
         co_return;
      });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"catch ptr async"})));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
   });
}

TEST_CASE("TestHelper Catch typed mismatch keeps original rejection", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      auto chain  = TestHelper::Catch(*source, [](CatchError const&) { return 5; });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"typed mismatch"})));
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<TestError>(chain.Exception());
   });
}

TEST_CASE("TestHelper Catch on resolved value does passthrough", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      REQUIRE(TestHelper::Resolve(*source, 55));

      bool called = false;
      auto chain  = TestHelper::Catch(*source, [&called](TestError const&) {
         called = true;
         return 0;
      });

      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == 55);
      REQUIRE_FALSE(called);
   });
}

TEST_CASE(
  "TestHelper Finally pending resolved value throwing handler rejects",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      auto chain =
        TestHelper::Finally(*source, [] { throw FinallyError{"pending finally throw"}; });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source, 88));
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<FinallyError>(chain.Exception());
   });
}

TEST_CASE(
  "TestHelper Finally pending rejected throwing handler overrides rejection",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      auto chain =
        TestHelper::Finally(*source, [] { throw FinallyError{"override final reject"}; });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"initial reject"})));
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<FinallyError>(chain.Exception());
   });
}

TEST_CASE(
  "TestHelper Finally pending void resolved sync handler resolves",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<void, true>();
      bool called = false;
      auto chain  = TestHelper::Finally(*source, [&called] { called = true; });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(called);
   });
}

TEST_CASE("TestHelper mixed awaiters increase use count", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto pending = TestHelper::CreatePending<int, true>();

      auto id = TestHelper::RegisterAwaitFunction(*pending, [] {});
      REQUIRE(id == 1);
      REQUIRE(TestHelper::await_suspend(*pending, std::noop_coroutine()));
      REQUIRE(TestHelper::UseCount(*pending) == 2);
      REQUIRE(TestHelper::Awaiters(*pending) == 2);
      REQUIRE(TestHelper::Resolve(*pending, 2));
   });
}

TEST_CASE(
  "TestHelper Finally pending value with rejecting async handler rejects",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<int, true>();
      auto chain  = TestHelper::Finally(*source, []() -> Promise<void> {
         throw FinalThenError{"finally async reject value"};
         co_return;
      });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source, 13));
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<FinalThenError>(chain.Exception());
   });
}

TEST_CASE(
  "TestHelper Finally pending void with rejecting async handler rejects",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<void, true>();
      auto chain  = TestHelper::Finally(*source, []() -> Promise<void> {
         throw FinalThenError{"finally async reject void"};
         co_return;
      });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source));
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<FinalThenError>(chain.Exception());
   });
}

TEST_CASE(
  "Handle PromiseType GetParent is wired for coroutine-backed promises",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto detail = TestHelper::CreatePending<int, true>();

      REQUIRE(TestHelper::GetParentFromPromiseType(*detail) == nullptr);
   });
}

TEST_CASE(
  "Handle InitSuspend await_ready and await_suspend are callable",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto detail = TestHelper::CreatePending<void, true>();

      REQUIRE_FALSE(TestHelper::InitSuspendAwaitReady(*detail));
      REQUIRE_NOTHROW(TestHelper::InitSuspendAwaitSuspend(*detail, std::noop_coroutine()));
   });
}

TEST_CASE("TestHelper lock_guard await registration paths execute", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto pending = TestHelper::CreatePending<double, true>();
      bool called  = false;

      auto id =
        TestHelper::RegisterAwaitFunctionWithLockGuard(*pending, [&called] { called = true; });
      REQUIRE(id == 1);
      TestHelper::AwaitSuspendWithLockGuard(*pending, std::noop_coroutine());
      REQUIRE(TestHelper::Awaiters(*pending) == 2);
      REQUIRE_FALSE(TestHelper::UnregisterAwaitFunctionWithLockGuard(*pending, id + 99));
      REQUIRE(TestHelper::UnregisterAwaitFunctionWithLockGuard(*pending, id));
      REQUIRE(TestHelper::Resolve(*pending, 1.25));
      REQUIRE_FALSE(called);
   });
}

TEST_CASE(
  "TestHelper Catch double with typed std::function async handler",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<double, true>();
      std::function<Promise<double>(FinallyError const&)> handler =
        [](FinallyError const& ex) -> Promise<double> {
         REQUIRE(std::string{ex.what()} == "double typed");
         co_return 2.5;
      };

      auto chain = TestHelper::Catch(*source, std::move(handler));
      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(FinallyError{"double typed"})));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == 2.5);
   });
}

TEST_CASE(
  "TestHelper Catch double with exception_ptr std::function async handler",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<double, true>();
      std::function<Promise<double>(std::exception_ptr)> handler =
        [](std::exception_ptr ex) -> Promise<double> {
         RequireException<FinallyError>(ex);
         co_return 3.5;
      };

      auto chain = TestHelper::Catch(*source, std::move(handler));
      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(FinallyError{"double ptr"})));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == 3.5);
   });
}

TEST_CASE(
  "TestHelper Finally double with std::function covers resolve and reject paths",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      {
         auto                  source     = TestHelper::CreatePending<double, true>();
         bool                  called     = false;
         std::function<void()> finally_fn = [&called] { called = true; };

         auto chain = TestHelper::Finally(*source, std::move(finally_fn));
         source->WaitAwaited(0);
         REQUIRE(TestHelper::Resolve(*source, 4.75));
         chain.WaitDone();
         REQUIRE(chain.Resolved());
         REQUIRE(chain.Value() == 4.75);
         REQUIRE(called);
      }

      {
         auto                  source     = TestHelper::CreatePending<double, true>();
         std::function<void()> finally_fn = [] {};

         auto chain = TestHelper::Finally(*source, std::move(finally_fn));
         source->WaitAwaited(0);
         REQUIRE(
           TestHelper::Reject(*source, std::make_exception_ptr(FinallyError{"double final reject"}))
         );
         chain.WaitDone();
         REQUIRE(chain.Rejected());
         RequireException<FinallyError>(chain.Exception());
      }
   });
}

TEST_CASE(
  "TestHelper resolver-less double Catch with typed std::function async handler",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<double, false>();
      std::function<Promise<double>(FinallyError const&)> handler =
        [](FinallyError const& ex) -> Promise<double> {
         REQUIRE(std::string{ex.what()} == "double no resolver typed");
         co_return 6.5;
      };

      auto chain = TestHelper::Catch(*source, std::move(handler));
      source->WaitAwaited(0);
      REQUIRE(
        TestHelper::Reject(
          *source, std::make_exception_ptr(FinallyError{"double no resolver typed"})
        )
      );
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == 6.5);
   });
}

TEST_CASE(
  "TestHelper resolver-less double Catch with exception_ptr std::function async handler",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<double, false>();
      std::function<Promise<double>(std::exception_ptr)> handler =
        [](std::exception_ptr ex) -> Promise<double> {
         RequireException<FinallyError>(ex);
         co_return 7.5;
      };

      auto chain = TestHelper::Catch(*source, std::move(handler));
      source->WaitAwaited(0);
      REQUIRE(
        TestHelper::Reject(*source, std::make_exception_ptr(FinallyError{"double no resolver ptr"}))
      );
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == 7.5);
   });
}

TEST_CASE(
  "TestHelper resolver-less double Finally std::function paths",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      {
         auto                  source     = TestHelper::CreatePending<double, false>();
         bool                  called     = false;
         std::function<void()> finally_fn = [&called] { called = true; };

         auto chain = TestHelper::Finally(*source, std::move(finally_fn));
         source->WaitAwaited(0);
         REQUIRE(TestHelper::Resolve(*source, 8.75));
         chain.WaitDone();
         REQUIRE(chain.Resolved());
         REQUIRE(chain.Value() == 8.75);
         REQUIRE(called);
      }

      {
         auto                  source     = TestHelper::CreatePending<double, false>();
         std::function<void()> finally_fn = [] {};

         auto chain = TestHelper::Finally(*source, std::move(finally_fn));
         source->WaitAwaited(0);
         REQUIRE(
           TestHelper::Reject(
             *source, std::make_exception_ptr(FinallyError{"double no resolver final reject"})
           )
         );
         chain.WaitDone();
         REQUIRE(chain.Rejected());
         RequireException<FinallyError>(chain.Exception());
      }
   });
}

TEST_CASE(
  "TestHelper resolver-less double Catch passthrough on resolved source",
  "[Promise][DeepBranches]"
) {
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<double, false>();
      REQUIRE(TestHelper::Resolve(*source, 9.25));

      bool                                               called = false;
      std::function<Promise<double>(std::exception_ptr)> handler =
        [&called](std::exception_ptr) -> Promise<double> {
         called = true;
         co_return 0.0;
      };

      auto chain = TestHelper::Catch(*source, std::move(handler));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == 9.25);
      REQUIRE_FALSE(called);
   });
}
