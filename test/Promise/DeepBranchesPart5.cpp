#include "../TestCommon.h"

using Matrix = test_types::Matrix;

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Catch double with typed std::function async handler",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto                                           source = TestHelper::CreatePending<T, true>();
      std::function<Promise<T>(FinallyError const&)> handler =
        [](FinallyError const& ex) -> Promise<T> {
         REQUIRE(std::string{ex.what()} == "double typed");
         co_return test_types::ValueFromInt<T>(25);
      };

      auto chain = TestHelper::Catch(*source, std::move(handler));
      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(FinallyError{"double typed"})));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == test_types::ValueFromInt<T>(25));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Catch double with exception_ptr std::function async handler",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto                                          source = TestHelper::CreatePending<T, true>();
      std::function<Promise<T>(std::exception_ptr)> handler =
        [](std::exception_ptr ex) -> Promise<T> {
         RequireException<FinallyError>(ex);
         co_return test_types::ValueFromInt<T>(35);
      };

      auto chain = TestHelper::Catch(*source, std::move(handler));
      source->WaitAwaited(0);
      REQUIRE(TestHelper::Reject(*source, std::make_exception_ptr(FinallyError{"double ptr"})));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == test_types::ValueFromInt<T>(35));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper Finally double with std::function covers resolve and reject paths",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      {
         auto                  source     = TestHelper::CreatePending<T, true>();
         bool                  called     = false;
         std::function<void()> finally_fn = [&called] { called = true; };

         auto chain = TestHelper::Finally(*source, std::move(finally_fn));
         source->WaitAwaited(0);
         REQUIRE(TestHelper::Resolve(*source, test_types::ValueFromInt<T>(475)));
         chain.WaitDone();
         REQUIRE(chain.Resolved());
         REQUIRE(chain.Value() == test_types::ValueFromInt<T>(475));
         REQUIRE(called);
      }

      {
         auto                  source     = TestHelper::CreatePending<T, true>();
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

TEMPLATE_LIST_TEST_CASE(
  "TestHelper resolver-less double Catch with typed std::function async handler",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto                                           source = TestHelper::CreatePending<T, false>();
      std::function<Promise<T>(FinallyError const&)> handler =
        [](FinallyError const& ex) -> Promise<T> {
         REQUIRE(std::string{ex.what()} == "double no resolver typed");
         co_return test_types::ValueFromInt<T>(65);
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
      REQUIRE(chain.Value() == test_types::ValueFromInt<T>(65));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper resolver-less double Catch with exception_ptr std::function async handler",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto                                          source = TestHelper::CreatePending<T, false>();
      std::function<Promise<T>(std::exception_ptr)> handler =
        [](std::exception_ptr ex) -> Promise<T> {
         RequireException<FinallyError>(ex);
         co_return test_types::ValueFromInt<T>(75);
      };

      auto chain = TestHelper::Catch(*source, std::move(handler));
      source->WaitAwaited(0);
      REQUIRE(
        TestHelper::Reject(*source, std::make_exception_ptr(FinallyError{"double no resolver ptr"}))
      );
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == test_types::ValueFromInt<T>(75));
   });
}

TEMPLATE_LIST_TEST_CASE(
  "TestHelper resolver-less double Finally std::function paths",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      {
         auto                  source     = TestHelper::CreatePending<T, false>();
         bool                  called     = false;
         std::function<void()> finally_fn = [&called] { called = true; };

         auto chain = TestHelper::Finally(*source, std::move(finally_fn));
         source->WaitAwaited(0);
         REQUIRE(TestHelper::Resolve(*source, test_types::ValueFromInt<T>(875)));
         chain.WaitDone();
         REQUIRE(chain.Resolved());
         REQUIRE(chain.Value() == test_types::ValueFromInt<T>(875));
         REQUIRE(called);
      }

      {
         auto                  source     = TestHelper::CreatePending<T, false>();
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

TEMPLATE_LIST_TEST_CASE(
  "TestHelper resolver-less double Catch passthrough on resolved source",
  "[Promise][DeepBranches]",
  Matrix::PromiseValueTypes
) {
   using T = TestType;
   RunWithTimeout(2s, [&] {
      auto source = TestHelper::CreatePending<T, false>();
      REQUIRE(TestHelper::Resolve(*source, test_types::ValueFromInt<T>(925)));

      bool                                          called = false;
      std::function<Promise<T>(std::exception_ptr)> handler =
        [&called](std::exception_ptr) -> Promise<T> {
         called = true;
         co_return test_types::ValueFromInt<T>(0);
      };

      auto chain = TestHelper::Catch(*source, std::move(handler));
      chain.WaitDone();
      REQUIRE(chain.Resolved());
      REQUIRE(chain.Value() == test_types::ValueFromInt<T>(925));
      REQUIRE_FALSE(called);
   });
}
