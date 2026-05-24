#include "../TestCommon.h"

using Matrix = test_types::Matrix;

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

TEMPLATE_LIST_TEST_CASE(
  "TestHelper mixed awaiters increase use count",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto pending = TestHelper::CreatePending<T, true>();

      auto id = TestHelper::RegisterAwaitFunction(*pending, [] {});
      REQUIRE(id == 1);
      REQUIRE(TestHelper::await_suspend(*pending, std::noop_coroutine()));
      REQUIRE(TestHelper::UseCount(*pending) == 2);
      REQUIRE(TestHelper::Awaiters(*pending) == 2);
      REQUIRE(TestHelper::Resolve(*pending, test_types::ValueFromInt<T>(2)));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Finally pending value with rejecting async handler rejects",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
      auto chain  = TestHelper::Finally(*source, []() -> Promise<void> {
         throw FinalThenError{"finally async reject value"};
         co_return;
      });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source, test_types::ValueFromInt<T>(13)));
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

TEMPLATE_LIST_TEST_CASE(
  "Handle PromiseType GetParent is wired for coroutine-backed promises",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto detail = TestHelper::CreatePending<T, true>();

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

TEMPLATE_LIST_TEST_CASE(
  "TestHelper lock_guard await registration paths execute",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto pending = TestHelper::CreatePending<T, true>();
      bool called  = false;

      auto id =
        TestHelper::RegisterAwaitFunctionWithLockGuard(*pending, [&called] { called = true; });
      REQUIRE(id == 1);
      TestHelper::AwaitSuspendWithLockGuard(*pending, std::noop_coroutine());
      REQUIRE(TestHelper::Awaiters(*pending) == 2);
      REQUIRE_FALSE(TestHelper::UnregisterAwaitFunctionWithLockGuard(*pending, id + 99));
      REQUIRE(TestHelper::UnregisterAwaitFunctionWithLockGuard(*pending, id));
      REQUIRE(TestHelper::Resolve(*pending, test_types::ValueFromInt<T>(125)));
      REQUIRE_FALSE(called);
   });
}
