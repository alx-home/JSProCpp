#include "../TestCommon.h"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "Internal UnAwait returns false when id is missing",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto pending = TestHelper::CreatePending<T, false>();
      auto id      = TestHelper::RegisterAwaitFunction(*pending, [] {});

      REQUIRE_FALSE(TestHelper::UnregisterAwaitFunction(*pending, id + 1000));
      REQUIRE(TestHelper::UnregisterAwaitFunction(*pending, id));
      REQUIRE(TestHelper::Resolve(*pending, test_types::ValueFromInt<T>(1)));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Detail promise VAwait allocates erased awaitable",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto resolved_detail = TestHelper::Create<T, false>();

      auto& erased_base      = static_cast<promise::VPromise&>(*resolved_detail);
      auto& erased_awaitable = erased_base.VAwait();
      REQUIRE(erased_awaitable.await_ready());
      REQUIRE_FALSE(erased_awaitable.await_suspend(std::noop_coroutine()));
      REQUIRE_NOTHROW(erased_awaitable.await_resume());
   });
}

TEST_CASE("Rejected void promise catch resolves through void handler", "[Promise][DeepBranches]") {
   RunWithTimeout(2s, [&] {
      auto promise   = Promise<void>::Reject<TestError>("rejected void");
      auto recovered = std::move(promise).Catch([](TestError const&) {});

      REQUIRE(recovered.Resolved());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Finally throwing on resolved value promise rejects with the exception",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto promise = Promise<T>::Resolve(test_types::ValueFromInt<T>(91));
      auto chained = std::move(promise).Finally([] { throw FinallyError{"finally throw branch"}; });

      REQUIRE(chained.Rejected());
      RequireException<FinallyError>(chained.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Rejected value promise catch with promise-returning handler resolves recovered value",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto promise = Promise<T>::template Reject<TestError>("catch promise branch");
      auto chained = std::move(promise).Catch([](TestError const&) -> Promise<T> {
         co_return test_types::ValueFromInt<T>(1234);
      });

      REQUIRE(chained.Resolved());
      REQUIRE(chained.Value() == test_types::ValueFromInt<T>(1234));
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

TEMPLATE_LIST_TEST_CASE(
  "Resolver-backed InitSuspend await_resume throws without resolver",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto raw = TestHelper::CreateWithoutResolver<T, true>();
      REQUIRE_THROWS_AS(TestHelper::InitSuspendAwaitResume(*raw), std::runtime_error);
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Resolver-backed pending Catch with typed async handler resolves value",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
      auto chain  = TestHelper::Catch(*source, [](TestError const& ex) -> Promise<T> {
         REQUIRE(std::string{ex.what()} == "resolver typed pending");
         co_return test_types::ValueFromInt<T>(909);
      });

      source->WaitAwaited(0);
      REQUIRE(
        TestHelper::Reject(*source, std::make_exception_ptr(TestError{"resolver typed pending"}))
      );
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == test_types::ValueFromInt<T>(909));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Resolver-backed done Catch with typed async handler resolves value",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
      REQUIRE(
        TestHelper::Reject(*source, std::make_exception_ptr(TestError{"resolver typed done"}))
      );

      auto chain = TestHelper::Catch(*source, [](TestError const& ex) -> Promise<T> {
         REQUIRE(std::string{ex.what()} == "resolver typed done");
         co_return test_types::ValueFromInt<T>(910);
      });

      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == test_types::ValueFromInt<T>(910));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Resolver-backed pending Finally promise handler preserves resolved value",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
      auto chain  = TestHelper::Finally(*source, []() -> Promise<void> { co_return; });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source, test_types::ValueFromInt<T>(717)));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == test_types::ValueFromInt<T>(717));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "Resolver-backed pending Finally promise handler preserves rejection",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
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
