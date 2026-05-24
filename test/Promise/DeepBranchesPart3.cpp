#include "../TestCommon.h"

using Matrix = test_types::Matrix;

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

TEMPLATE_LIST_TEST_CASE(
  "UnAwait ignores coroutine-handle awaiters",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto pending = TestHelper::CreatePending<T, true>();

      REQUIRE(TestHelper::await_suspend(*pending, std::noop_coroutine()));
      REQUIRE(TestHelper::Awaiters(*pending) == 1);
      REQUIRE_FALSE(TestHelper::UnregisterAwaitFunction(*pending, 1));
      REQUIRE(TestHelper::Awaiters(*pending) == 1);
      REQUIRE(TestHelper::Resolve(*pending, test_types::ValueFromInt<T>(9)));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Then pending void resolves into value",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<void, true>();
      auto chain  = TestHelper::Then(*source, [] { return test_types::ValueFromInt<T>(11); });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == test_types::ValueFromInt<T>(11));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Then pending void keeps rejection",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<void, true>();
      auto chain  = TestHelper::Then(*source, [] { return test_types::ValueFromInt<T>(12); });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"then void reject"})));
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<TestError>(chain.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Then done rejected short-circuits handler",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"done reject"})));

      bool called = false;
      auto chain  = TestHelper::Then(*source, [&called](T const&) {
         called = true;
         return test_types::ValueFromInt<T>(1);
      });

      chain.WaitDone();
      REQUIRE(chain.Rejected());
      REQUIRE_FALSE(called);
      RequireException<TestError>(chain.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Then pending value to void handler resolves",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
      bool called = false;
      T    seen{};
      auto chain = TestHelper::Then(*source, [&called, &seen](T const& value) {
         called = true;
         seen   = value;
      });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source, test_types::ValueFromInt<T>(44)));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(called);
      REQUIRE(seen == test_types::ValueFromInt<T>(44));
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

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Catch typed mismatch keeps original rejection",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
      auto chain  = TestHelper::Catch(*source, [](CatchError const&) {
         return test_types::ValueFromInt<T>(5);
      });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"typed mismatch"})));
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<TestError>(chain.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Catch on resolved value does passthrough",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
      REQUIRE(TestHelper::Resolve(*source, test_types::ValueFromInt<T>(55)));

      bool called = false;
      auto chain  = TestHelper::Catch(*source, [&called](TestError const&) {
         called = true;
         return test_types::ValueFromInt<T>(0);
      });

      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == test_types::ValueFromInt<T>(55));
      REQUIRE_FALSE(called);
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Finally pending resolved value throwing handler rejects",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
      auto chain =
        TestHelper::Finally(*source, [] { throw FinallyError{"pending finally throw"}; });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Resolve(*source, test_types::ValueFromInt<T>(88)));
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<FinallyError>(chain.Exception());
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Finally pending rejected throwing handler overrides rejection",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, true>();
      auto chain =
        TestHelper::Finally(*source, [] { throw FinallyError{"override final reject"}; });

      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(TestError{"initial reject"})));
      chain.WaitDone();
      REQUIRE(chain.Rejected());
      RequireException<FinallyError>(chain.Exception());
   });
}
